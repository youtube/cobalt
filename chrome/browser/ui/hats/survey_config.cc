// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "survey_config.h"

#include <optional>

#include "base/feature_list.h"
#include "base/features.h"
#include "base/strings/string_util.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/lens/lens_features.h"
#include "components/page_info/core/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_hats_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/variations/service/google_groups_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/download_warning_desktop_hats_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"  // nogncheck
#include "components/performance_manager/public/features.h"  // nogncheck
#include "components/permissions/constants.h"                // nogncheck
#include "components/safe_browsing/core/common/features.h"   // nogncheck
#include "components/safe_browsing/core/common/safebrowsing_constants.h"  // nogncheck
#else
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif  // #if !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_COMPOSE)
#include "components/compose/core/browser/compose_features.h"
#endif  // #if !BUILDFLAG(ENABLE_COMPOSE)

#if !BUILDFLAG(IS_ANDROID)
constexpr char kHatsSurveyTriggerAutofillAddress[] = "autofill-address";
constexpr char kHatsSurveyTriggerAutofillAddressUserPerception[] =
    "autofill-address-users-perception";
constexpr char kHatsSurveyTriggerAutofillAddressUserDeclinedSuggestion[] =
    "autofill-address-users-perception";
constexpr char kHatsSurveyTriggerAutofillCreditCardUserPerception[] =
    "autofill-credit-card-users-perception";
constexpr char kHatsSurveyTriggerAutofillPasswordUserPerception[] =
    "autofill-password-users-perception";
constexpr char kHatsSurveyTriggerAutofillCard[] = "autofill-card";
constexpr char kHatsSurveyTriggerAutofillPassword[] = "autofill-password";
constexpr char kHatsSurveyTriggerDownloadWarningBubbleBypass[] =
    "download-warning-bubble-bypass";
constexpr char kHatsSurveyTriggerDownloadWarningBubbleHeed[] =
    "download-warning-bubble-heed";
constexpr char kHatsSurveyTriggerDownloadWarningBubbleIgnore[] =
    "download-warning-bubble-ignore";
constexpr char kHatsSurveyTriggerDownloadWarningPageBypass[] =
    "download-warning-page-bypass";
constexpr char kHatsSurveyTriggerDownloadWarningPageHeed[] =
    "download-warning-page-heed";
constexpr char kHatsSurveyTriggerDownloadWarningPageIgnore[] =
    "download-warning-page-ignore";
constexpr char kHatsSurveyTriggerHistoryEmbeddings[] = "history-embeddings";
constexpr char kHatsSurveyTriggerLensOverlayResults[] = "lens-overlay-results";
constexpr char kHatsSurveyTriggerNtpModules[] = "ntp-modules";
constexpr char kHatsSurveyTriggerNtpPhotosModuleOptOut[] =
    "ntp-photos-module-opt-out";
constexpr char kHatsSurveyTriggerPerformanceControlsPerformance[] =
    "performance-general";
constexpr char kHatsSurveyTriggerPerformanceControlsBatteryPerformance[] =
    "performance-battery";
constexpr char kHatsSurveyTriggerPerformanceControlsMemorySaverOptOut[] =
    "performance-high-efficiency-opt-out";
constexpr char kHatsSurveyTriggerPerformanceControlsBatterySaverOptOut[] =
    "performance-battery-saver-opt-out";
// The permission prompt trigger permits configuring multiple triggers
// simultaneously. Each trigger increments a counter at the end -->
// "permission-prompt0", "permission-prompt1", ...
constexpr char kHatsSurveyTriggerPrivacyGuide[] = "privacy-guide";
constexpr char kHatsSurveyTriggerRedWarning[] = "red-warning";
constexpr char kHatsSurveyTriggerSafetyHubOneOffExperimentControl[] =
    "safety-hub-control";
constexpr char kHatsSurveyTriggerSafetyHubOneOffExperimentNotification[] =
    "safety-hub-notification";
constexpr char kHatsSurveyTriggerSafetyHubOneOffExperimentInteraction[] =
    "safety-hub-interaction";
constexpr char kHatsSurveyTriggerSettings[] = "settings";
constexpr char kHatsSurveyTriggerSettingsPrivacy[] = "settings-privacy";
constexpr char kHatsSurveyTriggerSettingsSecurity[] = "settings-security";
constexpr char kHatsSurveyTriggerTrustSafetyPrivacySettings[] =
    "ts-privacy-settings";
constexpr char kHatsSurveyTriggerTrustSafetyTrustedSurface[] =
    "ts-trusted-surface";
constexpr char kHatsSurveyTriggerTrustSafetyTransactions[] = "ts-transactions";
constexpr char kHatsSurveyTriggerWhatsNew[] = "whats-new";
constexpr char kHatsSurveyTriggerTrustSafetyV2BrowsingData[] =
    "ts-v2-browsing-data";
constexpr char kHatsSurveyTriggerTrustSafetyV2ControlGroup[] =
    "ts-v2-control-group";
constexpr char kHatsSurveyTriggerTrustSafetyV2DownloadWarningUI[] =
    "ts-v2-download-warning-ui";
constexpr char kHatsSurveyTriggerTrustSafetyV2PasswordCheck[] =
    "ts-v2-password-check";
constexpr char kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI[] =
    "ts-v2-password-protection-ui";
constexpr char kHatsSurveyTriggerTrustSafetyV2SafetyCheck[] =
    "ts-v2-safety-check";
