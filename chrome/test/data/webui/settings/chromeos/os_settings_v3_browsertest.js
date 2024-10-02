// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/browser/ash/crostini/fake_crostini_features.h"');
GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');
GEN('#include "chrome/common/buildflags.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/app_restore/features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ui/accessibility/accessibility_features.h"');

/** Test fixture for shared Polymer 3 elements. */
var OSSettingsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kEnableHostnameSetting',
        // TODO(b/217560706): Remove this explicit enabled flag when rollout
        // completed.
        'ash::features::kDiacriticsOnPhysicalKeyboardLongpress',
      ],
    };
  }
};

var OSSettingsDevicePageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/device_page/device_page_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kAudioSettingsPage',
        'ash::features::kInputDeviceSettingsSplit',
      ],
    };
  }
};

// TODO(https://crbug.com/1422799): The test is flaky on ChromeOS debug.
TEST_F_WITH_PREAMBLE(
    `
#if !defined(NDEBUG)
#define MAYBE_All DISABLED_All
#else
#define MAYBE_All All
#endif
    `,
    'OSSettingsDevicePageV3Test', 'MAYBE_All',
    () => mocha.grep('/^((?!arrow_key_arrangement_disabled).)*$/').run());

// TODO(crbug.com/1234871) Move this test back into the list of tests below once
// Fast pair is launched.
var OSSettingsOsBluetoothDevicesSubpageV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_bluetooth_page/os_bluetooth_devices_subpage_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat([
        'ash::features::kFastPair',
        'ash::features::kFastPairSavedDevices',
        'ash::features::kFastPairSoftwareScanning',
      ]),
    };
  }
};

TEST_F('OSSettingsOsBluetoothDevicesSubpageV3Test', 'AllJsTests', () => {
  mocha.run();
});

// TODO (b/238647706) Move this test back into the list of tests below once
// Fast pair is launched.
var OSSettingsOsBluetoothSavedDevicesSubpageV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_saved_devices_subpage_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat([
        'ash::features::kFastPair',
        'ash::features::kFastPairSavedDevices',
        'ash::features::kFastPairSoftwareScanning',
      ]),
    };
  }
};

// TODO (b/238647706) Move this test back into the list of tests below once
// Fast pair is launched.
TEST_F('OSSettingsOsBluetoothSavedDevicesSubpageV3Test', 'AllJsTests', () => {
  mocha.run();
});

// TODO(crbug.com/1234871) Move this test back into the list of tests below once
// Fast pair is launched.
var OSSettingsOsBluetoothSavedDevicesListV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/os_saved_devices_list_tests.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat([
        'ash::features::kFastPair',
        'ash::features::kFastPairSavedDevices',
        'ash::features::kFastPairSoftwareScanning',
      ]),
    };
  }
};

TEST_F('OSSettingsOsBluetoothSavedDevicesListV3Test', 'AllJsTests', () => {
  mocha.run();
});

var OSSettingsAppManagementAppDetailsV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/app_management/app_details_item_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: super.featureList.enabled.concat(
          ['features::kAppManagementAppDetails']),
    };
  }
};

function crostiniTestGenPreamble() {
  GEN('crostini::FakeCrostiniFeatures fake_crostini_features;');
  GEN('fake_crostini_features.SetAll(true);');
}

TEST_F('OSSettingsAppManagementAppDetailsV3Test', 'AllJsTests', () => {
  mocha.run();
});

var OSSettingsCrostiniPageV3Test = class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/crostini_page_test.js';
  }

  /** @override */
  testGenPreamble() {
    return crostiniTestGenPreamble();
  }
};

TEST_F('OSSettingsCrostiniPageV3Test', 'AllJsTests', () => {
  mocha.run();
});

var OSSettingsCrostiniExtraContainerPageV3Test =
    class extends OSSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/test_loader.html?module=settings/chromeos/crostini_extra_containers_subpage_test.js';
  }

  /** @override */
  testGenPreamble() {
    return crostiniTestGenPreamble();
  }
};

TEST_F('OSSettingsCrostiniExtraContainerPageV3Test', 'AllJsTests', () => {
  mocha.run();
});

