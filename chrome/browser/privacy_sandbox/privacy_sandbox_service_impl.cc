// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <algorithm>
#include <iterator>
#include <numeric>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_confirmation.h"
#include "chrome/browser/privacy_sandbox/profile_bucket_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt_helper.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/common/semantic_tree.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/common/content_features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "privacy_sandbox_countries_impl.h"
#include "privacy_sandbox_service_impl.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/widget/widget.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

DEFINE_REQUIRED_NOTICE_IDENTIFIER(kPrivacySandboxNotice);

namespace {

using PromptAction = ::PrivacySandboxService::PromptAction;
using SurfaceType = ::PrivacySandboxService::SurfaceType;
using PromptStartupState = ::PrivacySandboxService::PromptStartupState;
using NoticeQueueState = ::PrivacySandboxService::NoticeQueueState;
using FakeNoticePromptSuppressionReason =
    ::PrivacySandboxService::FakeNoticePromptSuppressionReason;
using PrimaryAccountUserGroups =
    ::PrivacySandboxService::PrimaryAccountUserGroups;

constexpr char kBlockedTopicsTopicKey[] = "topic";

bool g_prompt_disabled_for_tests = false;

bool IsFirstRunSuppressed(const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kNoFirstRun);
}

// Returns whether 3P cookies are blocked by |cookie_settings|. This can be
// either through blocking 3P cookies directly, or blocking all cookies.
// Blocking in this case also covers the "3P cookies limited" state.
bool ShouldBlockThirdPartyOrFirstPartyCookies(
    content_settings::CookieSettings* cookie_settings) {
  const auto default_content_setting =
      cookie_settings->GetDefaultCookieSetting();
  return cookie_settings->ShouldBlockThirdPartyCookies() ||
         default_content_setting == ContentSetting::CONTENT_SETTING_BLOCK;
}

#if !BUILDFLAG(IS_ANDROID)
const char* QueueSourceToUserActionString(NoticeQueueState notice_state) {
  switch (notice_state) {
    case NoticeQueueState::kQueueOnStartup:
      return "NoticeQueue.PrivacySandboxNotice.QueueOnStartup";
    case NoticeQueueState::kQueueOnThOrNav:
      return "NoticeQueue.PrivacySandboxNotice.QueueOnThOrNav";
    case NoticeQueueState::kReleaseOnThOrNav:
      return "NoticeQueue.PrivacySandboxNotice.ReleaseOnThOrNav";
    case NoticeQueueState::kReleaseOnDMA:
      return "NoticeQueue.PrivacySandboxNotice.ReleaseOnDMA";
    case NoticeQueueState::kReleaseOnShown:
      return "NoticeQueue.PrivacySandboxNotice.ReleaseOnShown";
    default:
      NOTREACHED();
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Similar to the function above, but checks for ALL 3P cookies to be blocked
// pre and post 3PCD.
bool AreAllThirdPartyCookiesBlocked(
    content_settings::CookieSettings* cookie_settings,
    PrefService* prefs,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings) {
  // Check if 1PCs are blocked.
  if (cookie_settings->GetDefaultCookieSetting() ==
      ContentSetting::CONTENT_SETTING_BLOCK) {
    return true;
  }
  // Check if all 3PCs are blocked.
  return tracking_protection_settings->AreAllThirdPartyCookiesBlocked() ||
         (!tracking_protection_settings->IsTrackingProtection3pcdEnabled() &&
          prefs->GetInteger(prefs::kCookieControlsMode) ==
              static_cast<int>(
                  content_settings::CookieControlsMode::kBlockThirdParty));
}

// Sorts |topics| alphabetically by topic display name for display.
// In addition, removes duplicate topics.
void SortAndDeduplicateTopicsForDisplay(
    std::vector<privacy_sandbox::CanonicalTopic>& topics) {
  std::sort(topics.begin(), topics.end(),
            [](const privacy_sandbox::CanonicalTopic& a,
               const privacy_sandbox::CanonicalTopic& b) {
              return a.GetLocalizedRepresentation() <
                     b.GetLocalizedRepresentation();
            });
  topics.erase(std::unique(topics.begin(), topics.end()), topics.end());
}

// Returns whether |profile_type|, and the current browser session on CrOS,
// represent a regular (e.g. non guest, non system, non kiosk) profile.
bool IsRegularProfile(profile_metrics::BrowserProfileType profile_type) {
  if (profile_type != profile_metrics::BrowserProfileType::kRegular) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Any Device Local account, which is a CrOS concept powering things like
  // Kiosks and Managed Guest Sessions, is not considered regular.
  return !chromeos::IsManagedGuestSession() && !chromeos::IsKioskSession() &&
         !profiles::IsChromeAppKioskSession();
#else
  return true;
#endif
}

// Returns the text contents of the Topics Consent dialog.
std::string GetTopicsConfirmationText() {
  std::vector<int> string_ids = {
      IDS_PRIVACY_SANDBOX_M1_CONSENT_TITLE,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_1,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_2,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_3,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_4,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_EXPAND_LABEL,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_1,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_2,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_3,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_LINK};

  return std::accumulate(
      string_ids.begin(), string_ids.end(), std::string(),
      [](const std::string& previous_result, int next_id) {
        auto next_string = l10n_util::GetStringUTF8(next_id);
        // Remove bold tags present in some strings.
        base::ReplaceSubstringsAfterOffset(&next_string, 0, "<b>", "");
        base::ReplaceSubstringsAfterOffset(&next_string, 0, "</b>", "");
        return previous_result + (!previous_result.empty() ? " " : "") +
               next_string;
      }

  );
}

// Returns the text contents of the Topics settings page.
std::string GetTopicsSettingsText(bool did_consent,
                                  bool has_current_topics,
                                  bool has_blocked_topics) {
  // `did_consent` refers to the _updated_ state, and so the previous state,
  // e.g. when the user clicked the toggle, will be the opposite.
  auto topics_prev_enabled = !did_consent;

  // A user should only have current topics while topics is enabled. Old topics
  // will not appear when the user enables, as they will have been cleared when
  // topics was previously disabled, or never generated at all.
  DCHECK(topics_prev_enabled || !has_current_topics);

  int blocked_topics_description =
      has_blocked_topics
          ? IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_NEW
          : IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_V2;

  std::vector<int> string_ids = {
      IDS_SETTINGS_TOPICS_PAGE_TITLE,
      IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
      IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2,
      IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING,
      IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
      IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW,
      blocked_topics_description,
      IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};

  // Additional strings are displayed if there were no current topics, either
  // because they were empty, or because Topics was disabled. These will have
  // appeared after the current topics description.
  if (!topics_prev_enabled) {
    string_ids.insert(
        string_ids.begin() + 5,
        IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED);
  } else if (!has_current_topics) {
    string_ids.insert(
        string_ids.begin() + 5,
        IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY_TEXT_V2);
  }

  return std::accumulate(string_ids.begin(), string_ids.end(), std::string(),
                         [](const std::string& previous_result, int next_id) {
                           auto next_string = l10n_util::GetStringUTF8(next_id);
                           return previous_result +
                                  (!previous_result.empty() ? " " : "") +
                                  next_string;
                         });
}

// Returns whether this is a Google Chrome-branded build.
bool IsChromeBuild() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

void RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(bool value) {
  base::UmaHistogramBoolean(
      "PrivacySandbox.ProtectedAudience.JoiningTopFrameDisplayed", value);
}

constexpr std::string_view GetTopicsConsentNoticeName(
    SurfaceType surface_type) {
  switch (surface_type) {
    case SurfaceType::kDesktop: {
      return privacy_sandbox::kTopicsConsentModal;
    }
    case SurfaceType::kBrApp: {
      return privacy_sandbox::kTopicsConsentModalClankBrApp;
    }
    case SurfaceType::kAGACCT: {
      return privacy_sandbox::kTopicsConsentModalClankCCT;
    }
  }
}

constexpr std::string_view GetProtectedAudienceMeasurementNoticeName(
    SurfaceType surface_type) {
  switch (surface_type) {
    case SurfaceType::kDesktop: {
      return privacy_sandbox::kProtectedAudienceMeasurementNoticeModal;
    }
    case SurfaceType::kBrApp: {
      return privacy_sandbox::
          kProtectedAudienceMeasurementNoticeModalClankBrApp;
    }
    case SurfaceType::kAGACCT: {
      return privacy_sandbox::kProtectedAudienceMeasurementNoticeModalClankCCT;
    }
  }
}

constexpr std::string_view GetThreeAdsAPIsNoticeName(SurfaceType surface_type) {
  switch (surface_type) {
    case SurfaceType::kDesktop: {
      return privacy_sandbox::kThreeAdsAPIsNoticeModal;
    }
    case SurfaceType::kBrApp: {
      return privacy_sandbox::kThreeAdsAPIsNoticeModalClankBrApp;
    }
    case SurfaceType::kAGACCT: {
      return privacy_sandbox::kThreeAdsAPIsNoticeModalClankCCT;
    }
  }
}

constexpr std::string_view GetMeasurementNoticeName(SurfaceType surface_type) {
  switch (surface_type) {
    case SurfaceType::kDesktop: {
      return privacy_sandbox::kMeasurementNoticeModal;
    }
    case SurfaceType::kBrApp: {
      return privacy_sandbox::kMeasurementNoticeModalClankBrApp;
    }
    case SurfaceType::kAGACCT: {
      return privacy_sandbox::kMeasurementNoticeModalClankCCT;
    }
  }
}

std::string_view GetNoticeName(PromptAction action, SurfaceType surface_type) {
  std::string_view empty_view;
  switch (action) {
    case PromptAction::kConsentShown:
    case PromptAction::kConsentAccepted:
    case PromptAction::kConsentDeclined:
    case PromptAction::kConsentMoreInfoOpened:
      return GetTopicsConsentNoticeName(surface_type);
    case PromptAction::kRestrictedNoticeShown:
    case PromptAction::kRestrictedNoticeAcknowledge:
    case PromptAction::kRestrictedNoticeOpenSettings:
      return GetMeasurementNoticeName(surface_type);
    case PromptAction::kNoticeShown:
    case PromptAction::kNoticeAcknowledge:
    case PromptAction::kNoticeOpenSettings:
    case PromptAction::kNoticeMoreInfoOpened:
      return privacy_sandbox::IsConsentRequired()
                 ? GetProtectedAudienceMeasurementNoticeName(surface_type)
                 : GetThreeAdsAPIsNoticeName(surface_type);
    // Ads API UX Enhancements
    case PromptAction::kNoticeSiteSuggestedAdsMoreInfoOpened:
    case PromptAction::kNoticeAdsMeasurementMoreInfoOpened:
      return GetProtectedAudienceMeasurementNoticeName(surface_type);
    default:
      return empty_view;
  }
}

void CreateTimingHistogram(const std::string& name, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(1),
                                base::Days(10), 100);
}

void EmitFakeNoticePromptSuppressionMetrics(PrefService* pref_service,
                                            std::string_view metrics_prefix,
                                            int current_suppression,
                                            std::string_view pref_name) {
  base::UmaHistogramCounts100(
      base::StrCat({metrics_prefix, ".PromptSuppressionReasonsCombined"}),
      current_suppression);
  if (current_suppression &
      static_cast<int>(FakeNoticePromptSuppressionReason::k3PC_Blocked)) {
    base::UmaHistogramEnumeration(
        base::StrCat({metrics_prefix, ".PromptSuppressionReason"}),
        FakeNoticePromptSuppressionReason::k3PC_Blocked);
  }
  if (current_suppression &
      static_cast<int>(FakeNoticePromptSuppressionReason::kCapabilityFalse)) {
    base::UmaHistogramEnumeration(
        base::StrCat({metrics_prefix, ".PromptSuppressionReason"}),
        FakeNoticePromptSuppressionReason::kCapabilityFalse);
  }
  if (current_suppression &
      static_cast<int>(FakeNoticePromptSuppressionReason::kManagedDevice)) {
    base::UmaHistogramEnumeration(
        base::StrCat({metrics_prefix, ".PromptSuppressionReason"}),
        FakeNoticePromptSuppressionReason::kManagedDevice);
  }
  if (current_suppression &
      static_cast<int>(FakeNoticePromptSuppressionReason::kNoticeShownBefore)) {
    base::UmaHistogramEnumeration(
        base::StrCat({metrics_prefix, ".PromptSuppressionReason"}),
        FakeNoticePromptSuppressionReason::kNoticeShownBefore);
    int notice_last_seen_days =
        (base::Time::Now() - pref_service->GetTime(pref_name)).InDaysFloored();
    base::UmaHistogramCounts1000(
        base::StrCat({metrics_prefix, ".PromptShownSince"}),
        notice_last_seen_days);
  }
}

int EmitFakeNoticeShownMetrics(PrefService* pref_service,
                               bool third_party_cookies_blocked,
                               PrimaryAccountUserGroups user_group,
                               std::string_view pref_name,
                               std::string_view metrics_prefix) {
  int current_suppression = 0;
  // Prompt already seen.
  if (pref_service->HasPrefPath(pref_name)) {
    current_suppression |=
        static_cast<int>(FakeNoticePromptSuppressionReason::kNoticeShownBefore);
  }

  // Enterprise account.
  if (pref_service->IsManagedPreference(prefs::kCookieControlsMode)) {
    current_suppression |=
        static_cast<int>(FakeNoticePromptSuppressionReason::kManagedDevice);
  }

  if (third_party_cookies_blocked) {
    current_suppression |=
        static_cast<int>(FakeNoticePromptSuppressionReason::k3PC_Blocked);
  }

  if (user_group == PrimaryAccountUserGroups::kSignedInCapabilityFalse) {
    current_suppression |=
        static_cast<int>(FakeNoticePromptSuppressionReason::kCapabilityFalse);
  }

  // If prompt isn't suppressed we should show it, if the notice has already
  // been shown the `current_suppression` will no longer be 0.
  if (!current_suppression) {
    pref_service->SetTime(pref_name, base::Time::Now());
    base::UmaHistogramBoolean(base::StrCat({metrics_prefix, ".PromptShown"}),
                              true);
  }
  return current_suppression;
}

}  // namespace