constexpr char kHatsSurveyTriggerTrustSafetyV2SafetyHubNotification[] =
    "ts-v2-safety-hub-notification";
constexpr char kHatsSurveyTriggerTrustSafetyV2SafetyHubInteraction[] =
    "ts-v2-safety-hub-interaction";
constexpr char kHatsSurveyTriggerTrustSafetyV2TrustedSurface[] =
    "ts-v2-trusted-surface";
constexpr char kHatsSurveyTriggerTrustSafetyV2PrivacyGuide[] =
    "ts-v2-privacy-guide";
constexpr char kHatsSurveyTriggerTrustSafetyV2SafeBrowsingInterstitial[] =
    "ts-v2-safe-browsing-interstitial";
constexpr char kHatsSurveyTriggerWallpaperSearch[] = "wallpaper-search";

#else   // BUILDFLAG(IS_ANDROID)
constexpr char kHatsSurveyTriggerAndroidStartupSurvey[] = "startup_survey";
constexpr char kHatsSurveyTriggerQuickDelete[] = "quick_delete_survey";
constexpr char kHatsSurveyTriggerClearBrowsingData[] =
    "clear_browsing_data_survey";
constexpr char kHatsSurveyTriggerSafetyHubAndroid[] =
    "safety_hub_android_survey";
constexpr char kHatsSurveyOrganicTriggerSafetyHubAndroid[] =
    "safety_hub_android_organic_survey";
#endif  // #if !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_COMPOSE)
constexpr char kHatsSurveyTriggerComposeAcceptance[] = "compose-acceptance";
constexpr char kHatsSurveyTriggerComposeClose[] = "compose-close";
constexpr char kHatsSurveyTriggerComposeNudgeClose[] = "compose-nudge-close";
#endif  // BUILDFLAG(ENABLE_COMPOSE)

constexpr char kHatsHistogramPrefix[] = "Feedback.HappinessTrackingSurvey.";

constexpr char kHatsSurveyTriggerTesting[] = "testing";
constexpr char kHatsNextSurveyTriggerIDTesting[] =
    "HLpeYy5Av0ugnJ3q1cK0XzzA8UHv";

constexpr char kHatsSurveyTriggerPermissionsPrompt[] = "permissions-prompt";
constexpr char kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate[] =
    "plus-address-accepted-first-time-create";
constexpr char kHatsSurveyTriggerPlusAddressCreatedMultiplePlusAddresses[] =
    "plus-address-created-multiple-plus_addresses";
constexpr char
    kHatsSurveyTriggerPlusAddressCreatedPlusAddressViaManualFallback[] =
        "plus-address-created-plus-address-via-manual-fallback";
constexpr char kHatsSurveyTriggerPlusAddressDeclinedFirstTimeCreate[] =
    "plus-address-declined-first-time-create";
constexpr char
    kHatsSurveyTriggerPlusAddressDidChooseEmailOverPlusAddressSurvey[] =
        "plus-address-did-choose-email-over-plus-address";
constexpr char
    kHatsSurveyTriggerPlusAddressDidChoosePlusAddressOverEmailSurvey[] =
        "plus-address-did-choose-plus-address-over-email";
constexpr char
    kHatsSurveyTriggerPlusAddressFilledPlusAddressViaManualFallback[] =
        "plus-address-filled-plus-address-via-manual-fallback";
constexpr char kHatsSurveyTriggerPrivacySandboxSentimentSurvey[] =
    "privacy-sandbox-sentiment-survey";
constexpr char kHatsSurveyTriggerMerchantTrustEvaluationControlSurvey[] =
    "merchant-trust-evaluation-control-survey";
constexpr char kHatsSurveyTriggerMerchantTrustEvaluationExperimentSurvey[] =
    "merchant-trust-evaluation-experiment-survey";
constexpr char kHatsSurveyTriggerMerchantTrustLearnSurvey[] =
    "merchant-trust-learn-survey";

