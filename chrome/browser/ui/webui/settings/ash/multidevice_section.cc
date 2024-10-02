// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/multidevice_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/android_sms/android_sms_service.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"
#include "chrome/browser/ui/webui/settings/ash/multidevice_handler.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager.h"
#include "chromeos/ash/components/phonehub/url_constants.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/url_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath;
using ::chromeos::settings::mojom::kMultiDeviceSectionPath;
using ::chromeos::settings::mojom::kNearbyShareSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

using Feature = multidevice_setup::mojom::Feature;
using FeatureState = multidevice_setup::mojom::FeatureState;
using HostStatus = multidevice_setup::mojom::HostStatus;

const std::vector<SearchConcept>& GetMultiDeviceOptedInSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MULTIDEVICE_FORGET,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kForgetPhone},
        {IDS_OS_SETTINGS_TAG_MULTIDEVICE_FORGET_ALT1,
         SearchConcept::kAltTagEnd}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE_MESSAGES,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kMessages,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kMessagesOnOff},
        {IDS_OS_SETTINGS_TAG_MULTIDEVICE_MESSAGES_ALT1,
         SearchConcept::kAltTagEnd}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kMultiDeviceFeatures},
        {IDS_OS_SETTINGS_TAG_MULTIDEVICE_ALT1, SearchConcept::kAltTagEnd}},
       {IDS_OS_SETTINGS_TAG_MULTIDEVICE_SMART_LOCK,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kLock,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kSmartLockOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>&
GetMultiDeviceOptedInPhoneHubSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB,
       mojom::kMultiDeviceFeaturesSubpagePath,
       mojom::SearchResultIcon::kPhone,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPhoneHubOnOff}},
      {IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB_NOTIFICATIONS,
       mojom::kMultiDeviceFeaturesSubpagePath,
       mojom::SearchResultIcon::kPhone,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPhoneHubNotificationsOnOff}},
      {IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB_TASK_CONTINUATION,
       mojom::kMultiDeviceFeaturesSubpagePath,
       mojom::SearchResultIcon::kPhone,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPhoneHubTaskContinuationOnOff}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetMultiDeviceOptedInPhoneHubCameraRollSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kPhoneHubCameraRollOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>&
GetMultiDeviceOptedInPhoneHubAppsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB_APPS,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kPhoneHubAppsOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>&
GetMultiDeviceOptedInWifiSyncSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MULTIDEVICE_WIFI_SYNC,
        mojom::kMultiDeviceFeaturesSubpagePath,
        mojom::SearchResultIcon::kWifi,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kWifiSyncOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>& GetMultiDeviceOptedOutSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags([] {
    // Special-case: The "set up" search tag also includes the names of the
    // multi-device features as a way to increase discoverability of these
    // features.
    SearchConcept set_up_concept{
        IDS_OS_SETTINGS_TAG_MULTIDEVICE_SET_UP,
        mojom::kMultiDeviceSectionPath,
        mojom::SearchResultIcon::kPhone,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kSetUpMultiDevice},
        {IDS_OS_SETTINGS_TAG_MULTIDEVICE,
         IDS_OS_SETTINGS_TAG_MULTIDEVICE_MESSAGES,
         IDS_OS_SETTINGS_TAG_MULTIDEVICE_SMART_LOCK, SearchConcept::kAltTagEnd},
    };

    // Include the following features in alternate message IDs if they are
    // enabled and the alt tag limit has not been reached: Phone Hub, Instant
    // Tethering and Wifi Sync.
    int alt_tag_index = 3;

    if (features::IsPhoneHubEnabled()) {
      set_up_concept.alt_tag_ids[alt_tag_index] =
          IDS_OS_SETTINGS_TAG_MULTIDEVICE_PHONE_HUB;
      alt_tag_index++;
    }
    if (base::FeatureList::IsEnabled(features::kInstantTethering)) {
      set_up_concept.alt_tag_ids[alt_tag_index] =
          IDS_OS_SETTINGS_TAG_INSTANT_TETHERING;
      alt_tag_index++;
    }

    // TODO(b/234730982): Update 5 alt tag limit to 6 and remove condition
    if (alt_tag_index < 5 && features::IsWifiSyncAndroidEnabled()) {
      set_up_concept.alt_tag_ids[alt_tag_index] =
          IDS_OS_SETTINGS_TAG_MULTIDEVICE_WIFI_SYNC;
      alt_tag_index++;
    }

    if (alt_tag_index < 5) {
      set_up_concept.alt_tag_ids[alt_tag_index] = SearchConcept::kAltTagEnd;
    }

    return std::vector<SearchConcept>{set_up_concept};
  }());
  return *tags;
}

