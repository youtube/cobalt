// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include "base/containers/cxx20_erase.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/unified_consent/pref_names.h"
#include "components/version_info/channel.h"

namespace {

base::TimeDelta GetMinTimeToPrompt() {
  return base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
             ? features::kTrustSafetySentimentSurveyV2MinTimeToPrompt.Get()
             : features::kTrustSafetySentimentSurveyMinTimeToPrompt.Get();
}

base::TimeDelta GetMaxTimeToPrompt() {
  return base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
             ? features::kTrustSafetySentimentSurveyV2MaxTimeToPrompt.Get()
             : features::kTrustSafetySentimentSurveyMaxTimeToPrompt.Get();
}

base::TimeDelta GetMinSessionTime() {
  DCHECK(base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2));
  return features::kTrustSafetySentimentSurveyV2MinSessionTime.Get();
}

int GetRequiredNtpCount() {
  return base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
             ? base::RandInt(
                   features::kTrustSafetySentimentSurveyV2NtpVisitsMinRange
                       .Get(),
                   features::kTrustSafetySentimentSurveyV2NtpVisitsMaxRange
                       .Get())
             : base::RandInt(
                   features::kTrustSafetySentimentSurveyNtpVisitsMinRange.Get(),
                   features::kTrustSafetySentimentSurveyNtpVisitsMaxRange
                       .Get());
}

int GetMaxRequiredNtpCount() {
  return base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
             ? features::kTrustSafetySentimentSurveyV2NtpVisitsMaxRange.Get()
             : features::kTrustSafetySentimentSurveyNtpVisitsMaxRange.Get();
}

bool HasNonDefaultPrivacySetting(Profile* profile) {
  auto* prefs = profile->GetPrefs();

  std::vector<std::string> prefs_to_check = {
      prefs::kSafeBrowsingEnabled,
      prefs::kSafeBrowsingEnhanced,
      prefs::kSafeBrowsingScoutReportingEnabled,
      prefs::kEnableDoNotTrack,
      password_manager::prefs::kPasswordLeakDetectionEnabled,
      prefs::kCookieControlsMode,
  };

  bool has_non_default_pref = false;
  for (const auto& pref_name : prefs_to_check) {
    auto* pref = prefs->FindPreference(pref_name);
    if (!pref->IsDefaultValue() && pref->IsUserControlled()) {
      has_non_default_pref = true;
      break;
    }
  }

  // Users consenting to sync automatically enable UKM collection
  auto* ukm_pref = prefs->FindPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  auto* sync_consent_pref =
      prefs->FindPreference(prefs::kGoogleServicesConsentedToSync);

  bool has_non_default_ukm =
      ukm_pref->GetValue()->GetBool() !=
          sync_consent_pref->GetValue()->GetBool() &&
      (ukm_pref->IsUserControlled() || sync_consent_pref->IsUserControlled());

  // Check the default value for each user facing content setting. Note that
  // this will not include content setting exceptions set via permission
  // prompts, as they are site specific.
  bool has_non_default_content_setting = false;
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);

  for (auto content_setting_type :
       site_settings::GetVisiblePermissionCategories()) {
    std::string content_setting_provider;
    auto current_value = map->GetDefaultContentSetting(
        content_setting_type, &content_setting_provider);
    auto content_setting_source =
        HostContentSettingsMap::GetSettingSourceFromProviderName(
            content_setting_provider);

    const bool user_controlled =
        content_setting_source ==
            content_settings::SettingSource::SETTING_SOURCE_NONE ||
        content_setting_source ==
            content_settings::SettingSource::SETTING_SOURCE_USER;

    auto default_value = static_cast<ContentSetting>(
        content_settings::WebsiteSettingsRegistry::GetInstance()
            ->Get(content_setting_type)
            ->initial_default_value()
            .GetInt());

    if (current_value != default_value && user_controlled) {
      has_non_default_content_setting = true;
      break;
    }
  }

  return has_non_default_pref || has_non_default_ukm ||
         has_non_default_content_setting;
}