namespace {

constexpr char kHatsSurveyProbability[] = "probability";
constexpr char kHatsSurveyEnSiteID[] = "en_site_id";
constexpr char kHatsSurveyHistogramName[] = "hats_histogram_name";
constexpr char kHatsSurveyUkmId[] = "hats_survey_ukm_id";
constexpr double kHatsSurveyProbabilityDefault = 0;

// Survey configs must always be hardcoded here, so that they require review
// from HaTS owners. Do not move this method out of the anonymous namespace or
// change its signature to work around this.
std::vector<hats::SurveyConfig> GetAllSurveyConfigs() {
  std::vector<hats::SurveyConfig> survey_configs;

  // Always add the default survey.
  hats::SurveyConfig default_survey;
  default_survey.enabled = true;
  default_survey.probability = 1.0f;
  default_survey.trigger = kHatsSurveyTriggerTesting;
  default_survey.trigger_id = kHatsNextSurveyTriggerIDTesting;
  default_survey.product_specific_bits_data_fields = {"Test Field 1",
                                                      "Test Field 2"};
  default_survey.product_specific_string_data_fields = {"Test Field 3"};
  survey_configs.emplace_back(default_survey);

  // Permission prompt survey
  survey_configs.emplace_back(
      &permissions::features::kPermissionsPromptSurvey,
      kHatsSurveyTriggerPermissionsPrompt,
      /*presupplied_trigger_id=*/std::nullopt,
      std::vector<std::string>{
          permissions::kPermissionsPromptSurveyHadGestureKey},
      std::vector<std::string>{
          permissions::kPermissionsPromptSurveyPromptDispositionKey,
          permissions::kPermissionsPromptSurveyPromptDispositionReasonKey,
          permissions::kPermissionsPromptSurveyActionKey,
          permissions::kPermissionsPromptSurveyRequestTypeKey,
          permissions::kPermissionsPromptSurveyReleaseChannelKey,
          permissions::kPermissionsPromptSurveyDisplayTimeKey,
          permissions::kPermissionPromptSurveyOneTimePromptsDecidedBucketKey,
          permissions::kPermissionPromptSurveyUrlKey,
          permissions::kPermissionPromptSurveyPepcPromptPositionKey,
          permissions::kPermissionPromptSurveyInitialPermissionStatusKey});

  // Privacy sandbox always on sentiment survey
  survey_configs.emplace_back(
      &privacy_sandbox::kPrivacySandboxSentimentSurvey,
      kHatsSurveyTriggerPrivacySandboxSentimentSurvey,
      privacy_sandbox::kPrivacySandboxSentimentSurveyTriggerId.Get(),
      /*product_specific_bits_data_fields=*/
      std::vector<std::string>{"Topics enabled", "Protected audience enabled",
                               "Measurement enabled", "Signed in"},
      /*product_specific_string_data_fields=*/
      std::vector<std::string>{"Channel"},
      /*log_responses_to_uma=*/true,
      /*log_responses_to_ukm=*/true);

#if !BUILDFLAG(IS_ANDROID)
  // Dev tools surveys.
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesCOEP,
                              "devtools-issues-coep",
                              "1DbEs89FS0ugnJ3q1cK0Nx6T99yT");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesMixedContent,
                              "devtools-issues-mixed-content",
                              "BhCYpUmyf0ugnJ3q1cK0VtxCftzo");
  survey_configs.emplace_back(
      &features::
          kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite,
      "devtools-issues-cookies-samesite", "w9JqqpmEr0ugnJ3q1cK0NezVg4iK");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesHeavyAd,
                              "devtools-issues-heavy-ad",
                              "bAeiT5J4P0ugnJ3q1cK0Ra6jg7s8");
  survey_configs.emplace_back(&features::kHaTSDesktopDevToolsIssuesCSP,
                              "devtools-issues-csp",
                              "c9fjDmwjb0ugnJ3q1cK0USeAJJ9C");

  // Settings surveys.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopSettings,
      kHatsSurveyTriggerSettings);
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopSettingsPrivacy,
      kHatsSurveyTriggerSettingsPrivacy,
      /*presupplied_trigger_id=*/std::nullopt,
      std::vector<std::string>{"3P cookies blocked"});
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForSecurityPage,
      kHatsSurveyTriggerSettingsSecurity,
      /*presupplied_trigger_id=*/
      features::kHappinessTrackingSurveysForSecurityPageTriggerId.Get(),
      std::vector<std::string>{},
      std::vector<std::string>{"Security Page User Action",
                               "Safe Browsing Setting Before Trigger",
                               "Safe Browsing Setting After Trigger",
                               "Client Channel", "Time On Page"});
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopPrivacyGuide,
      kHatsSurveyTriggerPrivacyGuide);

  // NTP modules survey.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopNtpModules,
      kHatsSurveyTriggerNtpModules);

  // History embeddings survey.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForHistoryEmbeddings,
      kHatsSurveyTriggerHistoryEmbeddings,
      /*presupplied_trigger_id=*/std::nullopt,
      std::vector<std::string>{"non empty results",
                               "best matches result clicked", "result clicked",
                               "answer shown", "answer citation clicked"},
      std::vector<std::string>{"query word count"});

  // NTP Photos module opt-out survey.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForNtpPhotosOptOut,
      kHatsSurveyTriggerNtpPhotosModuleOptOut);

  // Trust & Safety Sentiment surveys.
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyPrivacySettings,
      features::kTrustSafetySentimentSurveyPrivacySettingsTriggerId.Get(),
      std::vector<std::string>{"Non default setting", "Ran safety check"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyTrustedSurface,
      features::kTrustSafetySentimentSurveyTrustedSurfaceTriggerId.Get(),
      std::vector<std::string>{"Interacted with Page Info"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurvey,
      kHatsSurveyTriggerTrustSafetyTransactions,
      features::kTrustSafetySentimentSurveyTransactionsTriggerId.Get(),
      std::vector<std::string>{"Saved password"});

  // Trust & Safety Sentiment surveys - Version 2.
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2BrowsingData,
      features::kTrustSafetySentimentSurveyV2BrowsingDataTriggerId.Get(),
      std::vector<std::string>{"Deleted history", "Deleted downloads",
                               "Deleted autofill form data"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2ControlGroup,
      features::kTrustSafetySentimentSurveyV2ControlGroupTriggerId.Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2DownloadWarningUI,
      features::kTrustSafetySentimentSurveyV2DownloadWarningUITriggerId.Get(),
      std::vector<std::string>{"Enhanced protection enabled", "Is mainpage UI",
                               "Is subpage UI", "Is downloads page UI",
                               "Is download prompt UI",
                               "User proceeded past warning"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PasswordCheck,
      features::kTrustSafetySentimentSurveyV2PasswordCheckTriggerId.Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI,
      features::kTrustSafetySentimentSurveyV2PasswordProtectionUITriggerId
          .Get(),
      std::vector<std::string>{
          "Enhanced protection enabled", "Is page info UI",
          "Is modal dialog UI", "Is interstitial UI",
          "User completed password change", "User clicked change password",
          "User ignored warning", "User marked as legitimate"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2SafetyCheck,
      features::kTrustSafetySentimentSurveyV2SafetyCheckTriggerId.Get());
  std::vector<std::string> sh_psd_fields{
      "User visited Safety Hub page",
      "User clicked Safety Hub notification",
      "User interacted with Safety Hub",
      "Is notification module extensions",
      "Is notification module notification permissions",
      "Is notification module passwords",
      "Is notification module revoked permissions",
      "Is notification module safe browsing",
      "Global state is safe",
      "Global state is info",
      "Global state is warning",
      "Global state is weak"};
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2SafetyHubInteraction,
      features::kTrustSafetySentimentSurveyV2SafetyHubInteractionTriggerId
          .Get(),
      sh_psd_fields);
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2SafetyHubNotification,
      features::kTrustSafetySentimentSurveyV2SafetyHubNotificationTriggerId
          .Get(),
      sh_psd_fields);
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2TrustedSurface,
      features::kTrustSafetySentimentSurveyV2TrustedSurfaceTriggerId.Get(),
      std::vector<std::string>{"Interacted with Page Info"});
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2PrivacyGuide,
      features::kTrustSafetySentimentSurveyV2PrivacyGuideTriggerId.Get());
  survey_configs.emplace_back(
      &features::kTrustSafetySentimentSurveyV2,
      kHatsSurveyTriggerTrustSafetyV2SafeBrowsingInterstitial,
      features::kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialTriggerId
          .Get(),
      std::vector<std::string>{
          "User proceeded past interstitial", "Enhanced protection enabled",
          "Threat is phishing", "Threat is malware",
          "Threat is unwanted software", "Threat is billing"});

  // Autofill surveys.
  survey_configs.emplace_back(
      &::autofill::features::kAutofillAddressUserPerceptionSurvey,
      kHatsSurveyTriggerAutofillAddressUserPerception, std::nullopt,
      std::vector<std::string>{},
      std::vector<std::string>{
          "Accepted fields", "Corrected to same type",
          "Corrected to a different type", "Corrected to an unknown type",
          "Corrected to empty", "Manually filled to same type",
          "Manually filled to a different type",
          "Manually filled to an unknown type", "Total corrected",
          "Total filled", "Total unfilled", "Total manually filled",
          "Total number of fields"});

  survey_configs.emplace_back(
      &::autofill::features::kAutofillAddressUserDeclinedSuggestionSurvey,
      kHatsSurveyTriggerAutofillAddressUserDeclinedSuggestion, std::nullopt);

  survey_configs.emplace_back(
      &::autofill::features::kAutofillCreditCardUserPerceptionSurvey,
      kHatsSurveyTriggerAutofillCreditCardUserPerception, std::nullopt,
      std::vector<std::string>{},
      std::vector<std::string>{
          "Accepted fields", "Corrected to same type",
          "Corrected to a different type", "Corrected to an unknown type",
          "Corrected to empty", "Manually filled to same type",
          "Manually filled to a different type",
          "Manually filled to an unknown type", "Total corrected",
          "Total filled", "Total unfilled", "Total manually filled",
          "Total number of fields"});
  survey_configs.emplace_back(
      &password_manager::features::kAutofillPasswordUserPerceptionSurvey,
      kHatsSurveyTriggerAutofillPasswordUserPerception, std::nullopt,
      std::vector<std::string>{},
      std::vector<std::string>{"Filling assistance"});
  survey_configs.emplace_back(&features::kAutofillAddressSurvey,
                              kHatsSurveyTriggerAutofillAddress);
  survey_configs.emplace_back(&features::kAutofillCardSurvey,
                              kHatsSurveyTriggerAutofillCard);
  survey_configs.emplace_back(&features::kAutofillPasswordSurvey,
                              kHatsSurveyTriggerAutofillPassword);

  // Wallpaper Search survey.
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForWallpaperSearch,
      kHatsSurveyTriggerWallpaperSearch);