// static
bool PrivacySandboxService::IsUrlSuitableForPrompt(const GURL& url) {
  // The prompt should be shown on a limited list of pages:

  // about:blank is valid.
  if (url.IsAboutBlank()) {
    return true;
  }
  // Chrome settings page is valid. The subpages aren't as most of them are not
  // related to the prompt.
  if (url == GURL(chrome::kChromeUISettingsURL)) {
    return true;
  }
  // Chrome history is valid as the prompt mentions history.
  if (url == GURL(chrome::kChromeUIHistoryURL)) {
    return true;
  }
  // Only a Chrome controlled New Tab Page is valid. Third party NTP is still
  // Chrome controlled, but is without Google branding.
  if (url == GURL(chrome::kChromeUINewTabPageURL) ||
      url == GURL(chrome::kChromeUINewTabPageThirdPartyURL)) {
    return true;
  }

  return false;
}

// static
void PrivacySandboxService::SetPromptDisabledForTests(bool disabled) {
  g_prompt_disabled_for_tests = disabled;
}

PrivacySandboxServiceImpl::PrivacySandboxServiceImpl(
    Profile* profile,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    PrefService* pref_service,
    content::InterestGroupManager* interest_group_manager,
    profile_metrics::BrowserProfileType profile_type,
    content::BrowsingDataRemover* browsing_data_remover,
    HostContentSettingsMap* host_content_settings_map,
    browsing_topics::BrowsingTopicsService* browsing_topics_service,
    first_party_sets::FirstPartySetsPolicyService* first_party_sets_service,
    PrivacySandboxCountries* privacy_sandbox_countries)
    : profile_(profile),
      privacy_sandbox_settings_(privacy_sandbox_settings),
      tracking_protection_settings_(tracking_protection_settings),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service),
      interest_group_manager_(interest_group_manager),
      profile_type_(profile_type),
      browsing_data_remover_(browsing_data_remover),
      host_content_settings_map_(host_content_settings_map),
      browsing_topics_service_(browsing_topics_service),
      first_party_sets_policy_service_(first_party_sets_service),
      privacy_sandbox_countries_(privacy_sandbox_countries) {
  // Create notice storage
  notice_storage_ =
      std::make_unique<privacy_sandbox::PrivacySandboxNoticeStorage>();

  DCHECK(privacy_sandbox_settings_);
  DCHECK(pref_service_);
  DCHECK(cookie_settings_);
  CHECK(tracking_protection_settings_);
  CHECK(notice_storage_);

  // Register observers for the Privacy Sandbox preferences.
  user_prefs_registrar_.Init(pref_service_);
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1TopicsEnabled,
      base::BindRepeating(&PrivacySandboxServiceImpl::OnTopicsPrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1FledgeEnabled,
      base::BindRepeating(&PrivacySandboxServiceImpl::OnFledgePrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1AdMeasurementEnabled,
      base::BindRepeating(
          &PrivacySandboxServiceImpl::OnAdMeasurementPrefChanged,
          base::Unretained(this)));

  // If the Sandbox is currently restricted, disable it and reset any consent
  // information. The user must manually enable the sandbox if they stop being
  // restricted.
  if (IsPrivacySandboxRestricted()) {
    // Disable M1 prefs. Measurement pref should not be reset when restricted
    // notice feature is enabled.
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
    if (!privacy_sandbox::IsRestrictedNoticeRequired()) {
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                false);
    }

    // Clear any recorded consent information.
    pref_service_->ClearPref(prefs::kPrivacySandboxTopicsConsentGiven);
    pref_service_->ClearPref(prefs::kPrivacySandboxTopicsConsentLastUpdateTime);
    pref_service_->ClearPref(
        prefs::kPrivacySandboxTopicsConsentLastUpdateReason);
    pref_service_->ClearPref(
        prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate);
  }

  PromptSuppressedReason prompt_suppressed_reason =
      static_cast<PromptSuppressedReason>(
          pref_service->GetInteger(prefs::kPrivacySandboxM1PromptSuppressed));

  // kRestricted prompt suppression reason must be cleared at startup when
  // restricted notice feature is enabled.
  if (privacy_sandbox::IsRestrictedNoticeRequired() &&
      prompt_suppressed_reason == PromptSuppressedReason::kRestricted) {
    pref_service_->ClearPref(prefs::kPrivacySandboxM1PromptSuppressed);
  }

  // Special usecase for Third Party Coookies: Make sure the 3PC suppression
  // value is overridden in case 3PC Blocking is not a valid reason to block the
  // Prompt.
  if (prompt_suppressed_reason ==
          PromptSuppressedReason::kThirdPartyCookiesBlocked &&
      base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies)) {
    pref_service_->ClearPref(prefs::kPrivacySandboxM1PromptSuppressed);
  }

  // Check for FPS pref init at each startup.
  // TODO(crbug.com/40234448): Remove this logic when most users have run init.
  MaybeInitializeRelatedWebsiteSetsPref();

  // Record preference state for UMA at each startup.
  LogPrivacySandboxState();

  // Init the Identity Manager Observation and metrics.
  MaybeInitIdentityManager();
}