// Generates the Product Specific Data which accompanies survey results for the
// Privacy Settings product area. This includes whether the user is receiving
// the survey because they ran safety check, and whether they have any
// non-default core privacy settings.
std::map<std::string, bool> GetPrivacySettingsProductSpecificData(
    Profile* profile,
    bool ran_safety_check) {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["Non default setting"] =
      HasNonDefaultPrivacySetting(profile);
  product_specific_data["Ran safety check"] = ran_safety_check;
  return product_specific_data;
}

}  // namespace

TrustSafetySentimentService::TrustSafetySentimentService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
  observed_profiles_.AddObservation(profile);

  // As this service is created lazily, there may already be a primary OTR
  // profile created for the main profile.
  if (auto* primary_otr =
          profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)) {
    observed_profiles_.AddObservation(primary_otr);
  }

  if (base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)) {
    metrics::DesktopSessionDurationTracker::Get()->AddObserver(this);
    performed_control_group_dice_roll_ = false;
  }
}

TrustSafetySentimentService::~TrustSafetySentimentService() {
  if (base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)) {
    metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
  }
}

void TrustSafetySentimentService::OpenedNewTabPage() {
  // Explicit early exit for the common path, where the user has not performed
  // any of the trigger actions.
  if (pending_triggers_.size() == 0)
    return;

  // Reduce the NTPs to open count for all the active triggers.
  for (auto& area_trigger : pending_triggers_) {
    auto& trigger = area_trigger.second;
    if (trigger.remaining_ntps_to_open > 0)
      trigger.remaining_ntps_to_open--;
  }

  // Cleanup any triggers which are no longer relevant. This will be every
  // trigger which occurred more than the maximum prompt time ago, or the
  // trigger for the kIneligible area if it is no longer blocking
  // eligibility.
  base::EraseIf(pending_triggers_,
                [](const std::pair<FeatureArea, PendingTrigger>& area_trigger) {
                  return base::Time::Now() - area_trigger.second.occurred_time >
                             GetMaxTimeToPrompt() ||
                         (area_trigger.first == FeatureArea::kIneligible &&
                          !ShouldBlockSurvey(area_trigger.second));
                });

  // This may have emptied the set of pending triggers.
  if (pending_triggers_.size() == 0)
    return;

  // A primary OTR profile (incognito) existing will prevent any surveys from
  // being shown.
  if (profile_->HasPrimaryOTRProfile())
    return;

  // Check if any of the triggers make the user not yet eligible to receive a
  // survey.
  for (const auto& area_trigger : pending_triggers_) {
    if (ShouldBlockSurvey(area_trigger.second))
      return;
  }

  // Choose a trigger at random to avoid any order biasing.
  auto winning_area_iterator = pending_triggers_.begin();
  std::advance(winning_area_iterator,
               base::RandInt(0, pending_triggers_.size() - 1));

  // The winning feature area should never be kIneligible, as this will
  // have either been removed above, or blocked showing any survey.
  DCHECK(winning_area_iterator->first != FeatureArea::kIneligible);

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);

  // A null HaTS service should have prevented this service from being created.
  DCHECK(hats_service);
  hats_service->LaunchSurvey(
      GetHatsTriggerForFeatureArea(winning_area_iterator->first),
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(),
      winning_area_iterator->second.product_specific_data);
  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.SurveyRequested",
                                winning_area_iterator->first);
  pending_triggers_.clear();
}

