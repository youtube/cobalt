// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/hats_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace {

// Generate the Product Specific bits data which accompanies privacy settings
// survey responses from |profile|.
SurveyBitsData GetPrivacySettingsProductSpecificBitsData(Profile* profile) {
  const bool third_party_cookies_blocked =
      static_cast<content_settings::CookieControlsMode>(
          profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode)) ==
      content_settings::CookieControlsMode::kBlockThirdParty;
  const bool privacy_sandbox_enabled =
      PrivacySandboxSettingsFactory::GetForProfile(profile)
          ->IsPrivacySandboxEnabled();

  return {{"3P cookies blocked", third_party_cookies_blocked},
          {"Privacy Sandbox enabled", privacy_sandbox_enabled}};
}

// Generate the Product Specific bits data which accompanies M1 Ad Privacy
// survey responses from |profile|.
SurveyBitsData GetAdPrivacyProductSpecificBitsData(Profile* profile) {
  const bool third_party_cookies_blocked =
      static_cast<content_settings::CookieControlsMode>(
          profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode)) ==
      content_settings::CookieControlsMode::kBlockThirdParty;

  return {
      {"3P cookies blocked", third_party_cookies_blocked},
      {"Topics enabled",
       profile->GetPrefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)},
      {"Fledge enabled",
       profile->GetPrefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled)},
      {"Ad Measurement enabled",
       profile->GetPrefs()->GetBoolean(
           prefs::kPrivacySandboxM1AdMeasurementEnabled)},
  };
}
}  // namespace

namespace settings {

HatsHandler::HatsHandler() = default;

HatsHandler::~HatsHandler() = default;

void HatsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "trustSafetyInteractionOccurred",
      base::BindRepeating(&HatsHandler::HandleTrustSafetyInteractionOccurred,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "securityPageInteractionOccurred",
      base::BindRepeating(&HatsHandler::HandleSecurityPageInteractionOccurred,
                          base::Unretained(this)));
}

/**
 * First arg in the list indicates the SecurityPageInteraction.
 * Second arg in the list indicates the SafeBrowsingSetting.
 */
void HatsHandler::HandleSecurityPageInteractionOccurred(
    const base::Value::List& args) {
  AllowJavascript();

  // There are 2 argument in the input list.
  // The first one is the SecurityPageInteraction that triggered the survey.
  // The second one is the safe browsing setting the user was on.
  CHECK_EQ(2U, args.size());

  Profile* profile = Profile::FromWebUI(web_ui());

  // Enterprise users consideration.
  // If the admin disabled the survey, the survey will not be requested.
  if (!safe_browsing::IsSafeBrowsingSurveysEnabled(*profile->GetPrefs())) {
    return;
  }

  // Request HaTS survey.
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      profile, /* create_if_necessary = */ true);

  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service) {
    return;
  }

  // Generate the Product Specific bits data from |profile| and |args|.
  SurveyStringData product_specific_string_data =
      GetSecurityPageProductSpecificStringData(profile, args);

  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettingsSecurity, web_ui()->GetWebContents(),
      features::kHappinessTrackingSurveysForSecurityPageTime.Get()
          .InMilliseconds(),
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/product_specific_string_data,
      /*require_same_origin=*/true);
}

/**
 * Generate the Product Specific string data from |profile| and |args|.
 * - First arg in the list indicates the SecurityPageInteraction.
 * - Second arg in the list indicates the SafeBrowsingSetting.
 */