PrivacySandboxServiceImpl::~PrivacySandboxServiceImpl() = default;

void PrivacySandboxServiceImpl::MaybeInitIdentityManager() {
  // Non Regular Profiles are excluded from anything Dark Launch related.
  if (!IsRegularProfile(profile_type_)) {
    return;
  }

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);

  if (!identity_manager_) {
    base::UmaHistogramBoolean(
        "PrivacySandbox.DarkLaunch.IdentityManagerSuccess", false);
    // If there's no identity manager, then don't try to observe it.
    return;
  }
  base::UmaHistogramBoolean("PrivacySandbox.DarkLaunch.IdentityManagerSuccess",
                            true);

  identity_manager_obs_.Observe(identity_manager_.get());

  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    SetPrimaryAccountState(PrimaryAccountUserGroups::kSignedOut);
  } else {
    SetPrimaryAccountState(
        PrimaryAccountUserGroups::kSignedInCapabilityUnknown);
  }
  // Account capabilities are not available immediately at startup and are
  // updated asynchronously, so metrics relating to those will be recorded once
  // in `OnExtendedAccountInfoUpdated`.
}

void PrivacySandboxServiceImpl::SetPrimaryAccountState(
    PrimaryAccountUserGroups user_group_to_set) {
  if (user_group_to_set == primary_account_state_) {
    return;
  }

  std::string profile_bucket = privacy_sandbox::GetProfileBucketName(profile_);
  if (!profile_bucket.empty()) {
    base::UmaHistogramEnumeration(base::StrCat({"PrivacySandbox.DarkLaunch.",
                                                profile_bucket, ".UserGroups"}),
                                  user_group_to_set);
  }
  primary_account_state_ = user_group_to_set;
}

void PrivacySandboxServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  std::string profile_bucket = privacy_sandbox::GetProfileBucketName(profile_);
  if (profile_bucket.empty()) {
    return;
  }

  if (event_details.GetCurrentState().consent_level !=
      signin::ConsentLevel::kSignin) {
    return;
  }

  // We only keep track of the first sign in time.
  if (profile_->GetPrefs()->GetTime(
          prefs::kPrivacySandboxFakeNoticeFirstSignInTime) != base::Time()) {
    return;
  }

  base::Time sign_in_time = base::Time::Now();
  profile_->GetPrefs()->SetTime(prefs::kPrivacySandboxFakeNoticeFirstSignInTime,
                                sign_in_time);
  CreateTimingHistogram(
      base::StrCat({"PrivacySandbox.DarkLaunch.", profile_bucket,
                    ".ProfileSignInDuration"}),
      sign_in_time - profile_->GetCreationTime());
}

void PrivacySandboxServiceImpl::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }
  switch (info.capabilities.can_run_chrome_privacy_sandbox_trials()) {
    case signin::Tribool::kUnknown:
      SetPrimaryAccountState(
          PrimaryAccountUserGroups::kSignedInCapabilityUnknown);
      break;
    case signin::Tribool::kFalse:
      SetPrimaryAccountState(
          PrimaryAccountUserGroups::kSignedInCapabilityFalse);
      break;
    case signin::Tribool::kTrue:
      SetPrimaryAccountState(PrimaryAccountUserGroups::kSignedInCapabilityTrue);
      break;
  }
}

void PrivacySandboxServiceImpl::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  SetPrimaryAccountState(PrimaryAccountUserGroups::kSignedOut);
  base::Time first_sign_in_time = profile_->GetPrefs()->GetTime(
      prefs::kPrivacySandboxFakeNoticeFirstSignInTime);
  std::string profile_bucket = privacy_sandbox::GetProfileBucketName(profile_);
  if (profile_bucket.empty()) {
    return;
  }

  // If this pref wasn't recorded, it means the user has already signed in at
  // the time of startup, we won't record the metric here since we have no way
  // of knowing when the user signed in.
  if (first_sign_in_time == base::Time()) {
    return;
  }

  // We only keep track of the first sign out time.
  if (profile_->GetPrefs()->GetTime(
          prefs::kPrivacySandboxFakeNoticeFirstSignOutTime) != base::Time()) {
    return;
  }

  base::Time sign_out_time = base::Time::Now();
  profile_->GetPrefs()->SetTime(
      prefs::kPrivacySandboxFakeNoticeFirstSignOutTime, sign_out_time);
  CreateTimingHistogram(
      base::StrCat({"PrivacySandbox.DarkLaunch.", profile_bucket,
                    ".ProfileSignOutDuration"}),
      sign_out_time - first_sign_in_time);
}

PrivacySandboxService::PromptType
// TODO(crbug.com/352575567): Use the SurfaceType passed in.
PrivacySandboxServiceImpl::GetRequiredPromptType(SurfaceType surface_type) {
  // We delay emitting the metrics here so the profile manager can finish
  // setting up and retrieving the profile buckets.
  if (should_emit_dark_launch_startup_metrics_) {
    MaybeEmitPromptStartupAccountMetrics();
    should_emit_dark_launch_startup_metrics_ = false;
  }
  // If the prompt is disabled for testing, never show it.
  if (g_prompt_disabled_for_tests) {
    return PromptType::kNone;
  }

  // If the profile isn't a regular profile, no prompt should ever be shown.
  if (!IsRegularProfile(profile_type_)) {
    return PromptType::kNone;
  }

  bool third_party_cookies_blocked = AreAllThirdPartyCookiesBlocked(
      cookie_settings_.get(), pref_service_, tracking_protection_settings_);
  MaybeEmitFakeNoticePromptMetrics(third_party_cookies_blocked);

  // Forced testing feature parameters override everything.
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kDisablePrivacySandboxPrompts)) {
    return PromptType::kNone;
  }

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowConsentForTesting
          .Get()) {
    return PromptType::kM1Consent;
  }

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowNoticeRowForTesting
          .Get()) {
    return PromptType::kM1NoticeROW;
  }

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowNoticeEeaForTesting
          .Get()) {
    return PromptType::kM1NoticeEEA;
  }

  if (privacy_sandbox::
          kPrivacySandboxSettings4ForceShowNoticeRestrictedForTesting.Get()) {
    return PromptType::kM1NoticeRestricted;
  }

  // Suppress the prompt if we force --no-first-run for testing
  // and benchmarking.
  if (IsFirstRunSuppressed(*base::CommandLine::ForCurrentProcess())) {
    return PromptType::kNone;
  }

  // If this a non-Chrome build, do not show a prompt.
  if (!(force_chrome_build_for_tests_ || IsChromeBuild())) {
    return PromptType::kNone;
  }

  // If neither a notice nor a consent is required, do not show a prompt.
  if (!privacy_sandbox::IsNoticeRequired() &&
      !privacy_sandbox::IsConsentRequired()) {
    return PromptType::kNone;
  }

  // Only one of the consent or notice should be required.
  DCHECK(!privacy_sandbox::IsNoticeRequired() ||
         !privacy_sandbox::IsConsentRequired());

  // If a prompt was suppressed once, for any reason, it will forever remain
  // suppressed and a prompt will not be shown.
  if (static_cast<PromptSuppressedReason>(pref_service_->GetInteger(
          prefs::kPrivacySandboxM1PromptSuppressed)) !=
      PromptSuppressedReason::kNone) {
    return PromptType::kNone;
  }

  // If an Admin controls any of the K-APIs or suppresses the prompt explicitly
  // then don't show the prompt.
  if (IsM1PrivacySandboxEffectivelyManaged(pref_service_)) {
    return PromptType::kNone;
  }

  // TODO(crbug.com/383566930): Modify the suppression prefs to be set multiple
  // suppressions if they exist instead of exiting early.

  // If third party cookies are blocked, set the suppression reason as such, and
  // do not show a prompt. Unless the prompt is allowed when 3P Cookies are
  // blocked.
  if (third_party_cookies_blocked &&
      !base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies)) {
    pref_service_->SetInteger(
        prefs::kPrivacySandboxM1PromptSuppressed,
        static_cast<int>(PromptSuppressedReason::kThirdPartyCookiesBlocked));
    return PromptType::kNone;
  }

  // If the Privacy Sandbox is restricted, set the suppression reason as such,
  // and do not show a prompt.
  if (privacy_sandbox_settings_->IsPrivacySandboxRestricted() &&
      !privacy_sandbox::IsRestrictedNoticeRequired()) {
    pref_service_->SetInteger(
        prefs::kPrivacySandboxM1PromptSuppressed,
        static_cast<int>(PromptSuppressedReason::kRestricted));
    return PromptType::kNone;
  }

  if (privacy_sandbox::IsRestrictedNoticeRequired()) {
    CHECK(privacy_sandbox::IsConsentRequired() ||
          privacy_sandbox::IsNoticeRequired());
    if (!pref_service_->GetBoolean(
            prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged) &&
        !pref_service_->GetBoolean(
            prefs::kPrivacySandboxM1EEANoticeAcknowledged) &&
        !pref_service_->GetBoolean(
            prefs::kPrivacySandboxM1RowNoticeAcknowledged)) {
      if (privacy_sandbox_settings_->IsSubjectToM1NoticeRestricted()) {
        return PromptType::kM1NoticeRestricted;
      }
      if (privacy_sandbox_settings_->IsPrivacySandboxRestricted()) {
        pref_service_->SetInteger(
            prefs::kPrivacySandboxM1PromptSuppressed,
            static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian));
        pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                  true);
        return PromptType::kNone;
      }
    }
  }

  if (privacy_sandbox::IsConsentRequired()) {
    if (pref_service_->GetBoolean(
            prefs::kPrivacySandboxM1ConsentDecisionMade)) {
      // Since a consent decision has been made, if the eea notice has already
      // been acknowledged, do not show a prompt; else, show the eea notice.
      if (pref_service_->GetBoolean(
              prefs::kPrivacySandboxM1EEANoticeAcknowledged)) {
        return PromptType::kNone;
      } else {
        return PromptType::kM1NoticeEEA;
      }
    } else {
      // A consent decision has not yet been made. If the user has seen a notice
      // and disabled Topics, we should not attempt to consent them. As they
      // already have sufficient notice for the other APIs, no prompt is
      // required.
      if (pref_service_->GetBoolean(
              prefs::kPrivacySandboxM1RowNoticeAcknowledged) &&
          !pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)) {
        pref_service_->SetInteger(
            prefs::kPrivacySandboxM1PromptSuppressed,
            static_cast<int>(
                PromptSuppressedReason::
                    kROWFlowCompletedAndTopicsDisabledBeforeEEAMigration));
        return PromptType::kNone;
      }
      return PromptType::kM1Consent;
    }
  }

  DCHECK(privacy_sandbox::IsNoticeRequired());

  // If a user that migrated from EEA to ROW has already completed the EEA
  // consent and notice flow, set the suppression reason as such and do not show
  // a prompt.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade) &&
      (pref_service_->GetBoolean(
          prefs::kPrivacySandboxM1EEANoticeAcknowledged))) {
    pref_service_->SetInteger(
        prefs::kPrivacySandboxM1PromptSuppressed,
        static_cast<int>(
            PromptSuppressedReason::kEEAFlowCompletedBeforeRowMigration));
    return PromptType::kNone;
  }

  // If either the ROW notice or the restricted notice has already been
  // acknowledged, do not show a prompt. Else, show the row notice prompt.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxM1RowNoticeAcknowledged) ||
      pref_service_->GetBoolean(
          prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged)) {
    return PromptType::kNone;
  } else {
    return PromptType::kM1NoticeROW;
  }
}