void TrustSafetySentimentService::InteractedWithPrivacySettings(
    content::WebContents* web_contents) {
  // Only observe one instance settings at a time. This ignores both multiple
  // instances of settings, and repeated interactions with settings. This
  // reduces the chance that a user is eligible for a survey, but is much
  // simpler. As interactions with settings (visiting password manager and using
  // the privacy card) can occur independently, there is also little risk of
  // starving one interaction.
  if (settings_watcher_)
    return;

  settings_watcher_ = std::make_unique<SettingsWatcher>(
      web_contents,
      features::kTrustSafetySentimentSurveyPrivacySettingsTime.Get(),
      base::BindOnce(&TrustSafetySentimentService::TriggerOccurred,
                     weak_ptr_factory_.GetWeakPtr(),
                     FeatureArea::kPrivacySettings,
                     GetPrivacySettingsProductSpecificData(
                         profile_, /*ran_safety_check=*/false)),
      base::BindOnce(&TrustSafetySentimentService::SettingsWatcherComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrustSafetySentimentService::RanSafetyCheck() {
  // Since we have logic to block a trigger for an incorrect version, we can
  // call both of these and only the appropriate trigger and probability will be
  // recorded.
  TriggerOccurred(FeatureArea::kSafetyCheck, {});
  TriggerOccurred(FeatureArea::kPrivacySettings,
                  GetPrivacySettingsProductSpecificData(
                      profile_, /*ran_safety_check=*/true));
}

void TrustSafetySentimentService::PageInfoOpened() {
  // Only one Page Info should ever be open.
  DCHECK(!page_info_state_);
  page_info_state_ = std::make_unique<PageInfoState>();
}

void TrustSafetySentimentService::InteractedWithPageInfo() {
  DCHECK(page_info_state_);
  page_info_state_->interacted = true;
}

void TrustSafetySentimentService::PageInfoClosed() {
  DCHECK(page_info_state_);

  base::TimeDelta threshold =
      base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)
          ? features::kTrustSafetySentimentSurveyV2TrustedSurfaceTime.Get()
          : features::kTrustSafetySentimentSurveyTrustedSurfaceTime.Get();
  // Record a trigger if either the user had page info open for the required
  // time, or if they interacted with it.
  if (base::Time::Now() - page_info_state_->opened_time >= threshold ||
      page_info_state_->interacted) {
    TriggerOccurred(
        FeatureArea::kTrustedSurface,
        {{"Interacted with Page Info", page_info_state_->interacted}});
  }

  page_info_state_.reset();
}

void TrustSafetySentimentService::SavedPassword() {
  TriggerOccurred(FeatureArea::kTransactions, {{"Saved password", true}});
}

void TrustSafetySentimentService::OpenedPasswordManager(
    content::WebContents* web_contents) {
  if (settings_watcher_)
    return;

  std::map<std::string, bool> product_specific_data = {
      {"Saved password", false}};

  settings_watcher_ = std::make_unique<SettingsWatcher>(
      web_contents,
      features::kTrustSafetySentimentSurveyTransactionsPasswordManagerTime
          .Get(),
      base::BindOnce(&TrustSafetySentimentService::TriggerOccurred,
                     weak_ptr_factory_.GetWeakPtr(), FeatureArea::kTransactions,
                     product_specific_data),
      base::BindOnce(&TrustSafetySentimentService::SettingsWatcherComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrustSafetySentimentService::SavedCard() {
  TriggerOccurred(FeatureArea::kTransactions, {{"Saved password", false}});
}

void TrustSafetySentimentService::RanPasswordCheck() {
  TriggerOccurred(FeatureArea::kPasswordCheck, {});
}

void TrustSafetySentimentService::ClearedBrowsingData(
    browsing_data::BrowsingDataType datatype) {
  // We are only interested in history, downloads, and autofill.
  switch (datatype) {
    case (browsing_data::BrowsingDataType::HISTORY):
    case (browsing_data::BrowsingDataType::DOWNLOADS):
    case (browsing_data::BrowsingDataType::FORM_DATA):
      break;
    default:
      return;
  }
  return TriggerOccurred(
      FeatureArea::kBrowsingData,
      {{"Deleted history",
        datatype == browsing_data::BrowsingDataType::HISTORY},
       {"Deleted downloads",
        datatype == browsing_data::BrowsingDataType::DOWNLOADS},
       {"Deleted autofill form data",
        datatype == browsing_data::BrowsingDataType::FORM_DATA}});
  ;
}

void TrustSafetySentimentService::FinishedPrivacyGuide() {
  TriggerOccurred(FeatureArea::kPrivacyGuide, {});
}

void TrustSafetySentimentService::InteractedWithPrivacySandbox3(
    FeatureArea feature_area) {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["Stable channel"] =
      (chrome::GetChannel() == version_info::Channel::STABLE) ? true : false;
  bool blockCookies =
      HostContentSettingsMapFactory::GetForProfile(profile_)
          ->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                     /*provider_id=*/nullptr) ==
      ContentSetting::CONTENT_SETTING_BLOCK;
  blockCookies =
      blockCookies ||
      (static_cast<content_settings::CookieControlsMode>(
           profile_->GetPrefs()->GetInteger(prefs::kCookieControlsMode)) ==
       content_settings::CookieControlsMode::kBlockThirdParty);
  product_specific_data["3P cookies blocked"] = blockCookies ? true : false;
  product_specific_data["Privacy Sandbox enabled"] =
      profile_->GetPrefs()->GetBoolean(prefs::kPrivacySandboxApisEnabledV2)
          ? true
          : false;
  TriggerOccurred(feature_area, product_specific_data);
}

void TrustSafetySentimentService::InteractedWithPrivacySandbox4(
    FeatureArea feature_area) {
  TriggerOccurred(feature_area, {});
}

void TrustSafetySentimentService::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  // Only interested in the primary OTR profile i.e. the one used for incognito
  // browsing. Non-primary OTR profiles are often used as implementation details
  // of other features, and are not inherintly relevant to Trust & Safety.
  if (off_the_record->GetOTRProfileID() == Profile::OTRProfileID::PrimaryID())
    observed_profiles_.AddObservation(off_the_record);
}

void TrustSafetySentimentService::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);

  if (profile->IsOffTheRecord()) {
    // Closing the incognito profile, which is the only OTR profie observed by
    // this class, is an ileligible action.
    PerformedIneligibleAction();
  }
}

void TrustSafetySentimentService::OnSessionEnded(base::TimeDelta session_length,
                                                 base::TimeTicks session_end) {
  DCHECK(base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2));
  // Check if the user is eligible for the control group.
  if (!performed_control_group_dice_roll_ &&
      session_length > GetMinSessionTime()) {
    performed_control_group_dice_roll_ = true;
    TriggerOccurred(FeatureArea::kControlGroup, {});
  }
}

TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const std::map<std::string, bool>& product_specific_data,
    int remaining_ntps_to_open)
    : product_specific_data(product_specific_data),
      remaining_ntps_to_open(remaining_ntps_to_open),
      occurred_time(base::Time::Now()) {}

TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    int remaining_ntps_to_open)
    : remaining_ntps_to_open(remaining_ntps_to_open),
      occurred_time(base::Time::Now()) {}

TrustSafetySentimentService::PendingTrigger::PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::~PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const PendingTrigger& other) = default;

TrustSafetySentimentService::SettingsWatcher::SettingsWatcher(
    content::WebContents* web_contents,
    base::TimeDelta required_open_time,
    base::OnceCallback<void()> success_callback,
    base::OnceCallback<void()> complete_callback)
    : web_contents_(web_contents),
      success_callback_(std::move(success_callback)),
      complete_callback_(std::move(complete_callback)) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &TrustSafetySentimentService::SettingsWatcher::TimerComplete,
          weak_ptr_factory_.GetWeakPtr()),
      required_open_time);
  Observe(web_contents);
}

TrustSafetySentimentService::SettingsWatcher::~SettingsWatcher() = default;

void TrustSafetySentimentService::SettingsWatcher::WebContentsDestroyed() {
  std::move(complete_callback_).Run();
}

void TrustSafetySentimentService::SettingsWatcher::TimerComplete() {
  const bool stayed_on_settings =
      web_contents_ &&
      web_contents_->GetVisibility() == content::Visibility::VISIBLE &&
      web_contents_->GetLastCommittedURL().host_piece() ==
          chrome::kChromeUISettingsHost;
  if (stayed_on_settings)
    std::move(success_callback_).Run();

  std::move(complete_callback_).Run();
}

TrustSafetySentimentService::PageInfoState::PageInfoState()
    : opened_time(base::Time::Now()) {}

