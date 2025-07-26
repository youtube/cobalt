// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_

// clang-format off
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries_impl.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
// clang-format on

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "content/public/browser/interest_group_manager.h"
#include "net/base/schemeful_site.h"

DECLARE_REQUIRED_NOTICE_IDENTIFIER(kPrivacySandboxNotice);

class Browser;
class PrefService;

namespace content {
class BrowsingDataRemover;
}

namespace content_settings {
class CookieSettings;
}

namespace browsing_topics {
class BrowsingTopicsService;
}

namespace views {
class Widget;
}

class PrivacySandboxServiceImpl : public PrivacySandboxService,
                                  public signin::IdentityManager::Observer {
 public:
  PrivacySandboxServiceImpl(
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
      PrivacySandboxCountries* privacy_sandbox_countries);

  ~PrivacySandboxServiceImpl() override;

  // PrivacySandboxService:
  PromptType GetRequiredPromptType(SurfaceType surface_type) override;
  void PromptActionOccurred(PromptAction action,
                            SurfaceType surface_type) override;
#if !BUILDFLAG(IS_ANDROID)
  void PromptOpenedForBrowser(Browser* browser, views::Widget* widget) override;
  void PromptClosedForBrowser(Browser* browser) override;
  bool IsPromptOpenForBrowser(Browser* browser) override;
  void HoldQueueHandle(user_education::RequiredNoticePriorityHandle
                           messaging_priority_handle) override;
  bool IsNoticeQueued() override;
  void MaybeUnqueueNotice(NoticeQueueState unqueue_source) override;
  void MaybeQueueNotice(NoticeQueueState queue_source) override;
  bool IsHoldingHandle() override;
  void SetSuppressQueue(bool suppress_queue) override;
#endif  // !BUILDFLAG(IS_ANDROID)
  void ForceChromeBuildForTests(bool force_chrome_build) override;
  bool IsPrivacySandboxRestricted() override;
  bool IsRestrictedNoticeEnabled() override;
  void SetRelatedWebsiteSetsDataAccessEnabled(bool enabled) override;
  bool IsRelatedWebsiteSetsDataAccessEnabled() const override;
  bool IsRelatedWebsiteSetsDataAccessManaged() const override;
  base::flat_map<net::SchemefulSite, net::SchemefulSite>
  GetSampleRelatedWebsiteSets() const override;
  std::optional<net::SchemefulSite> GetRelatedWebsiteSetOwner(
      const GURL& site_url) const override;
  std::optional<std::u16string> GetRelatedWebsiteSetOwnerForDisplay(
      const GURL& site_url) const override;
  bool IsPartOfManagedRelatedWebsiteSet(
      const net::SchemefulSite& site) const override;
  void GetFledgeJoiningEtldPlusOneForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback) override;
  std::vector<std::string> GetBlockedFledgeJoiningTopFramesForDisplay()
      const override;
  void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                               bool allowed) const override;
  std::vector<privacy_sandbox::CanonicalTopic> GetCurrentTopTopics()
      const override;
  std::vector<privacy_sandbox::CanonicalTopic> GetBlockedTopics()
      const override;
  std::vector<privacy_sandbox::CanonicalTopic> GetFirstLevelTopics()
      const override;
  std::vector<privacy_sandbox::CanonicalTopic> GetChildTopicsCurrentlyAssigned(
      const privacy_sandbox::CanonicalTopic& topic) const override;
  void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                       bool allowed) override;
  bool PrivacySandboxPrivacyGuideShouldShowAdTopicsCard() override;
  bool ShouldUsePrivacyPolicyChinaDomain() override;
  void TopicsToggleChanged(bool new_value) const override;
  bool TopicsConsentRequired() const override;
  bool TopicsHasActiveConsent() const override;
  privacy_sandbox::TopicsConsentUpdateSource TopicsConsentLastUpdateSource()
      const override;
  base::Time TopicsConsentLastUpdateTime() const override;
  std::string TopicsConsentLastUpdateText() const override;

  // signin::IdentityManager::Observer
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

 protected:
  friend class PrivacySandboxServiceTest;
  friend class PrivacySandboxQueueTestNoticeWithSearchEngine;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           MetricsLoggingOccursCorrectly);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestNonRegularProfile,
                           NoMetricsRecorded);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServicePromptTest, RestrictedPrompt);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServicePromptTest, ManagedNoPrompt);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServicePromptTest,
                           ManuallyControlledNoPrompt);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServicePromptTest, NoParamNoPrompt);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceDeathTest,
                           GetRequiredPromptType);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxPromptNoticeWaiting);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxPromptConsentWaiting);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxV1OffEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxV1OffDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxConsentEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxConsentDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoticeEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoticeDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandbox3PCOffEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandbox3PCOffDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxManagedEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxManagedDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxManuallyControlledEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxManuallyControlledDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoPromptDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoPromptEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest, PrivacySandboxRestricted);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsNotRelevantMetricAllowedCookies);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsNotRelevantMetricBlockedCookies);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsEnabledMetric);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsDisabledMetric);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceTest,
      RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Explicitly);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceTest,
      RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Implicitly);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceTest,
      RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_EEA);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceTest,
      RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_ROW);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RecordPrivacySandbox4StartupMetrics_APIs);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
                           ReturnsCorrectStatus);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1RestrictedNoticePromptTest,
      RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted,
      RecordPrivacySandbox4StartupMetrics_GraduationFlow);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyRestricted,
      RecordPrivacySandbox4StartupMetrics_GraduationFlow);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted,
      RecordPrivacySandbox4StartupMetrics_GraduationFlowWhenNoticeShownToGuardian);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxQueueTestNoticeWithSearchEngine,
                           PromptSuppressed);

  // Contains all possible privacy sandbox states, recorded on startup.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Must be kept in sync with the SettingsPrivacySandboxEnabled enum in
  // histograms/enums.xml.
  enum class SettingsPrivacySandboxEnabled {
    kPSEnabledAllowAll = 0,
    kPSEnabledBlock3P = 1,
    kPSEnabledBlockAll = 2,
    kPSDisabledAllowAll = 3,
    kPSDisabledBlock3P = 4,
    kPSDisabledBlockAll = 5,
    kPSDisabledPolicyBlock3P = 6,
    kPSDisabledPolicyBlockAll = 7,
    // DEPRECATED
    kPSEnabledFlocDisabledAllowAll = 8,
    // DEPRECATED
    kPSEnabledFlocDisabledBlock3P = 9,
    // DEPRECATED
    kPSEnabledFlocDisabledBlockAll = 10,
    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kPSEnabledFlocDisabledBlockAll,
  };

  // Contains all possible states of first party sets preference.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Must be kept in sync with the FirstPartySetsState enum in
  // histograms/enums.xml.
  enum class FirstPartySetsState {
    // The user allows all cookies, or blocks all cookies.
    kFpsNotRelevant = 0,
    // The user blocks third-party cookies, and has FPS enabled.
    kFpsEnabled = 1,
    // The user blocks third-party cookies, and has FPS disabled.
    kFpsDisabled = 2,
    kMaxValue = kFpsDisabled,
  };

  // Contains the possible states of a users Privacy Sandbox overall settings.
  // Must be kept in sync with SettingsPrivacySandboxStartupStates in
  // histograms/enums.xml
  enum class PSStartupStates {
    kPromptWaiting = 0,
    kPromptOffV1OffEnabled = 1,
    kPromptOffV1OffDisabled = 2,
    kConsentShownEnabled = 3,
    kConsentShownDisabled = 4,
    kNoticeShownEnabled = 5,
    kNoticeShownDisabled = 6,
    kPromptOff3PCOffEnabled = 7,
    kPromptOff3PCOffDisabled = 8,
    kPromptOffManagedEnabled = 9,
    kPromptOffManagedDisabled = 10,
    kPromptOffRestricted = 11,
    kPromptOffManuallyControlledEnabled = 12,
    kPromptOffManuallyControlledDisabled = 13,
    kNoPromptRequiredEnabled = 14,
    kNoPromptRequiredDisabled = 15,

    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kNoPromptRequiredDisabled,
  };

  // Helper function to log first party sets state.
  void RecordFirstPartySetsStateHistogram(FirstPartySetsState state);

  // Logs the state of the privacy sandbox and cookie settings. Called once per
  // profile startup.
  void LogPrivacySandboxState();

  // Logs the state of privacy sandbox 4 in regards to prompts. Called once per
  // profile startup.
  void RecordPrivacySandbox4StartupMetrics();

  // Converts the provided list of |top_frames| into eTLD+1s for display, and
  // provides those to |callback|.
  void ConvertInterestGroupDataKeysForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback,
      std::vector<content::InterestGroupManager::InterestGroupDataKey>
          data_keys);

  // Checks to see if initialization of the user's RWS pref is required, and if
  // so, sets the default value based on the user's current cookie settings.
  void MaybeInitializeRelatedWebsiteSetsPref();

  // Updates the preferences which store the current Topics consent information.
  void RecordUpdatedTopicsConsent(
      privacy_sandbox::TopicsConsentUpdateSource source,
      bool did_consent) const;

