// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "extensions/buildflags/buildflags.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(USE_BLINK)
#include "third_party/blink/public/common/features_generated.h"
#endif

namespace content_settings {

CookieSettings::CookieSettings(
    HostContentSettingsMap* host_content_settings_map,
    PrefService* prefs,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    bool is_incognito,
    const char* extension_scheme)
    : tracking_protection_settings_(tracking_protection_settings),
      host_content_settings_map_(host_content_settings_map),
      is_incognito_(is_incognito),
      extension_scheme_(extension_scheme),
      block_third_party_cookies_(
          net::cookie_util::IsForceThirdPartyCookieBlockingEnabled()) {
  content_settings_observation_.Observe(host_content_settings_map_.get());
  if (tracking_protection_settings_) {
    tracking_protection_settings_observation_.Observe(
        tracking_protection_settings_.get());
    tracking_protection_enabled_for_3pcd_ =
        tracking_protection_settings_->IsTrackingProtection3pcdEnabled();
  }
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::BindRepeating(&CookieSettings::OnCookiePreferencesChanged,
                          base::Unretained(this)));
  OnCookiePreferencesChanged();
  OnBlockAllThirdPartyCookiesChanged();
}

ContentSetting CookieSettings::GetDefaultCookieSetting(
    std::string* provider_id) const {
  return host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, provider_id);
}

ContentSettingsForOneType CookieSettings::GetCookieSettings() const {
  return host_content_settings_map_->GetSettingsForOneType(
      ContentSettingsType::COOKIES);
}

void CookieSettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kCookieControlsMode,
      static_cast<int>(CookieControlsMode::kIncognitoOnly),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void CookieSettings::SetDefaultCookieSetting(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, setting);
}

void CookieSettings::SetCookieSetting(const GURL& primary_url,
                                      ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetContentSettingDefaultScope(
      primary_url, GURL(), ContentSettingsType::COOKIES, setting);
}

bool CookieSettings::IsAllowedByTpcdMetadataGrant(
    const GURL& url,
    const GURL& first_party_url) const {
  if (!ShouldConsider3pcdMetadataGrantsSettings(
          net::CookieSettingOverrides())) {
    return false;
  }

  base::AutoLock lock(tpcd_lock_);
  const auto& entry = base::ranges::find_if(
      settings_for_3pcd_metadata_grants_,
      [&](const ContentSettingPatternSource& entry) {
        CHECK(IsAllowed(
            content_settings::ValueToContentSetting(entry.setting_value)));
        return entry.primary_pattern.Matches(url) &&
               entry.secondary_pattern.Matches(first_party_url);
      });
  return entry != settings_for_3pcd_metadata_grants_.end();
}

void CookieSettings::SetTemporaryCookieGrantForHeuristic(
    const GURL& url,
    const GURL& first_party_url,
    base::TimeDelta ttl,
    bool use_schemeless_patterns) {
  if (url.is_empty() || first_party_url.is_empty()) {
    return;
  }

  // If the new grant has an earlier TTL than the existing setting, keep the
  // existing TTL.
  SettingInfo info;
  ContentSetting current_setting =
      host_content_settings_map_->GetContentSetting(
          url, first_party_url, ContentSettingsType::TPCD_HEURISTICS_GRANTS,
          &info);
  if (IsAllowed(current_setting) && !info.metadata.expiration().is_null() &&
      info.metadata.expiration() > base::Time::Now() + ttl) {
    return;
  }

  ContentSettingConstraints constraints;
  constraints.set_lifetime(ttl);

  if (use_schemeless_patterns) {
    ContentSettingsPattern url_pattern =
        ContentSettingsPattern::ToHostOnlyPattern(
            ContentSettingsPattern::FromURLToSchemefulSitePattern(url));
    ContentSettingsPattern first_party_url_pattern =
        ContentSettingsPattern::ToHostOnlyPattern(
            ContentSettingsPattern::FromURLToSchemefulSitePattern(
                first_party_url));

    host_content_settings_map_->SetContentSettingCustomScope(
        url_pattern, first_party_url_pattern,
        ContentSettingsType::TPCD_HEURISTICS_GRANTS, CONTENT_SETTING_ALLOW,
        constraints);
  } else {
    host_content_settings_map_->SetContentSettingDefaultScope(
        url, first_party_url, ContentSettingsType::TPCD_HEURISTICS_GRANTS,
        CONTENT_SETTING_ALLOW, constraints);
  }
}

void CookieSettings::SetCookieSettingForUserBypass(
    const GURL& first_party_url) {
  ContentSettingConstraints constraints;

  // Only apply a lifetime outside incognito. In incognito, the duration is
  // inherintly limited.
  if (!is_incognito_) {
    constraints.set_lifetime(
        content_settings::features::kUserBypassUIExceptionExpiration.Get());
  }

  constraints.set_session_model(SessionModel::Durable);

  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url),
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_ALLOW,
      constraints);
}