const std::vector<SearchConcept>& GetNearbyShareOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MULTIDEVICE_NEARBY_SHARE,
       mojom::kNearbyShareSubpagePath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kNearbyShare}},
      {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_TURN_OFF,
       mojom::kNearbyShareSubpagePath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNearbyShareOnOff},
       {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_TURN_OFF_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_DEVICE_NAME,
       mojom::kNearbyShareSubpagePath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNearbyShareDeviceName}},
      {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_DEVICE_VISIBILITY,
       mojom::kNearbyShareSubpagePath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNearbyShareDeviceVisibility},
       {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_DEVICE_VISIBILITY_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_CONTACTS,
       mojom::kNearbyShareSubpagePath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNearbyShareContacts}},
      {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_DATA_USAGE,
       mojom::kNearbyShareSubpagePath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNearbyShareDataUsage}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetNearbyShareBackgroundScanningOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_NEARBY_SHARE_DEVICES_NEARBY_SHARING_NOTIFICATION_ON,
        mojom::kNearbyShareSubpagePath,
        mojom::SearchResultIcon::kNearbyShare,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting =
             mojom::Setting::kDevicesNearbyAreSharingNotificationOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>&
GetNearbyShareBackgroundScanningOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_NEARBY_SHARE_DEVICES_NEARBY_SHARING_NOTIFICATION_OFF,
        mojom::kNearbyShareSubpagePath,
        mojom::SearchResultIcon::kNearbyShare,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting =
             mojom::Setting::kDevicesNearbyAreSharingNotificationOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>& GetNearbyShareOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_TURN_ON,
       mojom::kMultiDeviceSectionPath,
       mojom::SearchResultIcon::kNearbyShare,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNearbyShareOnOff},
       {IDS_OS_SETTINGS_TAG_NEARBY_SHARE_TURN_ON_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

bool IsOptedIn(HostStatus host_status) {
  return host_status == HostStatus::kHostSetButNotYetVerified ||
         host_status == HostStatus::kHostVerified;
}

}  // namespace

MultiDeviceSection::MultiDeviceSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    phonehub::PhoneHubManager* phone_hub_manager,
    android_sms::AndroidSmsService* android_sms_service,
    PrefService* pref_service,
    eche_app::EcheAppManager* eche_app_manager)
    : OsSettingsSection(profile, search_tag_registry),
      multidevice_setup_client_(multidevice_setup_client),
      phone_hub_manager_(phone_hub_manager),
      android_sms_service_(android_sms_service),
      pref_service_(pref_service),
      eche_app_manager_(eche_app_manager),
      html_source_(nullptr) {
  if (NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          profile)) {
    NearbySharingService* nearby_sharing_service =
        NearbySharingServiceFactory::GetForBrowserContext(profile);
    nearby_sharing_service->GetSettings()->AddSettingsObserver(
        settings_receiver_.BindNewPipeAndPassRemote());

    NearbyShareSettings* nearby_share_settings =
        nearby_sharing_service->GetSettings();
    OnEnabledChanged(nearby_share_settings->GetEnabled());
    RefreshNearbyBackgroundScanningShareSearchConcepts();
  }
  if (features::IsEcheSWAEnabled()) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        prefs::kEnableAutoScreenLock,
        base::BindRepeating(&MultiDeviceSection::OnEnableScreenLockChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        phonehub::prefs::kScreenLockStatus,
        base::BindRepeating(&MultiDeviceSection::OnScreenLockStatusChanged,
                            base::Unretained(this)));
  }

  // Note: |multidevice_setup_client_| is null when multi-device features are
  // prohibited by policy.
  if (!multidevice_setup_client_) {
    return;
  }

  multidevice_setup_client_->AddObserver(this);
  OnHostStatusChanged(multidevice_setup_client_->GetHostStatus());
  OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());
}