[['AboutPage', 'os_about_page_tests.js'],
 ['ApnDetailDialog', 'apn_detail_dialog_tests.js'],
 [
   'ApnSubpage', 'apn_subpage_tests.js',
   {enabled: ['ash::features::kApnRevamp']}
 ],
 ['AppsPage', 'apps_page_test.js'],
 ['AppNotificationsSubpage', 'app_notifications_subpage_tests.js'],
 ['AppManagementAppDetailsItem', 'app_management/app_details_item_test.js'],
 ['AppManagementAppDetailView', 'app_management/app_detail_view_test.js'],
 ['AppManagementAppItem', 'app_management/app_item_test.js'],
 ['AppManagementArcDetailView', 'app_management/arc_detail_view_test.js'],
 [
   'AppManagementBorealisDetailView',
   'app_management/borealis_detail_view_test.js',
 ],
 [
   'AppManagementChromeAppDetailView',
   'app_management/chrome_app_detail_view_test.js',
 ],
 ['AppManagementDomSwitch', 'app_management/dom_switch_test.js'],
 ['AppManagementFileHandlingItem', 'app_management/file_handling_item_test.js'],
 ['AppManagementMainView', 'app_management/main_view_test.js'],
 ['AppManagementManagedApp', 'app_management/managed_apps_test.js'],
 ['AppManagementPage', 'app_management/app_management_page_tests.js'],
 ['AppManagementPinToShelfItem', 'app_management/pin_to_shelf_item_test.js'],
 [
   'AppManagementPluginVmDetailView',
   'app_management/plugin_vm_detail_view_test.js',
 ],
 ['AppManagementPwaDetailView', 'app_management/pwa_detail_view_test.js'],
 ['AppManagementReducers', 'app_management/reducers_test.js'],
 ['AppManagementResizeLockItem', 'app_management/resize_lock_item_test.js'],
 [
   'AppManagementSupportedLinksItem',
   'app_management/supported_links_item_test.js',
 ],
 ['AppManagementToggleRow', 'app_management/toggle_row_test.js'],
 ['CellularNetworksList', 'cellular_networks_list_test.js'],
 ['CellularRoamingToggleButton', 'cellular_roaming_toggle_button_test.js'],
 [
   'CupsPrinterEntry', 'cups_printer_entry_tests.js',
   {enabled: ['ash::features::kPrinterSettingsRevamp']}
 ],
 ['CupsPrinterLandingPage', 'cups_printer_landing_page_tests.js'],
 [
   'CupsPrinterPage', 'cups_printer_page_tests.js',
   {enabled: ['ash::features::kPrinterSettingsRevamp']}
 ],
 ['DateTimePage', 'date_time_page_tests.js'],
 ['DateTimePageTimezoneSelector', 'date_time_page/timezone_selector_test.js'],
 ['DateTimePageTimezoneSubpage', 'date_time_page/timezone_subpage_test.js'],
 [
   'DevicePageFakeInputDeviceSettingsProvider',
   'device_page/fake_input_device_settings_provider_test.js'
 ],
 [
   'DevicePageInputDeviceMojoInterfaceProvider',
   'device_page/input_device_mojo_interface_provider_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceKeyboard', 'device_page/per_device_keyboard_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceKeyboardRemapKeys',
   'device_page/per_device_keyboard_remap_keys_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceKeyboardSubsection',
   'device_page/per_device_keyboard_subsection_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']},
 ],
 [
   'DevicePagePerDeviceMouse', 'device_page/per_device_mouse_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceMouseSubsection',
   'device_page/per_device_mouse_subsection_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDevicePointingStick',
   'device_page/per_device_pointing_stick_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDeviceTouchpad',
   'device_page/per_device_touchpad_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']},
 ],
 [
   'DevicePagePerDeviceTouchpadSubsection',
   'device_page/per_device_touchpad_subsection_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']}
 ],
 [
   'DevicePagePerDevicePointingStickSubsection',
   'device_page/per_device_pointing_stick_subsection_test.js',
   {enabled: ['ash::features::kInputDeviceSettingsSplit']},
 ],
 ['EsimInstallErrorDialog', 'esim_install_error_dialog_test.js'],
 ['EsimRemoveProfileDialog', 'esim_remove_profile_dialog_test.js'],
 ['EsimRenameDialog', 'esim_rename_dialog_test.js'],
 ['FakeCrosAudioConfig', 'fake_cros_audio_config_test.js'],
 ['FingerprintListSubpage', 'fingerprint_list_subpage_test.js'],
 ['GoogleAssistantSubpage', 'google_assistant_subpage_test.js'],
 ['GuestOsSharedPaths', 'guest_os_shared_paths_test.js'],
 ['GuestOsSharedUsbDevices', 'guest_os/guest_os_shared_usb_devices_test.js'],
 [
   'HotspotConfigDialog',
   'hotspot_config_dialog_tests.js',
   {enabled: ['ash::features::kHotspot']},
 ],
 [
   'HotspotSubpage',
   'hotspot_subpage_tests.js',
   {enabled: ['ash::features::kHotspot']},
 ],
 [
   'HotspotSummaryItem',
   'hotspot_summary_item_tests.js',
   {enabled: ['ash::features::kHotspot']},
 ],
 ['InputMethodOptionPage', 'input_method_options_page_test.js'],
 ['InputPage', 'input_page_test.js'],
 ['InternetConfig', 'internet_config_test.js'],
 ['InternetDetailMenu', 'internet_detail_menu_test.js'],
 ['InternetKnownNetworksSubpage', 'internet_known_networks_subpage_tests.js'],
 [
   'InternetPage', 'internet_page_tests.js',
   {enabled: ['ash::features::kApnRevamp']}
 ],
 [
   'InternetPageCellularSetupDialog',
   'internet_page/cellular_setup_dialog_test.js'
 ],
 [
   'InternetPageInternetDetailSubpage',
   'internet_page/internet_detail_subpage_tests.js',
   {enabled: ['ash::features::kApnRevamp']}
 ],
 ['InternetPageNetworkSummary', 'internet_page/network_summary_test.js'],
 [
   'InternetPageNetworkSummaryItem',
   'internet_page/network_summary_item_test.js'
 ],
 [
   'InternetPageTetherConnectionDialog',
   'internet_page/tether_connection_dialog_test.js'
 ],
 ['InternetSubpage', 'internet_subpage_tests.js'],
 ['InternetSubpageMenu', 'internet_subpage_menu_test.js'],
 ['KerberosPage', 'kerberos_page/kerberos_page_test.js'],
 ['KerberosPageKerberosAccounts', 'kerberos_page/kerberos_accounts_test.js'],
 [
   'KeyboardShortcutBanner',
   'keyboard_shortcut_banner/keyboard_shortcut_banner_test.js'
 ],
 ['LockScreenSubpage', 'lock_screen_subpage_test.js'],
 ['ManageUsersSubpage', 'manage_users_subpage_tests.js'],
 // TODO(b/208932892): Re-enable once flakiness is fixed.
 // ['MultideviceFeatureItem', 'multidevice_feature_item_tests.js'],
 ['MultideviceFeatureToggle', 'multidevice_feature_toggle_tests.js'],
 ['MultidevicePage', 'multidevice_page/multidevice_page_tests.js'],
 [
   'MultidevicePageMultideviceNotificationAccessSetupDialog',
   'multidevice_page/multidevice_notification_access_setup_dialog_tests.js',
 ],
 [
   'MultidevicePageMultidevicePermissionsSetupDialog',
   'multidevice_page/multidevice_permissions_setup_dialog_tests.js',
 ],
 [
   'MultidevicePageMultideviceSmartlockItem',
   'multidevice_page/multidevice_smartlock_item_test.js'
 ],
 [
   'MultidevicePageMultideviceSubPage',
   'multidevice_page/multidevice_subpage_tests.js'
 ],
 [
   'MultiDevicePageMultideviceCombinedSetupItem',
   'multidevice_page/multidevice_combined_setup_item_test.js'
 ],
 [
   'MultidevicePageMultideviceTaskContinuationItem',
   'multidevice_page/multidevice_task_continuation_item_test.js',
 ],
 [
   'MultidevicePageMultideviceWifiSyncItem',
   'multidevice_page/multidevice_wifi_sync_item_test.js'
 ],
 [
   'MultideviceTaskContinuationDisabledLink',
   'multidevice_task_continuation_disabled_link_tests.js',
 ],
 [
   'MultideviceWifiSyncDisabledLink',
   'multidevice_wifi_sync_disabled_link_tests.js',
 ],
 [
   'NearbySharePageNearbyShareConfirmPage',
   'nearby_share_page/nearby_share_confirm_page_test.js'
 ],
 [
   'NearbySharePageNearbyShareHighVisibilityPage',
   'nearby_share_page/nearby_share_high_visibility_page_test.js'
 ],
 ['NearbyShareReceiveDialog', 'nearby_share_receive_dialog_tests.js'],
 [
   'NearbyShareSubpage',
   'nearby_share_subpage_tests.js',
   {enabled: ['features::kNearbySharing']},
 ],
 ['NetworkAlwaysOnVpn', 'network_always_on_vpn_test.js'],
 ['NetworkProxySection', 'network_proxy_section_test.js'],
 ['NetworkDeviceInfoDialog', 'network_device_info_dialog_test.js'],
 ['OncMojoTest', 'onc_mojo_test.js'],
 [
   'OsA11yPage',
   'os_a11y_page/os_a11y_page_test.js',
   {enabled: ['features::kPdfOcr']},
 ],
 [
   'OsA11yPageAudioAndCaptionsPage',
   'os_a11y_page/audio_and_captions_page_test.js',
 ],
 [
   'OsA11yPageChromeVoxSubpage',
   'os_a11y_page/chromevox_subpage_test.js',
   {enabled: ['features::kAccessibilityChromeVoxPageMigration']},
 ],
 [
   'OsA11yPageCursorAndTouchpadPage',
   'os_a11y_page/cursor_and_touchpad_page_test.js',
 ],
 [
   'OsA11yPageChangeDictationLocaleDialog',
   'os_a11y_page/change_dictation_locale_dialog_test.js',
 ],
 [
   'OsA11yPageDisplayAndMagnificationPage',
   'os_a11y_page/display_and_magnification_page_test.js',
   {enabled: ['features::kExperimentalAccessibilityColorEnhancementSettings']},
 ],
 [
   'OsA11yPageKeyboardAndTextInputPage',
   'os_a11y_page/keyboard_and_text_input_page_test.js',
 ],
 [
   'OsA11yPageManageA11ySubpage',
   'os_a11y_page/manage_a11y_subpage_test.js',
 ],
 [
   'OsA11yPageTextToSpeechPage',
   'os_a11y_page/text_to_speech_page_test.js',
   {enabled: ['features::kPdfOcr']},
 ],
 [
   'OsA11yPageTextToSpeechSubpage',
   'os_a11y_page/text_to_speech_subpage_test.js',
 ],
 ['OsA11yPageTtsSubpage', 'os_a11y_page/tts_subpage_test.js'],
 ['OsBluetoothPage', 'os_bluetooth_page/os_bluetooth_page_tests.js'],
 [
   'OsBluetoothPageOsBluetoothChangeDeviceNameDialog',
   'os_bluetooth_page/os_bluetooth_change_device_name_dialog_tests.js',
 ],
 [
   'OsBluetoothPageOsBluetoothDeviceDetailSubpage',
   'os_bluetooth_page/os_bluetooth_device_detail_subpage_tests.js',
 ],
 [
   'OsBluetoothPageOsBluetoothPairingDialog',
   'os_bluetooth_page/os_bluetooth_pairing_dialog_tests.js'
 ],
 [
   'OsBluetoothPageOsBluetoothSummary',
   'os_bluetooth_page/os_bluetooth_summary_tests.js'
 ],
 [
   'OsBluetoothPageOsBluetoothTrueWirelessImages',
   'os_bluetooth_page/os_bluetooth_true_wireless_images_tests.js',
 ],
 ['OsEditDictionaryPage', 'os_edit_dictionary_page_test.js'],
 [
   'OsClearPersonalizationDataPage',
   'os_clear_personalization_data_page_test.js'
 ],
 ['OsFilesPage', 'os_files_page/os_files_page_test.js'],
 ['OsFilesPageGoogleDrivePage', 'os_files_page/google_drive_page_test.js'],
 ['OsFilesPageOfficePage', 'os_files_page/office_page_test.js'],
 ['OsFilesPageSmbSharesPage', 'os_files_page/smb_shares_page_test.js'],
 [
   'OsLanguagesPageSmartInputsPage',
   'os_languages_page/smart_inputs_page_test.js'
 ],
 ['OsLanguagesPageV2', 'os_languages_page_v2_tests.js'],
 ['OsPairedBluetoothList', 'os_paired_bluetooth_list_tests.js'],
 ['OsPairedBluetoothListItem', 'os_paired_bluetooth_list_item_tests.js'],
 ['OsPeoplePageAddUserDialog', 'os_people_page/add_user_dialog_test.js'],
 ['OsPrintingPage', 'os_printing_page/os_printing_page_test.js'],
 [
   'OsPrivacyPageSmartPrivacySubpage',
   'os_privacy_page/smart_privacy_subpage_test.js'
 ],
 ['OsSearchPageSearchSubpage', 'os_search_page/search_subpage_test.js'],
 ['OsSettingsHatsUi', 'os_settings_ui/os_settings_hats_ui_test.js'],
 ['OsSettingsPage', 'os_settings_page_test.js'],
 ['OsSettingsUi', 'os_settings_ui/os_settings_ui_test.js'],
 ['OsSettingsUiAboutPage', 'os_settings_ui/os_settings_ui_about_page_test.js'],
 ['OsSettingsUiMenu', 'os_settings_ui/os_settings_ui_menu_test.js'],
 ['OsSettingsUiToolbar', 'os_settings_ui/os_settings_ui_toolbar_test.js'],
 [
   'OsSettingsUiUserActionRecorder',
   'os_settings_ui/user_action_recorder_test.js'
 ],
 ['OsSettingsMain', 'os_settings_main_test.js'],
 ['OsSearchPage', 'os_search_page_test.js'],
 ['OsSettingsSearchBox', 'os_settings_search_box_test.js'],
 ['OsSettingsMenu', 'os_settings_menu_test.js'],
 ['OsSyncControlsSubpage', 'os_sync_controls_subpage_test.js'],
 [
   'ParentalControlsPage',
   'parental_controls_page/parental_controls_page_test.js'
 ],
 ['PeoplePage', 'os_people_page_test.js'],
 [
   'PeoplePageAccountManagerSubpage',
   'people_page_account_manager_subpage_test.js',
   {disabled: ['ash::features::kLacrosSupport']},
 ],
 [
   'PeoplePageAccountManagerSubpageWithArcAccountRestrictionsEnabled',
   'people_page_account_manager_subpage_test.js',
   {enabled: ['ash::features::kLacrosSupport']},
 ],
 ['PeoplePageQuickUnlock', 'quick_unlock_authenticate_browsertest_chromeos.js'],
 [
   'PersonalizationPageWithPersonalizationHub',
   'personalization_page_with_personalization_hub_test.js',
 ],
 [
   'PrinterStatus',
   'printer_status_tests.js',
 ],
 [
   'PrivacyHubSubpage',
   'privacy_hub_subpage_tests.js',
   {enabled: ['ash::features::kCrosPrivacyHub']},
 ],
 ['PrivacyPage', 'os_privacy_page_test.js'],
 ['ResetPage', 'os_reset_page_test.js'],
 ['SettingsSchedulerSlider', 'settings_scheduler_slider_test.js'],
 [
   'SelectToSpeakSubpage',
   'select_to_speak_subpage_tests.js',
   {enabled: ['features::kAccessibilitySelectToSpeakPageMigration']},
 ],
 ['SettingsTrafficCounters', 'settings_traffic_counters_test.js'],
 [
   'SwitchAccessActionAssignmentDialog',
   'switch_access_action_assignment_dialog_test.js',
 ],
 ['SwitchAccessSetupGuideDialog', 'switch_access_setup_guide_dialog_test.js'],
 ['SwitchAccessSubpage', 'switch_access_subpage_tests.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, featureList) {
  const className = `OSSettings${testName}V3Test`;
  this[className] = class extends OSSettingsV3BrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://os-settings/test_loader.html?module=settings/chromeos/${
          module}`;
    }
  };

  if (featureList) {
    Object.defineProperty(this[className].prototype, 'featureList', {
      get() {
        return featureList;
      },
    });
  }

  // AboutPage has a test suite that can only succeed on official builds where
  // the is_chrome_branded build flag is enabled.
  if (testName === 'AboutPage') {
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha.grep('/^(?!AboutPageTest_OfficialBuild).*$/').run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('AboutPageTest_OfficialBuild').run();
    });
    GEN('#endif');
  } else if (testName === 'PrivacyHubSubpage') {
    // PrivacyHubSubpage has a test suite that can only succeed on official
    // builds where the is_chrome_branded build flag is enabled.
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha.grep('/^(?!PrivacyHubSubpageTest_OfficialBuild).*$/').run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('PrivacyHubSubpageTest_OfficialBuild').run();
    });
    GEN('#endif');
  } else if (testName === 'OsSettingsSearchBox') {
    TEST_F(className, 'AllBuilds' || 'All', () => {
      mocha.grep('/^(?!(OSSettingsSearchBox SearchFeedback_OfficialBuild)).*$/')
          .run();
    });

    GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
    TEST_F(className, 'OfficialBuild' || 'All', () => {
      mocha.grep('SearchFeedback_OfficialBuild').run();
    });
    GEN('#endif');
  } else {
    TEST_F(className, 'All', () => mocha.run());
  }
}