bool CookieSettings::IsStoragePartitioningBypassEnabled(
    const GURL& first_party_url) {
  SettingInfo info;
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      GURL(), first_party_url, ContentSettingsType::COOKIES, &info);

  bool is_default = info.primary_pattern.MatchesAllHosts() &&
                    info.secondary_pattern.MatchesAllHosts();

  return is_default ? false : IsAllowed(setting);
}

void CookieSettings::ResetCookieSetting(const GURL& primary_url) {
  host_content_settings_map_->SetNarrowestContentSetting(
      primary_url, GURL(), ContentSettingsType::COOKIES,
      CONTENT_SETTING_DEFAULT);
}

// TODO(crbug.com/1386190): Update to take in CookieSettingOverrides.
bool CookieSettings::IsThirdPartyAccessAllowed(
    const GURL& first_party_url,
    content_settings::SettingInfo* info) const {
  // Use GURL() as an opaque primary url to check if any site
  // could access cookies in a 3p context on |first_party_url|.
  return IsAllowed(GetCookieSetting(GURL(), first_party_url,
                                    net::CookieSettingOverrides(), info));
}

void CookieSettings::SetThirdPartyCookieSetting(const GURL& first_party_url,
                                                ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLNoWildcard(first_party_url),
      ContentSettingsType::COOKIES, setting);
}

void CookieSettings::ResetThirdPartyCookieSetting(const GURL& first_party_url) {
  // Standard third party cookie settings are, with the introduction of User
  // Bypass, site scoped. There also may be an origin scoped exception either
  // created manually, or through the previous UI. Resetting should support
  // both of these.

  // TODO(crbug.com/1446230): Log metrics when there is pattern that has domain
  // as wildcard.
  auto pattern =
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url);

  SettingInfo info;
  host_content_settings_map_->GetContentSetting(
      GURL(), first_party_url, ContentSettingsType::COOKIES, &info);
  if (!info.secondary_pattern.HasDomainWildcard()) {
    pattern = info.secondary_pattern;
  }
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), pattern, ContentSettingsType::COOKIES,
      CONTENT_SETTING_DEFAULT);
}

bool CookieSettings::IsStorageDurable(const GURL& origin) const {
  // TODO(dgrogan): Don't use host_content_settings_map_ directly.
  // https://crbug.com/539538
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      origin /*primary*/, origin /*secondary*/,
      ContentSettingsType::DURABLE_STORAGE);
  return setting == CONTENT_SETTING_ALLOW;
}

bool CookieSettings::HasAnyFrameRequestedStorageAccess(
    const GURL& first_party_url) const {
  ContentSettingsForOneType settings =
      host_content_settings_map_->GetSettingsForOneType(
          ContentSettingsType::STORAGE_ACCESS);
  for (ContentSettingPatternSource source : settings) {
    // Skip default exceptions.
    if (source.primary_pattern.MatchesAllHosts() ||
        source.secondary_pattern.MatchesAllHosts()) {
      continue;
    }
    // Skip exceptions that doesn't match the secondary pattern.
    if (!source.secondary_pattern.Matches(first_party_url)) {
      continue;
    }

    // There is an active SAA exception created in the context of
    // |first_party_url|.
    return true;
  }

  return false;
}

bool CookieSettings::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  return site_for_cookies.RepresentativeUrl().SchemeIs(kChromeUIScheme) &&
         url.SchemeIsCryptographic();
}

void CookieSettings::ShutdownOnUIThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  tracking_protection_settings_ = nullptr;
  tracking_protection_settings_observation_.Reset();
  pref_change_registrar_.RemoveAll();
}

// Returns whether third-party cookie blocking should be bypassed (i.e. always
// allow the cookie regardless of cookie content settings and third-party
// cookie blocking settings.
// This just checks the scheme of the |url| and |site_for_cookies|:
//  - Allow cookies if the |site_for_cookies| is a chrome:// scheme URL, and
//    the |url| has a secure scheme.
//  - Allow cookies if the |site_for_cookies| and the |url| match in scheme
//    and both have the Chrome extensions scheme.
bool CookieSettings::ShouldAlwaysAllowCookies(
    const GURL& url,
    const GURL& first_party_url) const {
  if (first_party_url.SchemeIs(kChromeUIScheme) &&
      url.SchemeIsCryptographic()) {
    return true;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (url.SchemeIs(extension_scheme_) &&
      first_party_url.SchemeIs(extension_scheme_)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return false;
}

ContentSetting CookieSettings::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    content_settings::SettingInfo* info) const {
  if (content_type == ContentSettingsType::TPCD_METADATA_GRANTS) {
    return IsAllowedByTpcdMetadataGrant(primary_url, secondary_url)
               ? CONTENT_SETTING_ALLOW
               : CONTENT_SETTING_BLOCK;
  }

  return host_content_settings_map_->GetContentSetting(
      primary_url, secondary_url, content_type, info);
}

bool CookieSettings::IsThirdPartyCookiesAllowedScheme(
    const std::string& scheme) const {
  return scheme == extension_scheme_;
}

bool CookieSettings::IsStorageAccessApiEnabled() const {
  // TODO(https://crbug.com/1411765): instead of using a BUILDFLAG and checking
  // the feature here, we should rely on CookieSettingsFactory to plumb in this
  // boolean instead.
#if BUILDFLAG(USE_BLINK)
  return base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI) ||
         base::FeatureList::IsEnabled(
             permissions::features::kPermissionStorageAccessAPI);
#else
  return false;
#endif
}