SurveyStringData HatsHandler::GetSecurityPageProductSpecificStringData(
    Profile* profile,
    const base::Value::List& args) {
  auto interaction = static_cast<SecurityPageInteraction>(args[0].GetInt());
  auto safe_browsing_setting =
      static_cast<SafeBrowsingSetting>(args[1].GetInt());

  std::string security_page_interaction_type = "";
  std::string safe_browsing_setting_before = "";
  std::string safe_browsing_setting_current = "";

  switch (interaction) {
    case SecurityPageInteraction::RADIO_BUTTON_ENHANCED_CLICK: {
      security_page_interaction_type =
          "enhanced_protection_radio_button_clicked";
      break;
    }
    case SecurityPageInteraction::RADIO_BUTTON_STANDARD_CLICK: {
      security_page_interaction_type =
          "standard_protection_radio_button_clicked";
      break;
    }
    case SecurityPageInteraction::RADIO_BUTTON_DISABLE_CLICK: {
      security_page_interaction_type = "no_protection_radio_button_clicked";
      break;
    }
    case SecurityPageInteraction::EXPAND_BUTTON_ENHANCED_CLICK: {
      security_page_interaction_type =
          "enhanced_protection_expand_button_clicked.";
      break;
    }
    case SecurityPageInteraction::EXPAND_BUTTON_STANDARD_CLICK: {
      security_page_interaction_type =
          "standard_protection_expand_button_clicked.";
      break;
    }
  }

  switch (safe_browsing_setting) {
    case SafeBrowsingSetting::ENHANCED: {
      safe_browsing_setting_before = "enhanced_protection";
      break;
    }
    case SafeBrowsingSetting::STANDARD: {
      safe_browsing_setting_before = "standard_protection";
      break;
    }
    case SafeBrowsingSetting::DISABLED: {
      safe_browsing_setting_before = "no_protection";
      break;
    }
  }

  bool safe_browsing_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
  bool safe_browsing_enhanced_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnhanced);
  if (safe_browsing_enhanced_enabled) {
    safe_browsing_setting_current = "enhanced_protection";
  } else if (safe_browsing_enabled) {
    safe_browsing_setting_current = "standard_protection";
  } else {
    safe_browsing_setting_current = "no_protection";
  }

  std::string client_channel =
      std::string(version_info::GetChannelString(chrome::GetChannel()));

  return {
      {"Security Page User Action", security_page_interaction_type},
      {"Safe Browsing Setting Before Trigger", safe_browsing_setting_before},
      {"Safe Browsing Setting After Trigger", safe_browsing_setting_current},
      {"Client Channel", client_channel},
  };
}

void HatsHandler::HandleTrustSafetyInteractionOccurred(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  auto interaction = static_cast<TrustSafetyInteraction>(args[0].GetInt());

  // Both the HaTS service, and the T&S sentiment service (which is another
  // wrapper on the HaTS service), may decide to launch surveys based on this
  // user interaction. The HaTS service is responsible for ensuring that users
  // are not over-surveyed, and that other potential issues such as simultaneous
  // surveys are avoided.
  RequestHatsSurvey(interaction);
  InformSentimentService(interaction);
}