void UpdateNoticeStorage(
    PromptAction action,
    privacy_sandbox::PrivacySandboxNoticeStorage* notice_storage,
    PrefService* pref_service,
    SurfaceType surface_type) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPsDualWritePrefsToNoticeStorage)) {
    return;
  }

  // Set correct notice names, ready to receive and log PromptActions. Update
  // GetNoticeName when adding more entries to the switch statement.
  std::string_view notice_name = GetNoticeName(action, surface_type);

  switch (action) {
    // Topics notices (only shown for EEA, consent option)
    case PromptAction::kConsentShown: {
      notice_storage->SetNoticeShown(pref_service, notice_name,
                                     base::Time::Now());
      break;
    }
    case PromptAction::kConsentAccepted: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name, privacy_sandbox::NoticeActionTaken::kOptIn,
          base::Time::Now());
      break;
    }
    case PromptAction::kConsentDeclined: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name,
          privacy_sandbox::NoticeActionTaken::kOptOut, base::Time::Now());
      break;
    }
    case PromptAction::kConsentMoreInfoOpened: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name,
          privacy_sandbox::NoticeActionTaken::kLearnMore, base::Time::Now());
      break;
    }
    // EEA and ROW notices
    case PromptAction::kNoticeShown: {
      notice_storage->SetNoticeShown(pref_service, notice_name,
                                     base::Time::Now());
      break;
    }
    case PromptAction::kNoticeAcknowledge: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name, privacy_sandbox::NoticeActionTaken::kAck,
          base::Time::Now());
      break;
    }
    case PromptAction::kNoticeOpenSettings: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name,
          privacy_sandbox::NoticeActionTaken::kSettings, base::Time::Now());
      break;
    }
    case PromptAction::kNoticeMoreInfoOpened:
    // Ads API UX Enhancements
    case PromptAction::kNoticeSiteSuggestedAdsMoreInfoOpened:
    case PromptAction::kNoticeAdsMeasurementMoreInfoOpened: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name,
          privacy_sandbox::NoticeActionTaken::kLearnMore, base::Time::Now());
      break;
    }

    // Restricted notices
    case PromptAction::kRestrictedNoticeShown: {
      DCHECK(privacy_sandbox::IsRestrictedNoticeRequired());
      notice_storage->SetNoticeShown(pref_service, notice_name,
                                     base::Time::Now());
      break;
    }
    case PromptAction::kRestrictedNoticeAcknowledge: {
      DCHECK(privacy_sandbox::IsRestrictedNoticeRequired());
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name, privacy_sandbox::NoticeActionTaken::kAck,
          base::Time::Now());
      break;
    }
    case PromptAction::kRestrictedNoticeOpenSettings: {
      DCHECK(privacy_sandbox::IsRestrictedNoticeRequired());
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name,
          privacy_sandbox::NoticeActionTaken::kSettings, base::Time::Now());
      break;
    }
    default:
      break;
  }
}

#if !BUILDFLAG(IS_ANDROID)
user_education::ProductMessagingController*
PrivacySandboxServiceImpl::GetProductMessagingController() {
  if (!product_messaging_controller_) {
    // If there is a valid service, set it.
    if (auto* service =
            UserEducationServiceFactory::GetForBrowserContext(profile_)) {
      product_messaging_controller_ = &service->product_messaging_controller();
    }
  }
  return product_messaging_controller_;
}

void PrivacySandboxServiceImpl::SetSuppressQueue(bool suppress_queue) {
  suppress_queue_ = suppress_queue;
}

bool PrivacySandboxServiceImpl::IsHoldingHandle() {
  return static_cast<bool>(notice_handle_);
  // TODO(crbug.com/379900298): Add timeout for notice collision handle
}

void PrivacySandboxServiceImpl::HoldQueueHandle(
    user_education::RequiredNoticePriorityHandle messaging_priority_handle) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue)) {
    return;
  }
  notice_handle_ = std::move(messaging_priority_handle);
}

void SetQueueHandleShown(
    user_education::RequiredNoticePriorityHandle* notice_handle) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue)) {
    return;
  }
  notice_handle->SetShown();
}

void PrivacySandboxServiceImpl::MaybeUnqueueNotice(
    NoticeQueueState unqueue_source) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue) ||
      suppress_queue_) {
    return;
  }

  // Release the handle if we are holding it (checked by controller).
  notice_handle_.Release();

  // Ensure we don't attempt to access the product messaging controller if it
  // doesn't exist.
  if (auto* product_messaging_controller = GetProductMessagingController()) {
    // Unqueue if we are in the queue (handled by controller).
    product_messaging_controller->UnqueueRequiredNotice(kPrivacySandboxNotice);
    base::RecordAction(
        base::UserMetricsAction(QueueSourceToUserActionString(unqueue_source)));
  }
}

bool PrivacySandboxServiceImpl::IsNoticeQueued() {
  if (auto* product_messaging_controller = GetProductMessagingController()) {
    return product_messaging_controller->IsNoticeQueued(kPrivacySandboxNotice);
  }
  return false;
}