MultiDeviceSection::~MultiDeviceSection() {
  pref_change_registrar_.RemoveAll();
  if (multidevice_setup_client_) {
    multidevice_setup_client_->RemoveObserver(this);
  }
}

void MultiDeviceSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  html_source_ = html_source;
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"multidevicePageTitle", IDS_SETTINGS_MULTIDEVICE},
      {"multideviceSetupButton", IDS_SETTINGS_MULTIDEVICE_SETUP_BUTTON},
      {"multideviceVerifyButton", IDS_SETTINGS_MULTIDEVICE_VERIFY_BUTTON},
      {"multideviceSetupButtonA11yLabel",
       IDS_SETTINGS_MULTIDEVICE_SETUP_BUTTON_A11Y_LABEL},
      {"multideviceVerifyButtonA11yLabel",
       IDS_SETTINGS_MULTIDEVICE_VERIFY_BUTTON_A11Y_LABEL},
      {"multideviceSetupItemHeading",
       IDS_SETTINGS_MULTIDEVICE_SETUP_ITEM_HEADING},
      {"multideviceEnabled", IDS_SETTINGS_MULTIDEVICE_ENABLED},
      {"multideviceDisabled", IDS_SETTINGS_MULTIDEVICE_DISABLED},
      {"multideviceSuiteToggleA11yLabel",
       IDS_SETTINGS_MULTIDEVICE_SUITE_TOGGLE_A11Y_LABEL},
      {"multideviceSmartLockItemTitle", IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE},
      {"multidevicePhoneHubItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_SECTION_TITLE},
      {"multidevicePhoneHubCameraRollItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL_SECTION_TITLE},
      {"multidevicePhoneHubCameraRollItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL_SUMMARY},
      {"multidevicePhoneHubLearnMoreLabel",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_LEARN_MORE_LABEL},
      {"multidevicePhoneHubNotificationsItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_NOTIFICATIONS_SECTION_TITLE},
      {"multidevicePhoneHubTaskContinuationItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_TASK_CONTINUATION_SECTION_TITLE},
      {"multidevicePhoneHubTaskContinuationItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_TASK_CONTINUATION_SUMMARY},
      {"multidevicePhoneHubTaskContinuationSyncLabel",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_TASK_CONTINUATION_SYNC_LABEL},
      {"multideviceWifiSyncItemTitle",
       IDS_SETTINGS_MULTIDEVICE_WIFI_SYNC_SECTION_TITLE},
      {"multideviceWifiSyncChromeSyncLabel",
       IDS_SETTINGS_MULTIDEVICE_WIFI_SYNC_CHROME_SYNC_LABEL},
      {"multideviceWifiSyncLearnMoreLabel",
       IDS_SETTINGS_MULTIDEVICE_WIFI_SYNC_LEARN_MORE_LABEL},
      {"multideviceNotificationAccessSetupConnectingTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_CONNECTING_TITLE},
      {"multideviceNotificationAccessSetupScreenLockIconInstruction",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_SCREEN_LOCK_ICON_INSTRUCTION},
      {"multideviceNotificationAccessSetupAwaitingResponseTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_AWAITING_RESPONSE_TITLE},
      {"multideviceNotificationAccessSetupInstructions",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_INSTRUCTIONS},
      {"multideviceNotificationAccessSetupCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_COMPLETED_TITLE},
      {"multideviceNotificationAccessSetupConnectionLostWithPhoneTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_CONNECTION_LOST_WITH_PHONE_TITLE},
      {"multideviceNotificationAccessSetupCouldNotEstablishConnectionTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_COULD_NOT_ESTABLISH_CONNECTION_TITLE},
      {"multideviceNotificationAccessSetupGetStarted",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_GET_STARTED_BUTTON_LABEL},
      {"multideviceNotificationAccessSetupTryAgain",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_TRY_AGAIN_BUTTON_LABEL},
      {"multideviceNotificationAccessSetupMaintainFailureSummary",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_CONNECTION_LOST_WITH_PHONE_SUMMARY},
      {"multideviceNotificationAccessSetupEstablishFailureSummary",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_COULD_NOT_ESTABLISH_CONNECTION_SUMMARY},
      {"multideviceNotificationAccessSetupAccessProhibitedTitle",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_ACCESS_PROHIBITED_TITLE},
      {"multideviceNotificationAccessProhibitedTooltip",
       IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_PROHIBITED_TOOLTIP},
      {"multideviceItemDisabledByPhoneAdminTooltip",
       IDS_SETTINGS_MULTIDEVICE_ITEM_DISABLED_BY_PHONE_ADMIN_TOOLTIP},
      {"multideviceInstantTetheringItemTitle",
       IDS_SETTINGS_MULTIDEVICE_INSTANT_TETHERING},
      {"multideviceInstantTetheringItemSummary",
       IDS_SETTINGS_MULTIDEVICE_INSTANT_TETHERING_SUMMARY},
      {"multideviceAndroidMessagesItemTitle",
       IDS_SETTINGS_MULTIDEVICE_ANDROID_MESSAGES},
      {"multideviceForgetDevice", IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE},
      {"multideviceSmartLockOptions",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOCK},
      {"multideviceForgetDeviceDisconnect",
       IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE_DISCONNECT},
      {"multidevicePhoneHubAppsAndNotificationsItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_APPS_AND_NOTIFICATIONS_SECTION_TITLE},
      {"multidevicePhoneHubCameraRollAndNotificationsItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL_AND_NOTIFICATIONS_SECTION_TITLE},
      {"multidevicePhoneHubCameraRollAndNotificationsItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL_AND_NOTIFICATIONS_SUMMARY},
      {"multidevicePhoneHubCameraRollAndAppsItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL_AND_APPS_SECTION_TITLE},
      {"multidevicePhoneHubCameraRollNotificationsAndAppsItemTitle",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL_NOTIFICATIONS_AND_APPS_SECTION_TITLE},
      {"multidevicePhoneHubNotificationsItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_NOTIFICATIONS_SUMMARY},
      {"multidevicePhoneHubAppsItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_APPS_SUMMARY},
      {"multidevicePhoneHubAppsAndNotificationsItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_APPS_AND_NOTIFICATIONS_SUMMARY},
      {"multidevicePhoneHubCameraRollAndAppsItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL_AND_APPS_SUMMARY},
      {"multidevicePhoneHubCameraRollNotificationsAndAppsItemSummary",
       IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_CAMERA_ROLL_NOTIFICATIONS_AND_APPS_SUMMARY},
      {"multideviceLearnMoreWithoutURL", IDS_SETTINGS_LEARN_MORE},
      {"multidevicePhoneHubLearnMoreAriaLabel",
       IDS_SETTINGS_PHONE_HUB_LEARN_MORE_ARIA_LABEL},
      {"multidevicePermissionsSetupCameraRollSummary",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_CAMERA_ROLL_ITEM_SUMMARY},
      {"multidevicePermissionsSetupNotificationsSummary",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_NOTIFICATION_ITEM_SUMMARY},
      {"multidevicePermissionsSetupAppsSummary",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_APPS_ITEM_SUMMARY},
      {"multidevicePermissionsSetupInstructions",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_INSTRUCTIONS},
      {"multidevicePermissionsSetupOperationsInstructions",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_OPERATION_INSTRUCTIONS},
      {"multidevicePermissionsSetupAwaitingResponseTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_AWAITING_RESPONSE_TITLE},
      {"multidevicePermissionsSetupCouldNotEstablishConnectionTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_COULD_NOT_ESTABLISH_CONNECTION_TITLE},
      {"multidevicePermissionsSetupEstablishFailureSummary",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_COULD_NOT_ESTABLISH_CONNECTION_SUMMARY},
      {"multidevicePermissionsSetupMaintainFailureSummary",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_CONNECTION_LOST_WITH_PHONE_SUMMARY},
      {"multidevicePermissionsSetupNotificationAccessProhibitedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_NOTIFICATION_ACCESS_PROHIBITED_TITLE},
      {"multidevicePermissionsSetupCompletedMoreFeaturesSummary",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_COMPLETED_MORE_FEATURES_SUMMARY},
      {"multidevicePermissionsSetupAllCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_ALL_COMPLETED_TITLE},
      {"multidevicePermissionsSetupCameraRollAndNotificationsCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_CAMERA_ROLL_AND_NOTIFICATIONS_COMPLETED_TITLE},
      {"multidevicePermissionsSetupNotificationsAndAppsCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_NOTIFICATIONS_AND_APPS_COMPLETED_TITLE},
      {"multidevicePermissionsSetupCameraRollAndAppsCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_CAMERA_ROLL_AND_APPS_COMPLETED_TITLE},
      {"multidevicePermissionsSetupCameraRollCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_CAMERA_ROLL_COMPLETED_TITLE},
      {"multidevicePermissionsSetupNotificationsCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_NOTIFICATIONS_COMPLETED_TITLE},
      {"multidevicePermissionsSetupAppssCompletedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_APPS_COMPLETED_TITLE},
      {"multidevicePermissionsSetupAppssCompletedFailedTitle",
       IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_COMPLETED_FAILED_TITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("multideviceAllowedByPolicy",
                          multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
                              profile()->GetPrefs()));
  html_source->AddString(
      "multideviceNotificationAccessSetupScreenLockTitle",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_SCREEN_LOCK_TITLE));
  html_source->AddString(
      "multideviceNotificationAccessSetupAckSummary",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_ACK_SUMMARY));
  html_source->AddString(
      "multideviceNotificationAccessSetupCompletedSummary",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_COMPLETED_SUMMARY));
  html_source->AddString(
      "multideviceNotificationAccessSetupScreenLockSubtitle",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_SCREEN_LOCK_SUBTITLE));
  html_source->AddString(
      "multideviceForgetDeviceSummary",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_FORGET_THIS_DEVICE_EXPLANATION));
  html_source->AddString(
      "multideviceForgetDeviceDialogMessage",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_FORGET_DEVICE_DIALOG_MESSAGE));
  html_source->AddString(
      "multideviceVerificationText",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_VERIFICATION_TEXT,
          base::UTF8ToUTF16(
              multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceSetupSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_SETUP_SUMMARY, ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(
              multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceNoHostText",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_NO_ELIGIBLE_HOSTS,
          base::UTF8ToUTF16(
              multidevice_setup::
                  GetBoardSpecificBetterTogetherSuiteLearnMoreUrl()
                      .spec())));
  html_source->AddString(
      "multideviceAndroidMessagesItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_ANDROID_MESSAGES_SUMMARY,
          ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(
              multidevice_setup::GetBoardSpecificMessagesLearnMoreUrl()
                  .spec())));
  html_source->AddString(
      "multideviceSmartLockItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_SMART_LOCK_SUMMARY,
          ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kEasyUnlockLearnMoreUrl)));
  html_source->AddString(
      "multidevicePhoneHubItemSummary",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_SUMMARY,
                                 ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "multideviceNotificationAccessSetupAckTitle",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_ACK_TITLE));
  html_source->AddString(
      "multideviceNotificationAccessSetupAwaitingResponseSummary",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_AWAITING_RESPONSE_SUMMARY));
  html_source->AddString(
      "multideviceNotificationAccessSetupAccessProhibitedSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_NOTIFICATION_ACCESS_SETUP_DIALOG_ACCESS_PROHIBITED_SUMMARY,
          GetHelpUrlWithBoard(phonehub::kPhoneHubLearnMoreLink)));
  html_source->AddString(
      "multideviceWifiSyncItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_WIFI_SYNC_SUMMARY,
          GetHelpUrlWithBoard(chrome::kWifiSyncLearnMoreURL)));
  html_source->AddString(
      "multideviceEnableWifiSyncV1ItemSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_ENABLE_WIFI_SYNC_V1_SUMMARY,
          GetHelpUrlWithBoard(chrome::kWifiSyncLearnMoreURL)));
  html_source->AddString(
      "multidevicePhoneHubTaskContinuationDisabledSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_TASK_CONTINUATION_DISABLED_SUMMARY,
          GetHelpUrlWithBoard(phonehub::kPhoneHubLearnMoreLink)));
  html_source->AddString(
      "multidevicePhoneHubPermissionsLearnMoreURL",
      GetHelpUrlWithBoard(chrome::kPhoneHubPermissionLearnMoreURL));
  html_source->AddString(
      "multidevicePermissionsSetupAckTitle",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_ACK_TITLE));
  html_source->AddString(
      "multidevicePermissionsSetupAckSubtitle",
      ui::SubstituteChromeOSDeviceType(
          IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_ACK_SUBTITLE));
  html_source->AddString(
      "multidevicePermissionsSetupNotificationAccessProhibitedSummary",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_MULTIDEVICE_PERMISSIONS_SETUP_DIALOG_NOTIFICATION_ACCESS_PROHIBITED_SUMMARY,
          GetHelpUrlWithBoard(phonehub::kPhoneHubLearnMoreLink)));

  // We still need to register strings even if Nearby Share is not supported.
  // For example, the HTML is always built but only displayed if Nearby Share is
  // supported.
  ::settings::AddNearbyShareData(html_source);
  RegisterNearbySharedStrings(html_source);
  html_source->AddBoolean(
      "isNearbyShareSupported",
      NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          profile()));
  html_source->AddBoolean("isEcheAppEnabled", features::IsEcheSWAEnabled());
  OnEnableScreenLockChanged();
  OnScreenLockStatusChanged();
  html_source->AddBoolean("isOnePageOnboardingEnabled",
                          base::FeatureList::IsEnabled(
                              ::features::kNearbySharingOnePageOnboarding));
  html_source->AddBoolean(
      "isSmartLockSignInRemoved",
      base::FeatureList::IsEnabled(features::kSmartLockSignInRemoved));

  if (base::FeatureList::IsEnabled(features::kPhoneHubAppStreamingBetaBadge)) {
    html_source->AddString(
        "multidevicePhoneHubAppsItemTitle",
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_APPS_SECTION_BETA_TITLE));
  } else {
    html_source->AddString(
        "multidevicePhoneHubAppsItemTitle",
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_MULTIDEVICE_PHONE_HUB_APPS_SECTION_TITLE));
  }
}

void MultiDeviceSection::AddHandlers(content::WebUI* web_ui) {
  // No handlers in guest mode.
  if (profile()->IsGuestSession()) {
    return;
  }

  web_ui->AddMessageHandler(std::make_unique<MultideviceHandler>(
      pref_service_, multidevice_setup_client_,
      phone_hub_manager_
          ? phone_hub_manager_->GetMultideviceFeatureAccessManager()
          : nullptr,
      android_sms_service_
          ? android_sms_service_->android_sms_pairing_state_tracker()
          : nullptr,
      android_sms_service_ ? android_sms_service_->android_sms_app_manager()
                           : nullptr,
      eche_app_manager_ ? eche_app_manager_->GetAppsAccessManager() : nullptr,
      phone_hub_manager_ ? phone_hub_manager_->GetCameraRollManager() : nullptr,
      phone_hub_manager_ ? phone_hub_manager_->GetBrowserTabsModelProvider()
                         : nullptr));
}

int MultiDeviceSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_MULTIDEVICE;
}

mojom::Section MultiDeviceSection::GetSection() const {
  return mojom::Section::kMultiDevice;
}

mojom::SearchResultIcon MultiDeviceSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kPhone;
}

std::string MultiDeviceSection::GetSectionPath() const {
  return mojom::kMultiDeviceSectionPath;
}

bool MultiDeviceSection::LogMetric(mojom::Setting setting,
                                   base::Value& value) const {
  // Unimplemented.
  return false;
}

void MultiDeviceSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kSetUpMultiDevice);
  generator->RegisterTopLevelSetting(mojom::Setting::kVerifyMultiDeviceSetup);

  // MultiDevice features.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_MULTIDEVICE, mojom::Subpage::kMultiDeviceFeatures,
      mojom::SearchResultIcon::kPhone, mojom::SearchResultDefaultRank::kMedium,
      mojom::kMultiDeviceFeaturesSubpagePath);
  static constexpr mojom::Setting kMultiDeviceFeaturesSettings[] = {
      mojom::Setting::kMultiDeviceOnOff,
      mojom::Setting::kMessagesSetUp,
      mojom::Setting::kMessagesOnOff,
      mojom::Setting::kForgetPhone,
      mojom::Setting::kPhoneHubOnOff,
      mojom::Setting::kPhoneHubCameraRollOnOff,
      mojom::Setting::kPhoneHubNotificationsOnOff,
      mojom::Setting::kPhoneHubTaskContinuationOnOff,
      mojom::Setting::kWifiSyncOnOff,
      mojom::Setting::kPhoneHubAppsOnOff,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kMultiDeviceFeatures,
                            kMultiDeviceFeaturesSettings, generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kMultiDeviceOnOff);
  // Note: Instant Tethering is part of the Network section, but it has an
  // alternate setting within the MultiDevice section.
  generator->RegisterNestedAltSetting(mojom::Setting::kInstantTetheringOnOff,
                                      mojom::Subpage::kMultiDeviceFeatures);

  // Nearby Share, registered regardless of the flag.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_NEARBY_SHARE_TITLE, mojom::Subpage::kNearbyShare,
      mojom::SearchResultIcon::kNearbyShare,
      mojom::SearchResultDefaultRank::kMedium, mojom::kNearbyShareSubpagePath);
  static constexpr mojom::Setting kNearbyShareSettings[] = {
      mojom::Setting::kNearbyShareOnOff,
      mojom::Setting::kNearbyShareDeviceName,
      mojom::Setting::kNearbyShareDeviceVisibility,
      mojom::Setting::kNearbyShareContacts,
      mojom::Setting::kNearbyShareDataUsage,
      mojom::Setting::kDevicesNearbyAreSharingNotificationOnOff,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kNearbyShare, kNearbyShareSettings,
                            generator);
  generator->RegisterTopLevelAltSetting(mojom::Setting::kNearbyShareOnOff);
}

void MultiDeviceSection::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetMultiDeviceOptedOutSearchConcepts());
  updater.RemoveSearchTags(GetMultiDeviceOptedInSearchConcepts());

  if (IsOptedIn(host_status_with_device.first)) {
    updater.AddSearchTags(GetMultiDeviceOptedInSearchConcepts());
  } else {
    updater.AddSearchTags(GetMultiDeviceOptedOutSearchConcepts());
  }
}

void MultiDeviceSection::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetMultiDeviceOptedInPhoneHubSearchConcepts());
  updater.RemoveSearchTags(
      GetMultiDeviceOptedInPhoneHubCameraRollSearchConcepts());
  updater.RemoveSearchTags(GetMultiDeviceOptedInWifiSyncSearchConcepts());
  updater.RemoveSearchTags(GetMultiDeviceOptedInPhoneHubAppsSearchConcepts());

  if (IsFeatureSupported(Feature::kPhoneHub)) {
    updater.AddSearchTags(GetMultiDeviceOptedInPhoneHubSearchConcepts());
    if (features::IsPhoneHubCameraRollEnabled() &&
        IsFeatureSupported(Feature::kPhoneHubCameraRoll)) {
      updater.AddSearchTags(
          GetMultiDeviceOptedInPhoneHubCameraRollSearchConcepts());
    }
  }
  if (IsFeatureSupported(Feature::kWifiSync)) {
    updater.AddSearchTags(GetMultiDeviceOptedInWifiSyncSearchConcepts());
  }
  if (IsFeatureSupported(Feature::kEche)) {
    updater.AddSearchTags(GetMultiDeviceOptedInPhoneHubAppsSearchConcepts());
  }
}