#if BUILDFLAG(ENABLE_COMPOSE)
  // Compose surveys.
  survey_configs.emplace_back(
      &compose::features::kHappinessTrackingSurveysForComposeAcceptance,
      kHatsSurveyTriggerComposeAcceptance,
      /*presupplied_trigger_id=*/std::nullopt,
      std::vector<std::string>{
          "Session used a modifier, like elaborate or formal",
          "A safety filter edited a response in this session",
          "Any error appeared in this session",
          "This session started with nudge"},
      std::vector<std::string>{
          "Execution ID linked to your recent input and page context", "Url",
          "Locale"});

  survey_configs.emplace_back(
      &compose::features::kHappinessTrackingSurveysForComposeClose,
      kHatsSurveyTriggerComposeClose,
      /*presupplied_trigger_id=*/std::nullopt,
      std::vector<std::string>{
          "Session used a modifier, like elaborate or formal",
          "A safety filter edited a response in this session",
          "Any error appeared in this session",
          "This session started with nudge"},
      std::vector<std::string>{
          "Execution ID linked to your recent input and page context", "Url",
          "Locale"});

  survey_configs.emplace_back(
      &compose::features::kHappinessTrackingSurveysForComposeNudgeClose,
      kHatsSurveyTriggerComposeNudgeClose);