void PrivacySandboxServiceImpl::MaybeQueueNotice(
    NoticeQueueState queue_source) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxNoticeQueue) ||
      suppress_queue_) {
    return;
  }
  // We don't want to queue in the case the profile does not require a prompt.
  if (GetRequiredPromptType(SurfaceType::kDesktop) == PromptType::kNone) {
    return;
  }
  // If we are already holding the handle or in the queue, we don't want to
  // requeue.
  if (IsHoldingHandle() || IsNoticeQueued()) {
    return;
  }

  if (auto* product_messaging_controller = GetProductMessagingController()) {
    product_messaging_controller->QueueRequiredNotice(
      kPrivacySandboxNotice,
      base::BindOnce(&PrivacySandboxServiceImpl::HoldQueueHandle, weak_factory_.GetWeakPtr()), {/* TODO(crbug.com/370804492): When we add the DMA notice, add it to this show_after_ list*/});
    base::RecordAction(
        base::UserMetricsAction(QueueSourceToUserActionString(queue_source)));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

void PrivacySandboxServiceImpl::PromptActionOccurred(PromptAction action,
                                                     SurfaceType surface_type) {
  RecordPromptActionMetrics(action);
  UpdateNoticeStorage(action, notice_storage_.get(), pref_service_.get(),
                      surface_type);

  if (PromptAction::kNoticeShown == action ||
      PromptAction::kConsentShown == action ||
      PromptAction::kRestrictedNoticeShown == action) {
#if !BUILDFLAG(IS_ANDROID)
    SetQueueHandleShown(&notice_handle_);
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  if (PromptAction::kNoticeAcknowledge == action ||
      PromptAction::kNoticeOpenSettings == action) {
    if (privacy_sandbox::IsConsentRequired()) {
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged,
                                true);
      // It's possible the user is seeing this notice as part of an upgrade to
      // EEA consent. In this instance, we shouldn't alter the control state,
      // as the user may have already altered it in settings.
      if (!pref_service_->GetBoolean(
              prefs::kPrivacySandboxM1RowNoticeAcknowledged)) {
        pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
        pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                  true);
      }
    } else {
      DCHECK(privacy_sandbox::IsNoticeRequired());
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged,
                                true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                true);
    }
#if !BUILDFLAG(IS_ANDROID)
    MaybeCloseOpenPrompts();
#endif  // !BUILDFLAG(IS_ANDROID)
    // Consent-related PromptActions refer to to Topics Notice Consent
  } else if (PromptAction::kConsentAccepted == action) {
    DCHECK(privacy_sandbox::IsConsentRequired());
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade,
                              true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
    RecordUpdatedTopicsConsent(
        privacy_sandbox::TopicsConsentUpdateSource::kConfirmation, true);
  } else if (PromptAction::kConsentDeclined == action) {
    DCHECK(privacy_sandbox::IsConsentRequired());
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade,
                              true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    RecordUpdatedTopicsConsent(
        privacy_sandbox::TopicsConsentUpdateSource::kConfirmation, false);
  } else if (PromptAction::kRestrictedNoticeAcknowledge == action ||
             PromptAction::kRestrictedNoticeOpenSettings == action) {
    CHECK(privacy_sandbox::IsRestrictedNoticeRequired());
    pref_service_->SetBoolean(
        prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged, true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                              true);
#if !BUILDFLAG(IS_ANDROID)
    MaybeCloseOpenPrompts();
#endif  // !BUILDFLAG(IS_ANDROID)
  }
}

#if !BUILDFLAG(IS_ANDROID)
void PrivacySandboxServiceImpl::PromptOpenedForBrowser(Browser* browser,
                                                       views::Widget* widget) {
  DCHECK(!browsers_to_open_prompts_.count(browser));
  browsers_to_open_prompts_[browser] = widget;
}

void PrivacySandboxServiceImpl::PromptClosedForBrowser(Browser* browser) {
  DCHECK(browsers_to_open_prompts_.count(browser));
  browsers_to_open_prompts_.erase(browser);
}

bool PrivacySandboxServiceImpl::IsPromptOpenForBrowser(Browser* browser) {
  return browsers_to_open_prompts_.count(browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void PrivacySandboxServiceImpl::ForceChromeBuildForTests(
    bool force_chrome_build) {
  force_chrome_build_for_tests_ = force_chrome_build;
}

void PrivacySandboxServiceImpl::MaybeEmitFakeNoticePromptMetrics(
    bool third_party_cookies_blocked) {
  std::string profile_bucket = privacy_sandbox::GetProfileBucketName(profile_);
  if (profile_bucket.empty()) {
    return;
  }

  // Emit metrics for fake prompt synced prefs.
  auto metrics_prefix_synced = base::StrCat(
      {"PrivacySandbox.DarkLaunch.", profile_bucket, ".SyncedPref"});
  int current_suppression = EmitFakeNoticeShownMetrics(
      pref_service_, third_party_cookies_blocked, primary_account_state_,
      prefs::kPrivacySandboxFakeNoticePromptShownTimeSync,
      metrics_prefix_synced);

  // If the eligibility doesn't change we don't want to log any new
  // histograms.
  if (current_suppression &&
      current_suppression != prompt_suppression_bitmap_sync_) {
    prompt_suppression_bitmap_sync_ = current_suppression;
    EmitFakeNoticePromptSuppressionMetrics(
        pref_service_, metrics_prefix_synced, current_suppression,
        prefs::kPrivacySandboxFakeNoticePromptShownTimeSync);
  }

  // Emit metrics for fake prompt non-synced prefs.
  auto metrics_prefix = base::StrCat(
      {"PrivacySandbox.DarkLaunch.", profile_bucket, ".NonSyncedPref"});
  current_suppression = EmitFakeNoticeShownMetrics(
      pref_service_, third_party_cookies_blocked, primary_account_state_,
      prefs::kPrivacySandboxFakeNoticePromptShownTime, metrics_prefix);
  // If the eligibility doesn't change we don't want to log any new
  // histograms.
  if (current_suppression &&
      current_suppression != prompt_suppression_bitmap_) {
    prompt_suppression_bitmap_ = current_suppression;
    EmitFakeNoticePromptSuppressionMetrics(
        pref_service_, metrics_prefix, current_suppression,
        prefs::kPrivacySandboxFakeNoticePromptShownTime);
  }

  // Emit pref mismatch.
  bool sync_pref_exists =
      pref_service_->GetTime(
          prefs::kPrivacySandboxFakeNoticePromptShownTimeSync) != base::Time();
  bool nonsync_pref_exists =
      pref_service_->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTime) !=
      base::Time();
  if (sync_pref_exists && !nonsync_pref_exists) {
    base::UmaHistogramBoolean(base::StrCat({"PrivacySandbox.DarkLaunch.",
                                            profile_bucket, ".PrefMismatch"}),
                              true);
  } else if (sync_pref_exists && nonsync_pref_exists) {
    base::UmaHistogramBoolean(base::StrCat({"PrivacySandbox.DarkLaunch.",
                                            profile_bucket, ".PrefMismatch"}),
                              false);
  }
}

void PrivacySandboxServiceImpl::MaybeEmitPromptStartupAccountMetrics() {
  // No Startup Metrics emitted if the profile isn't regular.
  if (!IsRegularProfile(profile_type_)) {
    return;
  }
  std::string profile_bucket = privacy_sandbox::GetProfileBucketName(profile_);
  if (profile_bucket.empty()) {
    return;
  }
  // This histogram ideally should never emit PrimaryAccountUserGroups::kNotSet,
  // however we log it just in case.
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.DarkLaunch.", profile_bucket,
                    ".PrimaryAccountOnStartup"}),
      primary_account_state_);

  // Sign in time doesn't exist.
  if (primary_account_state_ != PrimaryAccountUserGroups::kSignedOut &&
      primary_account_state_ != PrimaryAccountUserGroups::kNotSet &&
      pref_service_->GetTime(prefs::kPrivacySandboxFakeNoticeFirstSignInTime) ==
          base::Time()) {
    base::UmaHistogramBoolean(
        base::StrCat({"PrivacySandbox.DarkLaunch.", profile_bucket,
                      ".UnknownProfileSignInDuration"}),
        true);
  }
}

bool PrivacySandboxServiceImpl::IsPrivacySandboxRestricted() {
  return privacy_sandbox_settings_->IsPrivacySandboxRestricted();
}

bool PrivacySandboxServiceImpl::IsRestrictedNoticeEnabled() {
  return privacy_sandbox_settings_->IsRestrictedNoticeEnabled();
}

void PrivacySandboxServiceImpl::SetRelatedWebsiteSetsDataAccessEnabled(
    bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                            enabled);
}

bool PrivacySandboxServiceImpl::IsRelatedWebsiteSetsDataAccessEnabled() const {
  return privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled();
}

bool PrivacySandboxServiceImpl::IsRelatedWebsiteSetsDataAccessManaged() const {
  return pref_service_->IsManagedPreference(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
}

void PrivacySandboxServiceImpl::RecordPromptStartupStateHistograms(
    PromptStartupState state) {
  std::string profile_bucket = privacy_sandbox::GetProfileBucketName(profile_);

  if (!profile_bucket.empty()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Settings.PrivacySandbox.", profile_bucket,
                      ".PromptStartupState"}),
        state);
    base::UmaHistogramEnumeration("Settings.PrivacySandbox.PromptStartupState",
                                  state);
  }
}