bool MultiDeviceSection::IsFeatureSupported(Feature feature) {
  const FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(feature);
  return feature_state != FeatureState::kNotSupportedByPhone &&
         feature_state != FeatureState::kNotSupportedByChromebook;
}

void MultiDeviceSection::RefreshNearbyBackgroundScanningShareSearchConcepts() {
  if (!NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          profile())) {
    return;
  }
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  NearbySharingService* nearby_sharing_service =
      NearbySharingServiceFactory::GetForBrowserContext(profile());
  NearbyShareSettings* nearby_share_settings =
      nearby_sharing_service->GetSettings();

  if (!nearby_share_settings->is_fast_initiation_hardware_supported()) {
    updater.RemoveSearchTags(
        GetNearbyShareBackgroundScanningOnSearchConcepts());
    updater.RemoveSearchTags(
        GetNearbyShareBackgroundScanningOffSearchConcepts());
    return;
  }

  if (nearby_share_settings->GetFastInitiationNotificationState() ==
      nearby_share::mojom::FastInitiationNotificationState::kEnabled) {
    updater.AddSearchTags(GetNearbyShareBackgroundScanningOnSearchConcepts());
    updater.RemoveSearchTags(
        GetNearbyShareBackgroundScanningOffSearchConcepts());
  } else {
    updater.AddSearchTags(GetNearbyShareBackgroundScanningOffSearchConcepts());
    updater.RemoveSearchTags(
        GetNearbyShareBackgroundScanningOnSearchConcepts());
  }
}