#endif  // BUILDFLAG(ENABLE_COMPOSE)

  // What's New survey.2
  survey_configs.emplace_back(
      &features::kHappinessTrackingSurveysForDesktopWhatsNew,
      kHatsSurveyTriggerWhatsNew);

  // Performance Controls surveys.
  survey_configs.emplace_back(
      &performance_manager::features::kPerformanceControlsPerformanceSurvey,
      kHatsSurveyTriggerPerformanceControlsPerformance,
      /*presupplied_trigger_id=*/std::nullopt,
      std::vector<std::string>{"high_efficiency_mode", "battery_saver_mode"},
      std::vector<std::string>{});
  survey_configs.emplace_back(
      &performance_manager::features::
          kPerformanceControlsBatteryPerformanceSurvey,
      kHatsSurveyTriggerPerformanceControlsBatteryPerformance,
      /*presupplied_trigger_id=*/std::nullopt,
      std::vector<std::string>{"high_efficiency_mode", "battery_saver_mode"},
      std::vector<std::string>{});
  survey_configs.emplace_back(
      &performance_manager::features::
          kPerformanceControlsMemorySaverOptOutSurvey,
      kHatsSurveyTriggerPerformanceControlsMemorySaverOptOut);
  survey_configs.emplace_back(
      &performance_manager::features::
          kPerformanceControlsBatterySaverOptOutSurvey,
      kHatsSurveyTriggerPerformanceControlsBatterySaverOptOut);

  // Red Warning surveys.
  survey_configs.emplace_back(
      &safe_browsing::kRedWarningSurvey, kHatsSurveyTriggerRedWarning,
      safe_browsing::kRedWarningSurveyTriggerId.Get(),
      std::vector<std::string>{},
      std::vector<std::string>{
          safe_browsing::kFlaggedUrl, safe_browsing::kMainFrameUrl,
          safe_browsing::kReferrerUrl, safe_browsing::kUserActivityWithUrls});

  // Desktop download warning surveys.
  survey_configs.emplace_back(
      &safe_browsing::kDownloadWarningSurvey,
      kHatsSurveyTriggerDownloadWarningBubbleBypass,
      /*presupplied_trigger_id=*/std::nullopt,
      DownloadWarningHatsProductSpecificData::GetBitsDataFields(
          DownloadWarningHatsType::kDownloadBubbleBypass),
      DownloadWarningHatsProductSpecificData::GetStringDataFields(
          DownloadWarningHatsType::kDownloadBubbleBypass));
  survey_configs.emplace_back(
      &safe_browsing::kDownloadWarningSurvey,
      kHatsSurveyTriggerDownloadWarningBubbleHeed,
      /*presupplied_trigger_id=*/std::nullopt,
      DownloadWarningHatsProductSpecificData::GetBitsDataFields(
          DownloadWarningHatsType::kDownloadBubbleHeed),
      DownloadWarningHatsProductSpecificData::GetStringDataFields(
          DownloadWarningHatsType::kDownloadBubbleHeed));
  survey_configs.emplace_back(
      &safe_browsing::kDownloadWarningSurvey,
      kHatsSurveyTriggerDownloadWarningBubbleIgnore,
      /*presupplied_trigger_id=*/std::nullopt,
      DownloadWarningHatsProductSpecificData::GetBitsDataFields(
          DownloadWarningHatsType::kDownloadBubbleIgnore),
      DownloadWarningHatsProductSpecificData::GetStringDataFields(
          DownloadWarningHatsType::kDownloadBubbleIgnore));
  survey_configs.emplace_back(
      &safe_browsing::kDownloadWarningSurvey,
      kHatsSurveyTriggerDownloadWarningPageBypass,
      /*presupplied_trigger_id=*/std::nullopt,
      DownloadWarningHatsProductSpecificData::GetBitsDataFields(
          DownloadWarningHatsType::kDownloadsPageBypass),
      DownloadWarningHatsProductSpecificData::GetStringDataFields(
          DownloadWarningHatsType::kDownloadsPageBypass));
  survey_configs.emplace_back(
      &safe_browsing::kDownloadWarningSurvey,
      kHatsSurveyTriggerDownloadWarningPageHeed,
      /*presupplied_trigger_id=*/std::nullopt,
      DownloadWarningHatsProductSpecificData::GetBitsDataFields(
          DownloadWarningHatsType::kDownloadsPageHeed),
      DownloadWarningHatsProductSpecificData::GetStringDataFields(
          DownloadWarningHatsType::kDownloadsPageHeed));
  survey_configs.emplace_back(
      &safe_browsing::kDownloadWarningSurvey,
      kHatsSurveyTriggerDownloadWarningPageIgnore,
      /*presupplied_trigger_id=*/std::nullopt,
      DownloadWarningHatsProductSpecificData::GetBitsDataFields(
          DownloadWarningHatsType::kDownloadsPageIgnore),
      DownloadWarningHatsProductSpecificData::GetStringDataFields(
          DownloadWarningHatsType::kDownloadsPageIgnore));

  survey_configs.emplace_back(
      &features::kSafetyHubHaTSOneOffSurvey,
      kHatsSurveyTriggerSafetyHubOneOffExperimentControl,
      features::kHatsSurveyTriggerSafetyHubOneOffExperimentControlTriggerId
          .Get());
  survey_configs.emplace_back(
      &features::kSafetyHubHaTSOneOffSurvey,
      kHatsSurveyTriggerSafetyHubOneOffExperimentNotification,
      features::kHatsSurveyTriggerSafetyHubOneOffExperimentNotificationTriggerId
          .Get(),
      sh_psd_fields);
  survey_configs.emplace_back(
      &features::kSafetyHubHaTSOneOffSurvey,
      kHatsSurveyTriggerSafetyHubOneOffExperimentInteraction,
      features::kHatsSurveyTriggerSafetyHubOneOffExperimentInteractionTriggerId
          .Get(),
      sh_psd_fields);

  // Lens overlay surveys.
  survey_configs.emplace_back(
      &lens::features::kLensOverlaySurvey, kHatsSurveyTriggerLensOverlayResults,
      /*presupplied_trigger_id=*/std::nullopt, std::vector<std::string>{},
      std::vector<std::string>{"ID that's tied to your Google Lens session"});

  // Merchant trust surveys
  survey_configs.emplace_back(
      &page_info::kMerchantTrustEvaluationControlSurvey,
      kHatsSurveyTriggerMerchantTrustEvaluationControlSurvey);

  survey_configs.emplace_back(
      &page_info::kMerchantTrustEvaluationExperimentSurvey,
      kHatsSurveyTriggerMerchantTrustEvaluationExperimentSurvey);

  // The reason for this survey params being set here instead of in a finch
  // config is that our MerchantTrust config has 2 HaTS surveys, one manually
  // triggered and one pop-up (default HaTS behavior), and the finch config only
  // supports one HaTS survey per study group. e.g. There can't be 2
  // features with same param names within the same group, hence we need to set
  // the one of the surveys params here.
  hats::SurveyConfig merchant_trust_learn_survey_config(
      &page_info::kMerchantTrustLearnSurvey,
      kHatsSurveyTriggerMerchantTrustLearnSurvey,
      page_info::kMerchantTrustLearnSurveyTriggerId.Get());
  merchant_trust_learn_survey_config.user_prompted =
      page_info::kMerchantTrustLearnSurveyUserPrompted.Get();
  merchant_trust_learn_survey_config.probability =
      page_info::kMerchantTrustLearnSurveyProbability.Get();
  survey_configs.push_back(merchant_trust_learn_survey_config);