base::flat_map<net::SchemefulSite, net::SchemefulSite>
PrivacySandboxServiceImpl::GetSampleRelatedWebsiteSets() const {
  if (privacy_sandbox::kPrivacySandboxFirstPartySetsUISampleSets.Get() &&
      IsRelatedWebsiteSetsDataAccessEnabled()) {
    return {{net::SchemefulSite(GURL("https://youtube.com")),
             net::SchemefulSite(GURL("https://google.com"))},
            {net::SchemefulSite(GURL("https://google.com")),
             net::SchemefulSite(GURL("https://google.com"))},
            {net::SchemefulSite(GURL("https://google.com.au")),
             net::SchemefulSite(GURL("https://google.com"))},
            {net::SchemefulSite(GURL("https://google.de")),
             net::SchemefulSite(GURL("https://google.com"))},
            {net::SchemefulSite(GURL("https://chromium.org")),
             net::SchemefulSite(GURL("https://chromium.org"))},
            {net::SchemefulSite(GURL("https://googlesource.com")),
             net::SchemefulSite(GURL("https://chromium.org"))},
            {net::SchemefulSite(GURL("https://muenchen.de")),
             net::SchemefulSite(GURL("https://xn--mnchen-3ya.de"))}};
  }

  return {};
}

std::optional<net::SchemefulSite>
PrivacySandboxServiceImpl::GetRelatedWebsiteSetOwner(
    const GURL& site_url) const {
  // If RWS is not affecting cookie access, then there are effectively no
  // related website sets.
  if (!(cookie_settings_->ShouldBlockThirdPartyCookies() &&
        cookie_settings_->GetDefaultCookieSetting() != CONTENT_SETTING_BLOCK &&
        base::FeatureList::IsEnabled(
            privacy_sandbox::kPrivacySandboxFirstPartySetsUI))) {
    return std::nullopt;
  }

  // Return the owner according to the sample sets if they're provided.
  if (privacy_sandbox::kPrivacySandboxFirstPartySetsUISampleSets.Get()) {
    const base::flat_map<net::SchemefulSite, net::SchemefulSite> sets =
        GetSampleRelatedWebsiteSets();
    net::SchemefulSite schemeful_site(site_url);

    base::flat_map<net::SchemefulSite, net::SchemefulSite>::const_iterator
        site_entry = sets.find(schemeful_site);
    if (site_entry == sets.end()) {
      return std::nullopt;
    }

    return site_entry->second;
  }

  std::optional<net::FirstPartySetEntry> site_entry =
      first_party_sets_policy_service_->FindEntry(net::SchemefulSite(site_url));
  if (!site_entry.has_value()) {
    return std::nullopt;
  }

  return site_entry->primary();
}

std::optional<std::u16string>
PrivacySandboxServiceImpl::GetRelatedWebsiteSetOwnerForDisplay(
    const GURL& site_url) const {
  std::optional<net::SchemefulSite> site_owner =
      GetRelatedWebsiteSetOwner(site_url);
  if (!site_owner.has_value()) {
    return std::nullopt;
  }

  return url_formatter::IDNToUnicode(site_owner->GetURL().host());
}

bool PrivacySandboxServiceImpl::IsPartOfManagedRelatedWebsiteSet(
    const net::SchemefulSite& site) const {
  if (privacy_sandbox::kPrivacySandboxFirstPartySetsUISampleSets.Get()) {
    return IsRelatedWebsiteSetsDataAccessManaged() ||
           GetSampleRelatedWebsiteSets()[site] ==
               net::SchemefulSite(GURL("https://chromium.org"));
  }

  return first_party_sets_policy_service_->IsSiteInManagedSet(site);
}

void PrivacySandboxServiceImpl::GetFledgeJoiningEtldPlusOneForDisplay(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  if (!interest_group_manager_) {
    std::move(callback).Run({});
    return;
  }

  interest_group_manager_->GetAllInterestGroupDataKeys(base::BindOnce(
      &PrivacySandboxServiceImpl::ConvertInterestGroupDataKeysForDisplay,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

std::vector<std::string>
PrivacySandboxServiceImpl::GetBlockedFledgeJoiningTopFramesForDisplay() const {
  const base::Value::Dict& pref_value =
      pref_service_->GetDict(prefs::kPrivacySandboxFledgeJoinBlocked);

  std::vector<std::string> blocked_top_frames;

  for (auto entry : pref_value) {
    blocked_top_frames.emplace_back(entry.first);
  }

  // Apply a lexographic ordering to match other settings permission surfaces.
  std::sort(blocked_top_frames.begin(), blocked_top_frames.end());

  return blocked_top_frames;
}

void PrivacySandboxServiceImpl::SetFledgeJoiningAllowed(
    const std::string& top_frame_etld_plus1,
    bool allowed) const {
  privacy_sandbox_settings_->SetFledgeJoiningAllowed(top_frame_etld_plus1,
                                                     allowed);

  if (!allowed && browsing_data_remover_) {
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter =
        content::BrowsingDataFilterBuilder::Create(
            content::BrowsingDataFilterBuilder::Mode::kDelete);
    filter->AddRegisterableDomain(top_frame_etld_plus1);
    browsing_data_remover_->RemoveWithFilter(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter));
  }
}

void PrivacySandboxServiceImpl::RecordFirstPartySetsStateHistogram(
    FirstPartySetsState state) {
  base::UmaHistogramEnumeration("Settings.FirstPartySets.State", state);
}

void PrivacySandboxServiceImpl::RecordPrivacySandbox4StartupMetrics() {
  // Record the status of the APIs.
  const bool topics_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled);
  base::UmaHistogramBoolean("Settings.PrivacySandbox.Topics.Enabled",
                            topics_enabled);
  base::UmaHistogramBoolean(
      "Settings.PrivacySandbox.Fledge.Enabled",
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  base::UmaHistogramBoolean(
      "Settings.PrivacySandbox.AdMeasurement.Enabled",
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));

  const bool user_reported_restricted =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1Restricted);
  const bool user_is_currently_unrestricted =
      privacy_sandbox_settings_->IsPrivacySandboxCurrentlyUnrestricted();

  // Prompt suppressed cases.
  PromptSuppressedReason prompt_suppressed_reason =
      static_cast<PromptSuppressedReason>(
          pref_service_->GetInteger(prefs::kPrivacySandboxM1PromptSuppressed));

  switch (prompt_suppressed_reason) {
    // Prompt never suppressed.
    case PromptSuppressedReason::kNone: {
      break;
    }

    case PromptSuppressedReason::kRestricted: {
      RecordPromptStartupStateHistograms(
          PromptStartupState::kPromptNotShownDueToPrivacySandboxRestricted);
      return;
    }

    case PromptSuppressedReason::kThirdPartyCookiesBlocked: {
      RecordPromptStartupStateHistograms(
          PromptStartupState::kPromptNotShownDueTo3PCBlocked);
      return;
    }

    case PromptSuppressedReason::kTrialsConsentDeclined: {
      RecordPromptStartupStateHistograms(
          PromptStartupState::kPromptNotShownDueToTrialConsentDeclined);
      return;
    }

    case PromptSuppressedReason::kTrialsDisabledAfterNotice: {
      RecordPromptStartupStateHistograms(
          PromptStartupState::
              kPromptNotShownDueToTrialsDisabledAfterNoticeShown);
      return;
    }

    case PromptSuppressedReason::kPolicy: {
      RecordPromptStartupStateHistograms(
          PromptStartupState::kPromptNotShownDueToManagedState);
      return;
    }

    case PromptSuppressedReason::kEEAFlowCompletedBeforeRowMigration: {
      RecordPromptStartupStateHistograms(
          topics_enabled
              ? PromptStartupState::kEEAFlowCompletedWithTopicsAccepted
              : PromptStartupState::kEEAFlowCompletedWithTopicsDeclined);
      return;
    }

    case PromptSuppressedReason::
        kROWFlowCompletedAndTopicsDisabledBeforeEEAMigration: {
      RecordPromptStartupStateHistograms(
          PromptStartupState::kROWNoticeFlowCompleted);
      return;
    }

    case PromptSuppressedReason::kNoticeShownToGuardian: {
      // Check for users waiting for graduation: If a user was ever reported as
      // restricted and is currently unrestricted it means they are ready for
      // graduation.
      RecordPromptStartupStateHistograms(
          user_reported_restricted && user_is_currently_unrestricted
              ? PromptStartupState::
                    kWaitingForGraduationRestrictedNoticeFlowNotCompleted
              : PromptStartupState::
                    kRestrictedNoticeNotShownDueToNoticeShownToGuardian);
      return;
    }
  }

  // Prompt was not suppressed explicitly at this point.
  CHECK_EQ(prompt_suppressed_reason, PromptSuppressedReason::kNone);

  // Check if prompt was suppressed implicitly.
  if (IsM1PrivacySandboxEffectivelyManaged(pref_service_)) {
    RecordPromptStartupStateHistograms(
        PromptStartupState::kPromptNotShownDueToManagedState);
    return;
  }

  const bool restricted_notice_acknowledged = pref_service_->GetBoolean(
      prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged);

  // Check for users waiting for graduation: If a user was ever reported as
  // restricted and is currently unrestricted it means they are ready for
  // graduation.
  if (user_reported_restricted && user_is_currently_unrestricted) {
    RecordPromptStartupStateHistograms(
        restricted_notice_acknowledged
            ? PromptStartupState::
                  kWaitingForGraduationRestrictedNoticeFlowCompleted
            : PromptStartupState::
                  kWaitingForGraduationRestrictedNoticeFlowNotCompleted);

    return;
  }

  const bool row_notice_acknowledged =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged);
  const bool eaa_notice_acknowledged =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged);
  // Restricted Notice
  // Note that ordering is important: one of consent or notice will always be
  // required when the restricted prompt is shown, and both return
  // unconditionally.
  if (privacy_sandbox_settings_->IsSubjectToM1NoticeRestricted()) {
    // Acknowledgement of any of the prompt types implies acknowledgement of the
    // restricted notice as well.
    if (row_notice_acknowledged || eaa_notice_acknowledged) {
      RecordPromptStartupStateHistograms(
          PromptStartupState::
              kRestrictedNoticeNotShownDueToFullNoticeAcknowledged);

      return;
    }
    RecordPromptStartupStateHistograms(
        restricted_notice_acknowledged
            ? PromptStartupState::kRestrictedNoticeFlowCompleted
            : PromptStartupState::kRestrictedNoticePromptWaiting);
    return;
  }

  // EEA
  if (privacy_sandbox::IsConsentRequired()) {
    // Consent decision not made
    if (!pref_service_->GetBoolean(
            prefs::kPrivacySandboxM1ConsentDecisionMade)) {
      RecordPromptStartupStateHistograms(
          PromptStartupState::kEEAConsentPromptWaiting);
      return;
    }

    // Consent decision made at this point.

    // Notice Acknowledged
    if (eaa_notice_acknowledged) {
      RecordPromptStartupStateHistograms(
          topics_enabled
              ? PromptStartupState::kEEAFlowCompletedWithTopicsAccepted
              : PromptStartupState::kEEAFlowCompletedWithTopicsDeclined);
    } else {
      RecordPromptStartupStateHistograms(
          PromptStartupState::kEEANoticePromptWaiting);
    }
    return;
  }

  // ROW
  if (privacy_sandbox::IsNoticeRequired()) {
    RecordPromptStartupStateHistograms(
        row_notice_acknowledged ? PromptStartupState::kROWNoticeFlowCompleted
                                : PromptStartupState::kROWNoticePromptWaiting);
    return;
  }
}