void TrustSafetySentimentService::SettingsWatcherComplete() {
  settings_watcher_.reset();
}

void TrustSafetySentimentService::TriggerOccurred(
    FeatureArea feature_area,
    const std::map<std::string, bool>& product_specific_data) {
  if (!ProbabilityCheck(feature_area))
    return;

  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.TriggerOccurred",
                                feature_area);

  // This will overwrite any previous trigger for this feature area. We are
  // only interested in the most recent trigger, so this is acceptable.
  pending_triggers_[feature_area] =
      PendingTrigger(product_specific_data, GetRequiredNtpCount());
}

void TrustSafetySentimentService::PerformedIneligibleAction() {
  pending_triggers_[FeatureArea::kIneligible] =
      PendingTrigger(GetMaxRequiredNtpCount());

  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.TriggerOccurred",
                                FeatureArea::kIneligible);
}

/*static*/ bool TrustSafetySentimentService::ShouldBlockSurvey(
    const PendingTrigger& trigger) {
  return base::Time::Now() - trigger.occurred_time < GetMinTimeToPrompt() ||
         trigger.remaining_ntps_to_open > 0;
}

// static
bool TrustSafetySentimentService::VersionCheck(FeatureArea feature_area) {
  bool isV2 =
      base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2);
  switch (feature_area) {
    // Version 1 only
    case (FeatureArea::kPrivacySettings):
    case (FeatureArea::kTransactions):
    case (FeatureArea::kPrivacySandbox3ConsentAccept):
    case (FeatureArea::kPrivacySandbox3ConsentDecline):
    case (FeatureArea::kPrivacySandbox3NoticeDismiss):
    case (FeatureArea::kPrivacySandbox3NoticeOk):
    case (FeatureArea::kPrivacySandbox3NoticeSettings):
    case (FeatureArea::kPrivacySandbox3NoticeLearnMore):
      return isV2 == false;
    // Version 2 only
    case (FeatureArea::kSafetyCheck):
    case (FeatureArea::kPasswordCheck):
    case (FeatureArea::kBrowsingData):
    case (FeatureArea::kPrivacyGuide):
    case (FeatureArea::kControlGroup):
      return isV2 == true;
    // Both Versions
    case (FeatureArea::kTrustedSurface):
    case (FeatureArea::kPrivacySandbox4ConsentAccept):
    case (FeatureArea::kPrivacySandbox4ConsentDecline):
    case (FeatureArea::kPrivacySandbox4NoticeOk):
    case (FeatureArea::kPrivacySandbox4NoticeSettings):
      return true;
    // None
    case (FeatureArea::kIneligible):
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

// static
std::string TrustSafetySentimentService::GetHatsTriggerForFeatureArea(
    FeatureArea feature_area) {
  if (base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)) {
    switch (feature_area) {
      case (FeatureArea::kTrustedSurface):
        return kHatsSurveyTriggerTrustSafetyV2TrustedSurface;
      case (FeatureArea::kSafetyCheck):
        return kHatsSurveyTriggerTrustSafetyV2SafetyCheck;
      case (FeatureArea::kPasswordCheck):
        return kHatsSurveyTriggerTrustSafetyV2PasswordCheck;
      case (FeatureArea::kBrowsingData):
        return kHatsSurveyTriggerTrustSafetyV2BrowsingData;
      case (FeatureArea::kPrivacyGuide):
        return kHatsSurveyTriggerTrustSafetyV2PrivacyGuide;
      case (FeatureArea::kControlGroup):
        return kHatsSurveyTriggerTrustSafetyV2ControlGroup;
      case (FeatureArea::kPrivacySandbox4ConsentAccept):
        return kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentAccept;
      case (FeatureArea::kPrivacySandbox4ConsentDecline):
        return kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentDecline;
      case (FeatureArea::kPrivacySandbox4NoticeOk):
        return kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeOk;
      case (FeatureArea::kPrivacySandbox4NoticeSettings):
        return kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeSettings;
      default:
        NOTREACHED();
        return "";
    }
  }
  switch (feature_area) {
    case (FeatureArea::kPrivacySettings):
      return kHatsSurveyTriggerTrustSafetyPrivacySettings;
    case (FeatureArea::kTrustedSurface):
      return kHatsSurveyTriggerTrustSafetyTrustedSurface;
    case (FeatureArea::kTransactions):
      return kHatsSurveyTriggerTrustSafetyTransactions;
    case (FeatureArea::kPrivacySandbox3ConsentAccept):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox3ConsentAccept;
    case (FeatureArea::kPrivacySandbox3ConsentDecline):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox3ConsentDecline;
    case (FeatureArea::kPrivacySandbox3NoticeDismiss):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeDismiss;
    case (FeatureArea::kPrivacySandbox3NoticeOk):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeOk;
    case (FeatureArea::kPrivacySandbox3NoticeSettings):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeSettings;
    case (FeatureArea::kPrivacySandbox3NoticeLearnMore):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox3NoticeLearnMore;
    case (FeatureArea::kPrivacySandbox4ConsentAccept):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentAccept;
    case (FeatureArea::kPrivacySandbox4ConsentDecline):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentDecline;
    case (FeatureArea::kPrivacySandbox4NoticeOk):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeOk;
    case (FeatureArea::kPrivacySandbox4NoticeSettings):
      return kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeSettings;
    default:
      NOTREACHED();
      return "";
  }
}