#else
  survey_configs.emplace_back(&chrome::android::kChromeSurveyNextAndroid,
                              kHatsSurveyTriggerAndroidStartupSurvey);

  survey_configs.emplace_back(
      &chrome::android::kQuickDeleteAndroidSurvey,
      kHatsSurveyTriggerQuickDelete,
      chrome::android::kQuickDeleteAndroidSurveyTriggerId.Get());

  survey_configs.emplace_back(&chrome::android::kClearBrowsingDataAndroidSurvey,
                              kHatsSurveyTriggerClearBrowsingData);

  std::vector<std::string> product_specific_bits_data_fields =
      std::vector<std::string>{"Tapped card", "Has visited"};
  std::vector<std::string> product_specific_string_data =
      std::vector<std::string>{"Notification module type", "Global state"};
  survey_configs.emplace_back(
      &features::kSafetyHubAndroidSurvey, kHatsSurveyTriggerSafetyHubAndroid,
      features::kSafetyHubAndroidTriggerId.Get(),
      product_specific_bits_data_fields, product_specific_string_data);
  survey_configs.emplace_back(&features::kSafetyHubAndroidOrganicSurvey,
                              kHatsSurveyOrganicTriggerSafetyHubAndroid,
                              features::kSafetyHubAndroidOrganicTriggerId.Get(),
                              product_specific_bits_data_fields,
                              product_specific_string_data);

  survey_configs.emplace_back(
      &privacy_sandbox::kPrivacySandboxCctAdsNoticeSurvey,
      "privacy-sandbox-cct-ads-notice-eea-control",
      privacy_sandbox::kPrivacySandboxCctAdsNoticeSurveyControlEeaTriggerId
          .Get());

  survey_configs.emplace_back(
      &privacy_sandbox::kPrivacySandboxCctAdsNoticeSurvey,
      "privacy-sandbox-cct-ads-notice-eea-accepted",
      privacy_sandbox::kPrivacySandboxCctAdsNoticeSurveyAcceptedEeaTriggerId
          .Get());

  survey_configs.emplace_back(
      &privacy_sandbox::kPrivacySandboxCctAdsNoticeSurvey,
      "privacy-sandbox-cct-ads-notice-eea-declined",
      privacy_sandbox::kPrivacySandboxCctAdsNoticeSurveyDeclinedEeaTriggerId
          .Get());

  survey_configs.emplace_back(
      &privacy_sandbox::kPrivacySandboxCctAdsNoticeSurvey,
      "privacy-sandbox-cct-ads-notice-row-control",
      privacy_sandbox::kPrivacySandboxCctAdsNoticeSurveyControlRowTriggerId
          .Get());

  survey_configs.emplace_back(
      &privacy_sandbox::kPrivacySandboxCctAdsNoticeSurvey,
      "privacy-sandbox-cct-ads-notice-row-acknowledged",
      privacy_sandbox::kPrivacySandboxCctAdsNoticeSurveyAcknowledgedRowTriggerId
          .Get());