void MultiDeviceSection::OnEnabledChanged(bool enabled) {
  if (!NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          profile())) {
    return;
  }
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  if (enabled) {
    updater.RemoveSearchTags(GetNearbyShareOffSearchConcepts());
    updater.AddSearchTags(GetNearbyShareOnSearchConcepts());
  } else {
    updater.RemoveSearchTags(GetNearbyShareOnSearchConcepts());
    updater.AddSearchTags(GetNearbyShareOffSearchConcepts());
  }
}

void MultiDeviceSection::OnFastInitiationNotificationStateChanged(
    nearby_share::mojom::FastInitiationNotificationState state) {
  RefreshNearbyBackgroundScanningShareSearchConcepts();
}

void MultiDeviceSection::OnIsFastInitiationHardwareSupportedChanged(
    bool is_supported) {
  RefreshNearbyBackgroundScanningShareSearchConcepts();
}

void MultiDeviceSection::OnEnableScreenLockChanged() {
  // We need AddBoolean here to update value because users could into onboarding
  // flow directly from phone hub tray.
  const bool is_screen_lock_enabled =
      SessionControllerClientImpl::CanLockScreen() &&
      SessionControllerClientImpl::ShouldLockScreenAutomatically();
  if (html_source_) {
    html_source_->AddBoolean("isChromeosScreenLockEnabled",
                             is_screen_lock_enabled);
  }
}

void MultiDeviceSection::OnScreenLockStatusChanged() {
  // We need AddBoolean here to update value because users could into onboarding
  // flow directly from phone hub tray.
  const bool is_phone_screen_lock_enabled =
      static_cast<phonehub::ScreenLockManager::LockStatus>(
          pref_service_->GetInteger(phonehub::prefs::kScreenLockStatus)) ==
      phonehub::ScreenLockManager::LockStatus::kLockedOn;
  if (html_source_) {
    html_source_->AddBoolean("isPhoneScreenLockEnabled",
                             is_phone_screen_lock_enabled);
  }
}

}  // namespace ash::settings