#if !BUILDFLAG(IS_ANDROID)
  // If appropriate based on feature state, closes all currently open Privacy
  // Sandbox prompts.
  void MaybeCloseOpenPrompts();
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<content::InterestGroupManager> interest_group_manager_;
  profile_metrics::BrowserProfileType profile_type_;
  std::unique_ptr<privacy_sandbox::PrivacySandboxNoticeStorage> notice_storage_;
  raw_ptr<content::BrowsingDataRemover> browsing_data_remover_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  raw_ptr<browsing_topics::BrowsingTopicsService> browsing_topics_service_;
  raw_ptr<first_party_sets::FirstPartySetsPolicyService>
      first_party_sets_policy_service_;
  raw_ptr<user_education::ProductMessagingController>
      product_messaging_controller_;
  raw_ptr<PrivacySandboxCountries> privacy_sandbox_countries_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_obs_{this};
  raw_ptr<signin::IdentityManager> identity_manager_;
  PrimaryAccountUserGroups primary_account_state_ =
      PrimaryAccountUserGroups::kNotSet;
  // Stores bitmaps for prompt suppression, 0 is not suppressed. This variable
  // stores information about the dark launch notice that uses the non-synced
  // pref.
  int prompt_suppression_bitmap_ = 0;
  // Stores bitmaps for prompt suppression, 0 is not suppressed. This variable
  // stores information about the dark launch notice that uses the synced pref.
  int prompt_suppression_bitmap_sync_ = 0;

  PrefChangeRegistrar user_prefs_registrar_;

  user_education::RequiredNoticePriorityHandle notice_handle_;