#endif  // #if !BUILDFLAG(IS_ANDROID)

  survey_configs.emplace_back(
      &autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey,
      kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate,
      /*presupplied_trigger_id=*/std::nullopt,
      /*product_specific_bits_data_fields=*/std::vector<std::string>{},
      /*product_specific_string_data_fields=*/
      std::vector<std::string>{
          plus_addresses::hats::kPlusAddressesCount,
          plus_addresses::hats::kFirstPlusAddressCreationTime,
          plus_addresses::hats::kLastPlusAddressFillingTime});
  survey_configs.back().SetCooldownPeriodOverride(base::Days(
      base::FeatureParam<int>(
          &autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey,
          plus_addresses::hats::kCooldownOverrideDays, 0)
          .Get()));

  survey_configs.emplace_back(
      &autofill::features::kPlusAddressDeclinedFirstTimeCreateSurvey,
      kHatsSurveyTriggerPlusAddressDeclinedFirstTimeCreate,
      /*presupplied_trigger_id=*/std::nullopt,
      /*product_specific_bits_data_fields=*/std::vector<std::string>{},
      /*product_specific_string_data_fields=*/
      std::vector<std::string>{
          plus_addresses::hats::kPlusAddressesCount,
          plus_addresses::hats::kFirstPlusAddressCreationTime,
          plus_addresses::hats::kLastPlusAddressFillingTime});
  survey_configs.back().SetCooldownPeriodOverride(base::Days(
      base::FeatureParam<int>(
          &autofill::features::kPlusAddressDeclinedFirstTimeCreateSurvey,
          plus_addresses::hats::kCooldownOverrideDays, 0)
          .Get()));

  survey_configs.emplace_back(
      &autofill::features::kPlusAddressUserCreatedMultiplePlusAddressesSurvey,
      kHatsSurveyTriggerPlusAddressCreatedMultiplePlusAddresses,
      /*presupplied_trigger_id=*/std::nullopt,
      /*product_specific_bits_data_fields=*/std::vector<std::string>{},
      /*product_specific_string_data_fields=*/
      std::vector<std::string>{
          plus_addresses::hats::kPlusAddressesCount,
          plus_addresses::hats::kFirstPlusAddressCreationTime,
          plus_addresses::hats::kLastPlusAddressFillingTime});
  survey_configs.back().SetCooldownPeriodOverride(
      base::Days(base::FeatureParam<int>(
                     &autofill::features::
                         kPlusAddressUserCreatedMultiplePlusAddressesSurvey,
                     plus_addresses::hats::kCooldownOverrideDays, 0)
                     .Get()));

  survey_configs.emplace_back(
      &autofill::features::
          kPlusAddressUserCreatedPlusAddressViaManualFallbackSurvey,
      kHatsSurveyTriggerPlusAddressCreatedPlusAddressViaManualFallback,
      /*presupplied_trigger_id=*/std::nullopt,
      /*product_specific_bits_data_fields=*/std::vector<std::string>{},
      /*product_specific_string_data_fields=*/
      std::vector<std::string>{
          plus_addresses::hats::kPlusAddressesCount,
          plus_addresses::hats::kFirstPlusAddressCreationTime,
          plus_addresses::hats::kLastPlusAddressFillingTime});
  survey_configs.back().SetCooldownPeriodOverride(base::Days(
      base::FeatureParam<int>(
          &autofill::features::
              kPlusAddressUserCreatedPlusAddressViaManualFallbackSurvey,
          plus_addresses::hats::kCooldownOverrideDays, 0)
          .Get()));

  survey_configs.emplace_back(
      &autofill::features::kPlusAddressUserDidChoosePlusAddressOverEmailSurvey,
      kHatsSurveyTriggerPlusAddressDidChoosePlusAddressOverEmailSurvey,
      /*presupplied_trigger_id=*/std::nullopt,
      /*product_specific_bits_data_fields=*/std::vector<std::string>{},
      /*product_specific_string_data_fields=*/
      std::vector<std::string>{
          plus_addresses::hats::kPlusAddressesCount,
          plus_addresses::hats::kFirstPlusAddressCreationTime,
          plus_addresses::hats::kLastPlusAddressFillingTime});
  survey_configs.back().SetCooldownPeriodOverride(
      base::Days(base::FeatureParam<int>(
                     &autofill::features::
                         kPlusAddressUserDidChoosePlusAddressOverEmailSurvey,
                     plus_addresses::hats::kCooldownOverrideDays, 0)
                     .Get()));

  survey_configs.emplace_back(
      &autofill::features::kPlusAddressUserDidChooseEmailOverPlusAddressSurvey,
      kHatsSurveyTriggerPlusAddressDidChooseEmailOverPlusAddressSurvey,
      /*presupplied_trigger_id=*/std::nullopt,
      /*product_specific_bits_data_fields=*/std::vector<std::string>{},
      /*product_specific_string_data_fields=*/
      std::vector<std::string>{
          plus_addresses::hats::kPlusAddressesCount,
          plus_addresses::hats::kFirstPlusAddressCreationTime,
          plus_addresses::hats::kLastPlusAddressFillingTime});
  survey_configs.back().SetCooldownPeriodOverride(
      base::Days(base::FeatureParam<int>(
                     &autofill::features::
                         kPlusAddressUserDidChooseEmailOverPlusAddressSurvey,
                     plus_addresses::hats::kCooldownOverrideDays, 0)
                     .Get()));

  survey_configs.emplace_back(
      &autofill::features::kPlusAddressFilledPlusAddressViaManualFallbackSurvey,
      kHatsSurveyTriggerPlusAddressFilledPlusAddressViaManualFallback,
      /*presupplied_trigger_id=*/std::nullopt,
      /*product_specific_bits_data_fields=*/std::vector<std::string>{},
      /*product_specific_string_data_fields=*/
      std::vector<std::string>{
          plus_addresses::hats::kPlusAddressesCount,
          plus_addresses::hats::kFirstPlusAddressCreationTime,
          plus_addresses::hats::kLastPlusAddressFillingTime});
  survey_configs.back().SetCooldownPeriodOverride(
      base::Days(base::FeatureParam<int>(
                     &autofill::features::
                         kPlusAddressFilledPlusAddressViaManualFallbackSurvey,
                     plus_addresses::hats::kCooldownOverrideDays, 0)
                     .Get()));

  return survey_configs;
}

}  // namespace