void PrivacySandboxServiceImpl::LogPrivacySandboxState() {
  // Do not record metrics for non-regular profiles.
  if (!IsRegularProfile(profile_type_)) {
    return;
  }

  auto rws_status = FirstPartySetsState::kFpsNotRelevant;
  if (cookie_settings_->ShouldBlockThirdPartyCookies() &&
      cookie_settings_->GetDefaultCookieSetting() != CONTENT_SETTING_BLOCK) {
    rws_status = privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled()
                     ? FirstPartySetsState::kFpsEnabled
                     : FirstPartySetsState::kFpsDisabled;
  }
  RecordFirstPartySetsStateHistogram(rws_status);

  RecordPrivacySandbox4StartupMetrics();

  // TODO(crbug.com/333406690): After migration, move this portion to the
  // chrome/browser/privacy_sandbox/privacy_sandbox_notice_service.h constructor
  // and emit ALL startup histograms instead of just Topics consent related
  // histograms.
  for (const auto& notice_name : privacy_sandbox::kPrivacySandboxNoticeNames) {
    notice_storage_->RecordHistogramsOnStartup(pref_service_, notice_name);
  }
}

void PrivacySandboxServiceImpl::ConvertInterestGroupDataKeysForDisplay(
    base::OnceCallback<void(std::vector<std::string>)> callback,
    std::vector<content::InterestGroupManager::InterestGroupDataKey>
        data_keys) {
  std::set<std::string> display_entries;
  for (const auto& data_key : data_keys) {
    // When displaying interest group information in settings, the joining
    // origin is the relevant origin.
    const auto& origin = data_key.joining_origin;

    // Prefer to display the associated eTLD+1, if there is one.
    auto etld_plus_one = net::registry_controlled_domains::GetDomainAndRegistry(
        origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (etld_plus_one.length() > 0) {
      display_entries.emplace(std::move(etld_plus_one));
      RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(true);
      continue;
    }

    // The next best option is a host, which may be an IP address or an eTLD
    // itself (e.g. github.io).
    if (origin.host().length() > 0) {
      display_entries.emplace(origin.host());
      RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(true);
      continue;
    }

    // By design, each interest group should have a joining site or host, and
    // so this could ideally be a NOTREACHED(). However, following
    // crbug.com/1487191, it is apparent that this is not always true.
    // A host or site is expected in other parts of the UI, so we cannot simply
    // display the origin directly (it may also be empty). Instead, we elide it
    // but record a metric to understand how widespread this is.
    // TODO(crbug.com/40283983) - Investigate how much of an issue this is.
    RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(false);
  }

  // Entries should be displayed alphabetically, as |display_entries| is a
  // std::set<std::string>, entries are already ordered correctly.
  std::move(callback).Run(
      std::vector<std::string>{display_entries.begin(), display_entries.end()});
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxServiceImpl::GetCurrentTopTopics() const {
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled) &&
      privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get()) {
    return {fake_current_topics_.begin(), fake_current_topics_.end()};
  }

  if (!browsing_topics_service_) {
    return {};
  }

  auto topics = browsing_topics_service_->GetTopTopicsForDisplay();
  SortAndDeduplicateTopicsForDisplay(topics);

  return topics;
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxServiceImpl::GetBlockedTopics() const {
  if (privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get()) {
    return {fake_blocked_topics_.begin(), fake_blocked_topics_.end()};
  }

  const base::Value::List& pref_value =
      pref_service_->GetList(prefs::kPrivacySandboxBlockedTopics);

  std::vector<privacy_sandbox::CanonicalTopic> blocked_topics;
  for (const auto& entry : pref_value) {
    auto blocked_topic = privacy_sandbox::CanonicalTopic::FromValue(
        *entry.GetDict().Find(kBlockedTopicsTopicKey));
    if (blocked_topic) {
      blocked_topics.emplace_back(*blocked_topic);
    }
  }

  SortAndDeduplicateTopicsForDisplay(blocked_topics);
  return blocked_topics;
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxServiceImpl::GetFirstLevelTopics() const {
  static const base::NoDestructor<std::vector<privacy_sandbox::CanonicalTopic>>
      kFirstLevelTopics([]() -> std::vector<privacy_sandbox::CanonicalTopic> {
        browsing_topics::SemanticTree semantic_tree;

        auto topics = semantic_tree.GetFirstLevelTopicsInCurrentTaxonomy();
        std::vector<privacy_sandbox::CanonicalTopic> first_level_topics;
        first_level_topics.reserve(topics.size());
        std::transform(
            topics.begin(), topics.end(),
            std::back_inserter(first_level_topics),
            [&](const browsing_topics::Topic& topic) {
              return privacy_sandbox::CanonicalTopic(
                  topic, blink::features::kBrowsingTopicsTaxonomyVersion.Get());
            });

        SortAndDeduplicateTopicsForDisplay(first_level_topics);

        return first_level_topics;
      }());

  return *kFirstLevelTopics;
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxServiceImpl::GetChildTopicsCurrentlyAssigned(
    const privacy_sandbox::CanonicalTopic& parent_topic) const {
  browsing_topics::SemanticTree semantic_tree;

  auto descendant_topics =
      semantic_tree.GetDescendantTopics(parent_topic.topic_id());
  auto current_assigned_topics = GetCurrentTopTopics();

  std::set<privacy_sandbox::CanonicalTopic> descendant_topics_set;
  std::transform(
      std::begin(descendant_topics), std::end(descendant_topics),
      std::inserter(descendant_topics_set, descendant_topics_set.begin()),
      [](browsing_topics::Topic topic) {
        return privacy_sandbox::CanonicalTopic(
            topic, blink::features::kBrowsingTopicsTaxonomyVersion.Get());
      });
  std::vector<privacy_sandbox::CanonicalTopic> child_topics_assigned;
  for (const auto topic : current_assigned_topics) {
    if (descendant_topics_set.contains(topic)) {
      child_topics_assigned.push_back(topic);
    }
  }
  return child_topics_assigned;
}

void PrivacySandboxServiceImpl::SetTopicAllowed(
    privacy_sandbox::CanonicalTopic topic,
    bool allowed) {
  if (privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get()) {
    if (allowed) {
      fake_current_topics_.insert(topic);
      fake_blocked_topics_.erase(topic);
    } else {
      fake_current_topics_.erase(topic);
      fake_blocked_topics_.insert(topic);
    }
    return;
  }

  if (!allowed && browsing_topics_service_) {
    browsing_topics_service_->ClearTopic(topic);
  }

  privacy_sandbox_settings_->SetTopicAllowed(topic, allowed);
}

PrivacySandboxCountries*
PrivacySandboxServiceImpl::GetPrivacySandboxCountries() {
  return privacy_sandbox_countries_;
}

bool PrivacySandboxServiceImpl::
    PrivacySandboxPrivacyGuideShouldShowAdTopicsCard() {
  return GetPrivacySandboxCountries()->IsConsentCountry() &&
         base::FeatureList::IsEnabled(
             privacy_sandbox::kPrivacySandboxAdTopicsContentParity);
}

bool PrivacySandboxServiceImpl::ShouldUsePrivacyPolicyChinaDomain() {
  return GetPrivacySandboxCountries()->IsLatestCountryChina();
}

void PrivacySandboxServiceImpl::TopicsToggleChanged(bool new_value) const {
  RecordUpdatedTopicsConsent(
      privacy_sandbox::TopicsConsentUpdateSource::kSettings, new_value);
}

bool PrivacySandboxServiceImpl::TopicsConsentRequired() const {
  return privacy_sandbox::IsConsentRequired();
}

bool PrivacySandboxServiceImpl::TopicsHasActiveConsent() const {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxTopicsConsentGiven);
}

privacy_sandbox::TopicsConsentUpdateSource
PrivacySandboxServiceImpl::TopicsConsentLastUpdateSource() const {
  return static_cast<privacy_sandbox::TopicsConsentUpdateSource>(
      pref_service_->GetInteger(
          prefs::kPrivacySandboxTopicsConsentLastUpdateReason));
}

base::Time PrivacySandboxServiceImpl::TopicsConsentLastUpdateTime() const {
  return pref_service_->GetTime(
      prefs::kPrivacySandboxTopicsConsentLastUpdateTime);
}

std::string PrivacySandboxServiceImpl::TopicsConsentLastUpdateText() const {
  return pref_service_->GetString(
      prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate);
}

void PrivacySandboxServiceImpl::MaybeInitializeRelatedWebsiteSetsPref() {
  // If initialization has already run, it is not required.
  if (pref_service_->GetBoolean(
          prefs::
              kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized)) {
    return;
  }

  // If the FPS UI is not available, no initialization is required.
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxFirstPartySetsUI)) {
    return;
  }

  // If the user blocks 3P cookies, disable the RWS data access preference.
  // As this logic relies on checking synced preference state, it is possible
  // that synced state is available when this decision is made. To err on the
  // side of privacy, this init logic is run per-device (the pref recording that
  // init has been run is not synced). If any of the user's devices local state
  // would disable the pref, it is disabled across all devices.
  if (ShouldBlockThirdPartyOrFirstPartyCookies(cookie_settings_.get())) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                              false);
  }

  pref_service_->SetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized,
      true);
}