#if !BUILDFLAG(IS_ANDROID)
  // A map of Browser windows which have an open Privacy Sandbox prompt,
  // to the Widget for that prompt.
  std::map<Browser*, raw_ptr<views::Widget, CtnExperimental>>
      browsers_to_open_prompts_;

  // Returns instance of product messaging controller.
  user_education::ProductMessagingController* GetProductMessagingController();
#endif

  // Fake implementation for current and blocked topics.
  static constexpr int kFakeTaxonomyVersion = 1;
  std::set<privacy_sandbox::CanonicalTopic> fake_current_topics_ = {
      {browsing_topics::Topic(1), kFakeTaxonomyVersion},
      {browsing_topics::Topic(2), kFakeTaxonomyVersion}};
  std::set<privacy_sandbox::CanonicalTopic> fake_blocked_topics_ = {
      {browsing_topics::Topic(3), kFakeTaxonomyVersion},
      {browsing_topics::Topic(4), kFakeTaxonomyVersion}};

  // Record user action metrics based on the |action|.
  void RecordPromptActionMetrics(PrivacySandboxService::PromptAction action);

  // Record user startup state metrics on both client and profile level.
  void RecordPromptStartupStateHistograms(
      PrivacySandboxService::PromptStartupState state);

  // Called when the Topics preference is changed.
  void OnTopicsPrefChanged();

  // Called when the Fledge preference is changed.
  void OnFledgePrefChanged();

  // Called when the Ad measurement preference is changed.
  void OnAdMeasurementPrefChanged();

  // Called on Startup to initialize the IdentityManager observation +
  // histograms.
  void MaybeInitIdentityManager();

  // Returns a PrivacySandboxCountries reference.
  PrivacySandboxCountries* GetPrivacySandboxCountries();

  // Sets member variable primary_account_state_
  void SetPrimaryAccountState(PrimaryAccountUserGroups user_group_to_set);

  // Returns true if _any_ of the k-API prefs are disabled via policy or
  // the prompt was suppressed via policy.
  static bool IsM1PrivacySandboxEffectivelyManaged(PrefService* pref_service);

  // Emits startup histograms relating to the user's sign in status.
  void MaybeEmitPromptStartupAccountMetrics();

  // Emits histograms relating to a fake notice's shown or suppression status.
  void MaybeEmitFakeNoticePromptMetrics(bool third_party_cookies_blocked);

  bool force_chrome_build_for_tests_ = false;
  bool should_emit_dark_launch_startup_metrics_ = true;
  // Temporary flag signifying not to requeue if the prompt has been suppressed.
  // TODO(crbug.com/370804492): When we add DMA notice to queue, remove this.
  bool suppress_queue_ = false;

  base::WeakPtrFactory<PrivacySandboxServiceImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_