void HatsHandler::RequestHatsSurvey(TrustSafetyInteraction interaction) {
  Profile* profile = Profile::FromWebUI(web_ui());
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      profile, /* create_if_necessary = */ true);

  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service)
    return;

  std::string trigger = "";
  int timeout_ms = 0;
  SurveyBitsData product_specific_bits_data = {};
  bool require_same_origin = false;

  switch (interaction) {
    case TrustSafetyInteraction::OPENED_PRIVACY_SANDBOX: {
      trigger = kHatsSurveyTriggerPrivacySandbox;
      timeout_ms =
          features::kHappinessTrackingSurveysForDesktopPrivacySandboxTime.Get()
              .InMilliseconds();
      product_specific_bits_data =
          GetPrivacySettingsProductSpecificBitsData(profile);
      require_same_origin = true;
      break;
    }
    case TrustSafetyInteraction::RAN_SAFETY_CHECK:
      [[fallthrough]];
    case TrustSafetyInteraction::USED_PRIVACY_CARD: {
      // The control group for the Privacy guide HaTS experiment will need to
      // see either safety check or the privacy page to be eligible and have
      // never seen privacy guide.
      if (features::kHappinessTrackingSurveysForDesktopSettingsPrivacyNoGuide
              .Get() &&
          Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
              prefs::kPrivacyGuideViewed)) {
        return;
      }
      // If the privacy settings survey is explicitly targeting users who have
      // not viewed the Privacy Sandbox page, and this user has viewed the page,
      // do not attempt to show the privacy settings survey.
      if (features::kHappinessTrackingSurveysForDesktopSettingsPrivacyNoSandbox
              .Get() &&
          Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
              prefs::kPrivacySandboxPageViewed)) {
        return;
      }
      trigger = kHatsSurveyTriggerSettingsPrivacy;
      timeout_ms =
          features::kHappinessTrackingSurveysForDesktopSettingsPrivacyTime.Get()
              .InMilliseconds();
      product_specific_bits_data =
          GetPrivacySettingsProductSpecificBitsData(profile);
      require_same_origin = true;
      break;
    }
    case TrustSafetyInteraction::COMPLETED_PRIVACY_GUIDE: {
      trigger = kHatsSurveyTriggerPrivacyGuide;
      timeout_ms =
          features::kHappinessTrackingSurveysForDesktopPrivacyGuideTime.Get()
              .InMilliseconds();
      require_same_origin = true;
      break;
    }
    case TrustSafetyInteraction::OPENED_AD_PRIVACY: {
      trigger = kHatsSurveyTriggerM1AdPrivacyPage;
      timeout_ms =
          features::kHappinessTrackingSurveysForDesktopM1AdPrivacyPageTime.Get()
              .InMilliseconds();
      require_same_origin = true;
      product_specific_bits_data = GetAdPrivacyProductSpecificBitsData(profile);
      break;
    }
    case TrustSafetyInteraction::OPENED_TOPICS_SUBPAGE: {
      trigger = kHatsSurveyTriggerM1TopicsSubpage;
      timeout_ms =
          features::kHappinessTrackingSurveysForDesktopM1TopicsSubpageTime.Get()
              .InMilliseconds();
      require_same_origin = true;
      product_specific_bits_data = GetAdPrivacyProductSpecificBitsData(profile);
      break;
    }
    case TrustSafetyInteraction::OPENED_FLEDGE_SUBPAGE: {
      trigger = kHatsSurveyTriggerM1FledgeSubpage;
      timeout_ms =
          features::kHappinessTrackingSurveysForDesktopM1FledgeSubpageTime.Get()
              .InMilliseconds();
      require_same_origin = true;
      product_specific_bits_data = GetAdPrivacyProductSpecificBitsData(profile);
      break;
    }
    case TrustSafetyInteraction::OPENED_AD_MEASUREMENT_SUBPAGE: {
      trigger = kHatsSurveyTriggerM1AdMeasurementSubpage;
      timeout_ms =
          features::
              kHappinessTrackingSurveysForDesktopM1AdMeasurementSubpageTime
                  .Get()
                  .InMilliseconds();
      require_same_origin = true;
      product_specific_bits_data = GetAdPrivacyProductSpecificBitsData(profile);
      break;
    }
    case TrustSafetyInteraction::OPENED_PASSWORD_MANAGER:
      [[fallthrough]];
    case TrustSafetyInteraction::RAN_PASSWORD_CHECK: {
      // Only relevant for the sentiment service
      return;
    }
  }
  // If we haven't returned, a trigger must have been set in the switch above.
  CHECK_NE(trigger, "");

  hats_service->LaunchDelayedSurveyForWebContents(
      trigger, web_ui()->GetWebContents(), timeout_ms,
      product_specific_bits_data,
      /*product_specific_string_data=*/{}, require_same_origin);
}

void HatsHandler::InformSentimentService(TrustSafetyInteraction interaction) {
  auto* sentiment_service = TrustSafetySentimentServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  if (!sentiment_service)
    return;

  if (interaction == TrustSafetyInteraction::USED_PRIVACY_CARD) {
    sentiment_service->InteractedWithPrivacySettings(
        web_ui()->GetWebContents());
  } else if (interaction == TrustSafetyInteraction::RAN_SAFETY_CHECK) {
    sentiment_service->RanSafetyCheck();
  } else if (interaction == TrustSafetyInteraction::OPENED_PASSWORD_MANAGER) {
    sentiment_service->OpenedPasswordManager(web_ui()->GetWebContents());
  } else if (interaction == TrustSafetyInteraction::RAN_PASSWORD_CHECK) {
    sentiment_service->RanPasswordCheck();
  } else if (interaction == TrustSafetyInteraction::COMPLETED_PRIVACY_GUIDE) {
    sentiment_service->FinishedPrivacyGuide();
  }
}

}  // namespace settings