CookieSettings::~CookieSettings() = default;

bool CookieSettings::ShouldBlockThirdPartyCookiesInternal() {
  DCHECK(thread_checker_.CalledOnValidThread());

#if BUILDFLAG(IS_IOS)
  if (!base::FeatureList::IsEnabled(kImprovedCookieControls)) {
    return false;
  }
#endif

  if (net::cookie_util::IsForceThirdPartyCookieBlockingEnabled()) {
    return true;
  }

  // Cookies should always be blocked in 3PCD experiment.
  if (tracking_protection_settings_ &&
      tracking_protection_settings_->IsTrackingProtection3pcdEnabled()) {
    return true;
  }

  CookieControlsMode mode = static_cast<CookieControlsMode>(
      pref_change_registrar_.prefs()->GetInteger(prefs::kCookieControlsMode));

  switch (mode) {
    case CookieControlsMode::kBlockThirdParty:
      return true;
    case CookieControlsMode::kIncognitoOnly:
      return is_incognito_;
    case CookieControlsMode::kOff:
      return false;
  }
  return false;
}

bool CookieSettings::MitigationsEnabledFor3pcdInternal() {
  // Mitigations won't be enabled when Third Party Cookies Blocking is enabled
  // by `features::kForceThirdPartyCookieBlocking` which is intended to be used
  // via command-lines by developers for testing.
  if (net::cookie_util::IsForceThirdPartyCookieBlockingEnabled()) {
    return false;
  }

  if (tracking_protection_settings_ &&
      tracking_protection_settings_->IsTrackingProtection3pcdEnabled()) {
    // Mitigations should be on iff we are not blocking all 3PC in 3PCD.
    return !tracking_protection_settings_->AreAllThirdPartyCookiesBlocked();
  }

  return false;
}

void CookieSettings::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(ContentSettingsType::COOKIES)) {
    for (auto& observer : observers_) {
      observer.OnCookieSettingChanged();
    }
  }
}

void CookieSettings::OnBlockAllThirdPartyCookiesChanged() {
  bool new_mitigations_enabled_for_3pcd = MitigationsEnabledFor3pcdInternal();
  {
    base::AutoLock auto_lock(lock_);
    if (mitigations_enabled_for_3pcd_ == new_mitigations_enabled_for_3pcd) {
      return;
    }
    mitigations_enabled_for_3pcd_ = new_mitigations_enabled_for_3pcd;
  }

  for (Observer& obs : observers_) {
    obs.OnMitigationsEnabledFor3pcdChanged(new_mitigations_enabled_for_3pcd);
  }
}

void CookieSettings::OnTrackingProtection3pcdChanged() {
  bool new_tracking_protection_enabled_for_3pcd =
      tracking_protection_settings_->IsTrackingProtection3pcdEnabled();
  {
    base::AutoLock auto_lock(lock_);
    if (tracking_protection_enabled_for_3pcd_ ==
        new_tracking_protection_enabled_for_3pcd) {
      return;
    }
    tracking_protection_enabled_for_3pcd_ =
        new_tracking_protection_enabled_for_3pcd;
  }
  for (Observer& obs : observers_) {
    obs.OnTrackingProtectionEnabledFor3pcdChanged(
        new_tracking_protection_enabled_for_3pcd);
  }
  // If the user opted to block all 3PC while in the experiment, preserve that
  // preference if they are offboarded.
  if (!new_tracking_protection_enabled_for_3pcd &&
      pref_change_registrar_.prefs()->GetBoolean(
          prefs::kBlockAll3pcToggleEnabled)) {
    pref_change_registrar_.prefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(CookieControlsMode::kBlockThirdParty));
  }
  OnCookiePreferencesChanged();
}

void CookieSettings::OnCookiePreferencesChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool new_block_third_party_cookies = ShouldBlockThirdPartyCookiesInternal();

  {
    base::AutoLock auto_lock(lock_);
    if (block_third_party_cookies_ == new_block_third_party_cookies) {
      return;
    }
    block_third_party_cookies_ = new_block_third_party_cookies;
  }
  for (Observer& obs : observers_) {
    obs.OnThirdPartyCookieBlockingChanged(new_block_third_party_cookies);
  }
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  base::AutoLock auto_lock(lock_);
  return block_third_party_cookies_;
}

bool CookieSettings::MitigationsEnabledFor3pcd() const {
  base::AutoLock auto_lock(lock_);
  return mitigations_enabled_for_3pcd_;
}

bool CookieSettings::TrackingProtectionEnabledFor3pcd() const {
  base::AutoLock auto_lock(lock_);
  return tracking_protection_enabled_for_3pcd_;
}

}  // namespace content_settings