void PrivacySandboxServiceImpl::RecordUpdatedTopicsConsent(
    privacy_sandbox::TopicsConsentUpdateSource source,
    bool did_consent) const {
  std::string consent_text;
  switch (source) {
    case privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue: {
      NOTREACHED();
    }
    case privacy_sandbox::TopicsConsentUpdateSource::kConfirmation: {
      consent_text = GetTopicsConfirmationText();
      break;
    }
    case privacy_sandbox::TopicsConsentUpdateSource::kSettings: {
      int current_topics_count = GetCurrentTopTopics().size();
      int blocked_topics_count = GetBlockedTopics().size();
      consent_text = GetTopicsSettingsText(
          did_consent, current_topics_count > 0, blocked_topics_count > 0);
      break;
    }
    default:
      NOTREACHED();
  }

  pref_service_->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven,
                            did_consent);
  pref_service_->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                         base::Time::Now());
  pref_service_->SetInteger(prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
                            static_cast<int>(source));
  pref_service_->SetString(prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate,
                           consent_text);
}

#if !BUILDFLAG(IS_ANDROID)
void PrivacySandboxServiceImpl::MaybeCloseOpenPrompts() {
  // Take a copy to avoid concurrent modification issues as widgets close and
  // remove themselves from the map synchronously. The map will typically have
  // at most a few elements, so this is cheap.
  // It is not possible that a new prompt may be added during this process, as
  // all prompts are created on the same thread, based on information which does
  // not cross task boundaries.
  auto browsers_to_open_prompts_copy = browsers_to_open_prompts_;
  for (const auto& browser_prompt : browsers_to_open_prompts_copy) {
    auto* prompt = browser_prompt.second.get();
    CHECK(prompt);
    prompt->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }

  // After we are done closing the last prompt, release the handle
  MaybeUnqueueNotice(NoticeQueueState::kReleaseOnShown);
}
#endif

std::string GetPromptActionHistogramSuffix(PromptAction action) {
  switch (action) {
    // Notice Actions
    case PromptAction::kNoticeShown:
      return "Notice.Shown";
    case PromptAction::kNoticeOpenSettings:
      return "Notice.OpenedSettings";
    case PromptAction::kNoticeAcknowledge:
      return "Notice.Acknowledged";
    case PromptAction::kNoticeDismiss:
      return "Notice.Dismissed";
    case PromptAction::kNoticeClosedNoInteraction:
      return "Notice.ClosedNoInteraction";
    case PromptAction::kNoticeLearnMore:
      return "Notice.LearnMore";
    case PromptAction::kNoticeMoreInfoOpened:
      return "Notice.LearnMoreExpanded";
    case PromptAction::kNoticeMoreInfoClosed:
      return "Notice.LearnMoreClosed";
    case PromptAction::kNoticeMoreButtonClicked:
      return "Notice.MoreButtonClicked";
    case PromptAction::kNoticeSiteSuggestedAdsMoreInfoOpened:
      return "Notice.SiteSuggestedAdsLearnMoreExpanded";
    case PromptAction::kNoticeSiteSuggestedAdsMoreInfoClosed:
      return "Notice.SiteSuggestedAdsLearnMoreClosed";
    case PromptAction::kNoticeAdsMeasurementMoreInfoOpened:
      return "Notice.AdsMeasurementLearnMoreExpanded";
    case PromptAction::kNoticeAdsMeasurementMoreInfoClosed:
      return "Notice.AdsMeasurementLearnMoreClosed";

    // Consent Actions
    case PromptAction::kConsentShown:
      return "Consent.Shown";
    case PromptAction::kConsentAccepted:
      return "Consent.Accepted";
    case PromptAction::kConsentDeclined:
      return "Consent.Declined";
    case PromptAction::kConsentMoreInfoOpened:
      return "Consent.LearnMoreExpanded";
    case PromptAction::kConsentMoreInfoClosed:
      return "Consent.LearnMoreClosed";
    case PromptAction::kConsentClosedNoDecision:
      return "Consent.ClosedNoInteraction";
    case PromptAction::kConsentMoreButtonClicked:
      return "Consent.MoreButtonClicked";
    case PromptAction::kPrivacyPolicyLinkClicked:
      return "Consent.PrivacyPolicyLinkClicked";

    // Restricted Notice Actions
    case PromptAction::kRestrictedNoticeAcknowledge:
      return "RestrictedNotice.Acknowledged";
    case PromptAction::kRestrictedNoticeOpenSettings:
      return "RestrictedNotice.OpenedSettings";
    case PromptAction::kRestrictedNoticeShown:
      return "RestrictedNotice.Shown";
    case PromptAction::kRestrictedNoticeClosedNoInteraction:
      return "RestrictedNotice.ClosedNoInteraction";
    case PromptAction::kRestrictedNoticeMoreButtonClicked:
      return "RestrictedNotice.MoreButtonClicked";
  }
}

void PrivacySandboxServiceImpl::RecordPromptActionMetrics(PromptAction action) {
  base::RecordAction(base::UserMetricsAction(
      base::StrCat(
          {"Settings.PrivacySandbox.", GetPromptActionHistogramSuffix(action)})
          .c_str()));
}

void PrivacySandboxServiceImpl::OnTopicsPrefChanged() {
  // If the user has disabled the preference, any related data stored should be
  // cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)) {
    return;
  }

  if (browsing_topics_service_) {
    browsing_topics_service_->ClearAllTopicsData();
  }
}

void PrivacySandboxServiceImpl::OnFledgePrefChanged() {
  // If the user has disabled the preference, any related data stored should be
  // cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled)) {
    return;
  }

  if (browsing_data_remover_) {
    browsing_data_remover_->Remove(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS |
            content::BrowsingDataRemover::DATA_TYPE_SHARED_STORAGE |
            content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS_INTERNAL,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  }
}

void PrivacySandboxServiceImpl::OnAdMeasurementPrefChanged() {
  // If the user has disabled the preference, any related data stored should be
  // cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled)) {
    return;
  }

  if (browsing_data_remover_) {
    browsing_data_remover_->Remove(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_ATTRIBUTION_REPORTING |
            content::BrowsingDataRemover::DATA_TYPE_AGGREGATION_SERVICE |
            content::BrowsingDataRemover::
                DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  }
}

// static
bool PrivacySandboxServiceImpl::IsM1PrivacySandboxEffectivelyManaged(
    PrefService* pref_service) {
  bool is_prompt_suppressed_by_policy =
      pref_service->IsManagedPreference(
          prefs::kPrivacySandboxM1PromptSuppressed) &&
      static_cast<int>(PromptSuppressedReason::kPolicy) ==
          pref_service->GetInteger(prefs::kPrivacySandboxM1PromptSuppressed);

  return is_prompt_suppressed_by_policy ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1TopicsEnabled) ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1FledgeEnabled) ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1AdMeasurementEnabled);
}