// static
bool TrustSafetySentimentService::ProbabilityCheck(FeatureArea feature_area) {
  if (!TrustSafetySentimentService::VersionCheck(feature_area)) {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kTrustSafetySentimentSurveyV2)) {
    switch (feature_area) {
      case (FeatureArea::kTrustedSurface):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2TrustedSurfaceProbability
                   .Get();
      case (FeatureArea::kSafetyCheck):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2SafetyCheckProbability
                   .Get();
      case (FeatureArea::kPasswordCheck):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2PasswordCheckProbability
                   .Get();
      case (FeatureArea::kBrowsingData):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2BrowsingDataProbability
                   .Get();
      case (FeatureArea::kPrivacyGuide):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2PrivacyGuideProbability
                   .Get();
      case (FeatureArea::kControlGroup):
        return base::RandDouble() <
               features::kTrustSafetySentimentSurveyV2ControlGroupProbability
                   .Get();
      case (FeatureArea::kPrivacySandbox4ConsentAccept):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentAcceptProbability
                       .Get();
      case (FeatureArea::kPrivacySandbox4ConsentDecline):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentDeclineProbability
                       .Get();
      case (FeatureArea::kPrivacySandbox4NoticeOk):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeOkProbability
                       .Get();
      case (FeatureArea::kPrivacySandbox4NoticeSettings):
        return base::RandDouble() <
               features::
                   kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeSettingsProbability
                       .Get();
      default:
        NOTREACHED();
        return false;
    }
  }

  switch (feature_area) {
    case (FeatureArea::kPrivacySettings):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyPrivacySettingsProbability
                 .Get();
    case (FeatureArea::kTrustedSurface):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTrustedSurfaceProbability
                 .Get();
    case (FeatureArea::kTransactions):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTransactionsProbability.Get();
    case (FeatureArea::kPrivacySandbox3ConsentAccept):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox3ConsentAcceptProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox3ConsentDecline):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox3ConsentDeclineProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox3NoticeDismiss):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox3NoticeDismissProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox3NoticeOk):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox3NoticeOkProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox3NoticeSettings):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox3NoticeSettingsProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox3NoticeLearnMore):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox3NoticeLearnMoreProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox4ConsentAccept):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox4ConsentAcceptProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox4ConsentDecline):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox4ConsentDeclineProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox4NoticeOk):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox4NoticeOkProbability
                     .Get();
    case (FeatureArea::kPrivacySandbox4NoticeSettings):
      return base::RandDouble() <
             features::
                 kTrustSafetySentimentSurveyPrivacySandbox4NoticeSettingsProbability
                     .Get();
    default:
      NOTREACHED();
      return false;
  }
}
