// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_H_

#include <string>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"

class GURL;
class PrefService;

namespace net {
class SiteForCookies;
}  // namespace net

namespace content_settings {

// This enum is used in prefs, do not change values.
// The enum needs to correspond to CookieControlsMode in enums.xml.
// This enum needs to be kept in sync with the enum of the same name in
// browser/resources/settings/site_settings/constants.js.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum class CookieControlsMode {
  kOff = 0,
  kBlockThirdParty = 1,
  kIncognitoOnly = 2,
  kMaxValue = kIncognitoOnly,
};

// Default value for |extension_scheme|.
const char kDummyExtensionScheme[] = ":no-extension-scheme:";

// A frontend to the cookie settings of |HostContentSettingsMap|. Handles
// cookie-specific logic such as blocking third-party cookies. Written on the UI
// thread and read on any thread.
class CookieSettings
    : public CookieSettingsBase,
      public content_settings::Observer,
      public privacy_sandbox::TrackingProtectionSettingsObserver,
      public RefcountedKeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnThirdPartyCookieBlockingChanged(
        bool block_third_party_cookies) {}
    virtual void OnMitigationsEnabledFor3pcdChanged(bool enable) {}
    virtual void OnTrackingProtectionEnabledFor3pcdChanged(bool enable) {}
    virtual void OnCookieSettingChanged() {}
  };

  // Creates a new CookieSettings instance.
  // The caller is responsible for ensuring that |extension_scheme| is valid for
  // the whole lifetime of this instance.
  // |is_incognito| indicates whether this is an incognito profile. It is not
  // true for other types of off-the-record profiles like guest mode.
  CookieSettings(
      HostContentSettingsMap* host_content_settings_map,
      PrefService* prefs,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      bool is_incognito,
      const char* extension_scheme = kDummyExtensionScheme);

  CookieSettings(const CookieSettings&) = delete;
  CookieSettings& operator=(const CookieSettings&) = delete;

  // Returns the default content setting (CONTENT_SETTING_ALLOW,
  // CONTENT_SETTING_BLOCK, or CONTENT_SETTING_SESSION_ONLY) for cookies. If
  // |provider_id| is not nullptr, the id of the provider which provided the
  // default setting is assigned to it.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultCookieSetting(
      std::string* provider_id = nullptr) const;

  // Returns all patterns with a non-default cookie setting, mapped to their
  // actual settings, in the precedence order of the setting rules.
  //
  // This may be called on any thread.
  ContentSettingsForOneType GetCookieSettings() const;

  // Sets the default content setting (CONTENT_SETTING_ALLOW,
  // CONTENT_SETTING_BLOCK, or CONTENT_SETTING_SESSION_ONLY) for cookies.
  //
  // This should only be called on the UI thread.
  void SetDefaultCookieSetting(ContentSetting setting);

  // Sets the cookie setting for the given url.
  //
  // This should only be called on the UI thread.
  void SetCookieSetting(const GURL& primary_url, ContentSetting setting);

  // Returns whether a cookie access is allowed for the `TPCD_METADATA_GRANTS`
  // content settings type, scoped on the provided `url` and `first_party_url`.
  //
  // This may be called on any thread.
  bool IsAllowedByTpcdMetadataGrant(const GURL& url,
                                    const GURL& first_party_url) const;

  // Sets the `TPCD_HEURISTICS_GRANTS` setting for the given (`url`,
  // `first_party_url`) pair, for the provided `ttl`. If
  // `use_schemeless_pattern` is set, the patterns will be generated from
  // `ContentSettingsPattern::FromUrl`, which maps HTTP URLs onto a wildcard
  // scheme.
  //
  // This should only be called on the UI thread.
  void SetTemporaryCookieGrantForHeuristic(
      const GURL& url,
      const GURL& first_party_url,
      base::TimeDelta ttl,
      bool use_schemeless_patterns = false);

  // Represents the TTL of each User Bypass entries.
  static constexpr base::TimeDelta kUserBypassEntriesTTL = base::Days(90);

  // Sets the cookie setting to allow for the |first_party_url|.
  void SetCookieSettingForUserBypass(const GURL& first_party_url);

  // Determines the current state of User Bypass for the given
  // |first_party_url|. This method only takes into consideration the hard-coded
  // default and user-specified values of cookie setting.
  //
  // Notes:
  // - Storage partitioning could be enabled by default even when third-party
  // cookies are allowed.
  // - Also, user bypass as of now is only integrated with the runtime feature
  // of the top-level frame.
  // - Cases like WebUIs, allowlisted internal apps, and extension iframes are
  // usually being exempted from storage partitioning or are allowlisted. Thus,
  // not covered by user bypass at this state of art.
  bool IsStoragePartitioningBypassEnabled(const GURL& first_party_url);

  // Sets the `settings_for_3pcd_metadata_grants_` `ContentSettingsForOneType`
  // to be held and accessed from memory by the cookie settings object.
  void SetContentSettingsFor3pcdMetadataGrants(
      const ContentSettingsForOneType settings) {
    base::AutoLock lock(tpcd_lock_);
    settings_for_3pcd_metadata_grants_ = settings;
  }

  ContentSettingsForOneType GetTpcdMetadataGrantsForTesting() {
    base::AutoLock lock(tpcd_lock_);
    return settings_for_3pcd_metadata_grants_;
  }

  // Resets the cookie setting for the given url.
  //
  // This should only be called on the UI thread.
  void ResetCookieSetting(const GURL& primary_url);

  // Returns true if cookies are allowed for *most* third parties on |url|.
  // There might be rules allowing or blocking specific third parties from
  // accessing cookies.
  //
  // This should only be called on the UI thread.
  bool IsThirdPartyAccessAllowed(
      const GURL& first_party_url,
      content_settings::SettingInfo* info = nullptr) const;

  // Sets the cookie setting for the site and third parties embedded in it.
  //
  // This should only be called on the UI thread.
  void SetThirdPartyCookieSetting(const GURL& first_party_url,
                                  ContentSetting setting);

  // Resets the third party cookie setting for the given url. Resets both site-
  // and origin-scoped exceptions since either one might be present.
  // `SetCookieSettingForUserBypass()` and `SetThirdPartyCookieSetting()` create
  // site- and origin-scoped exceptions respectively.
  //
  // This should only be called on the UI thread.
  void ResetThirdPartyCookieSetting(const GURL& first_party_url);

  bool IsStorageDurable(const GURL& origin) const;

  // Returns true if third party cookies should be blocked.
  //
  // This method may be called on any thread. Virtual for testing.
  bool ShouldBlockThirdPartyCookies() const override;

  // Returns true iff third party cookies deprecation mitigations should be
  // allowed.
  //
  // NOTE: Most mitigations will also be individually gated behind dedicated
  // feature flags.
  //
  // This method may be called on any thread. Virtual for testing.
  bool MitigationsEnabledFor3pcd() const override;

  // Returns true iff tracking protection for 3PCD (prefs + UX) is enabled.
  bool TrackingProtectionEnabledFor3pcd() const;

  // Returns true if there is an active storage access exception with
  // |first_party_url| as the secondary pattern.
  bool HasAnyFrameRequestedStorageAccess(const GURL& first_party_url) const;

  // content_settings::CookieSettingsBase:
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override;

  // Detaches the |CookieSettings| from |PrefService|. This methods needs to be
  // called before destroying the service. Afterwards, only const methods can be
  // called.
  void ShutdownOnUIThread() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }

  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

 protected:
  ~CookieSettings() override;

 private:
  // Evaluates if third-party cookies are blocked. Should only be called
  // when the preference changes to update the internal state.
  bool ShouldBlockThirdPartyCookiesInternal();

  // Evaluates whether third party cookies deprecation mitigations should be
  // enabled.
  bool MitigationsEnabledFor3pcdInternal();

  void OnCookiePreferencesChanged();

  // content_settings::CookieSettingsBase:
  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const override;
  ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      content_settings::SettingInfo* info = nullptr) const override;
  bool IsThirdPartyCookiesAllowedScheme(
      const std::string& scheme) const override;
  bool IsStorageAccessApiEnabled() const override;

  // TrackingProtectionSettingsObserver:
  void OnTrackingProtection3pcdChanged() override;
  void OnBlockAllThirdPartyCookiesChanged() override;

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer> observers_;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  base::ScopedObservation<privacy_sandbox::TrackingProtectionSettings,
                          privacy_sandbox::TrackingProtectionSettingsObserver>
      tracking_protection_settings_observation_{this};
  const scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};
  PrefChangeRegistrar pref_change_registrar_;
  const bool is_incognito_;
  const char* extension_scheme_;  // Weak.

  // Used to represent content settings for 3PC accesses granted via the
  // component updater service. This type will only be populated when
  // `net::features::kTpcdMetadataGrants` is enabled.
  //
  // TODO(http://b/290039145): There's a chance for the list to get considerably
  // big. Look into optimizing memory by querying straight from a global
  // service.

  mutable base::Lock tpcd_lock_;
  ContentSettingsForOneType settings_for_3pcd_metadata_grants_
      GUARDED_BY(tpcd_lock_);

  mutable base::Lock lock_;
  bool block_third_party_cookies_ GUARDED_BY(lock_);
  bool mitigations_enabled_for_3pcd_ GUARDED_BY(lock_) = false;
  bool tracking_protection_enabled_for_3pcd_ GUARDED_BY(lock_) = false;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_H_