namespace hats {

SurveyConfig::SurveyConfig() = default;
SurveyConfig::SurveyConfig(const SurveyConfig&) = default;
SurveyConfig::~SurveyConfig() = default;

SurveyConfig::SurveyConfig(
    const base::Feature* feature,
    const std::string& trigger,
    const std::optional<std::string>& presupplied_trigger_id,
    const std::vector<std::string>& product_specific_bits_data_fields,
    const std::vector<std::string>& product_specific_string_data_fields,
    bool log_responses_to_uma,
    bool log_responses_to_ukm)
    : trigger(trigger),
      product_specific_bits_data_fields(product_specific_bits_data_fields),
      product_specific_string_data_fields(product_specific_string_data_fields),
      survey_feature(feature) {
  enabled = base::FeatureList::IsEnabled(*feature);
  if (!enabled) {
    return;
  }

  probability = base::FeatureParam<double>(feature, kHatsSurveyProbability,
                                           kHatsSurveyProbabilityDefault)
                    .Get();

  // The trigger_id may be provided through the associated feature parameter or
  // may have been included in the source code. The latter is required to enable
  // multiple surveys with a single finch group, as a limitation with finch
  // prevents duplicate param names even for different features within a group.
  // The feature parameter name is "en_site_id" for legacy reasons, as this
  // was the HaTS v1 equivalent of a trigger ID in HaTS Next.
  trigger_id = presupplied_trigger_id ? *presupplied_trigger_id
                                      : base::FeatureParam<std::string>(
                                            feature, kHatsSurveyEnSiteID, "")
                                            .Get();

  if (log_responses_to_uma) {
    hats_histogram_name = ValidateHatsHistogramName(
        base::FeatureParam<std::string>(feature, kHatsSurveyHistogramName, "")
            .Get());
  }

  if (log_responses_to_ukm) {
    hats_survey_ukm_id = ValidateHatsSurveyUkmId(
        base::FeatureParam<int>(feature, kHatsSurveyUkmId, 0).Get());
  }

  user_prompted =
      base::FeatureParam<bool>(feature, "user_prompted", false).Get();
}

// static
std::optional<std::string> SurveyConfig::ValidateHatsHistogramName(
    const std::optional<std::string>& hats_histogram_name) {
  return hats_histogram_name.has_value() &&
                 !hats_histogram_name.value().empty() &&
                 base::StartsWith(hats_histogram_name.value(),
                                  kHatsHistogramPrefix)
             ? hats_histogram_name
             : std::nullopt;
}

// static
std::optional<uint64_t> SurveyConfig::ValidateHatsSurveyUkmId(
    const std::optional<uint64_t> hats_survey_ukm_id) {
  return hats_survey_ukm_id.has_value() && hats_survey_ukm_id.value() > 0
             ? hats_survey_ukm_id
             : std::nullopt;
}

void SurveyConfig::SetCooldownPeriodOverride(
    const base::TimeDelta& cooldown_period_override) {
  if (!cooldown_period_override.is_zero()) {
    cooldown_period_override_ = cooldown_period_override;
  }
}

std::optional<base::TimeDelta> SurveyConfig::GetCooldownPeriodOverride(
    Profile* profile) const {
  if (!cooldown_period_override_) {
    return std::nullopt;
  }

  GoogleGroupsManager* groups_manager =
      GoogleGroupsManagerFactory::GetForBrowserContext(profile);

  if (!groups_manager) {
    return std::nullopt;
  }

  if (!groups_manager->IsFeatureEnabledForProfile(*survey_feature) ||
      !groups_manager->IsFeatureGroupControlled(*survey_feature)) {
    return std::nullopt;
  }

  return cooldown_period_override_;
}

bool SurveyConfig::IsCooldownOverrideEnabled(Profile* profile) const {
  return GetCooldownPeriodOverride(profile).has_value();
}

void GetActiveSurveyConfigs(SurveyConfigs& survey_configs_by_triggers_) {
  auto surveys = GetAllSurveyConfigs();

  // Filter down to active surveys configs and store them in a map for faster
  // access. Triggers within the browser may attempt to show surveys regardless
  // of whether the feature is enabled, so checking whether a particular survey
  // is enabled should be fast.
  for (const SurveyConfig& survey : surveys) {
    if (!survey.enabled) {
      continue;
    }

    survey_configs_by_triggers_.emplace(survey.trigger, survey);
  }
}

}  // namespace hats
