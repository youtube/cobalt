// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The main entry point for the ChromeOS Settings SWA. This
 * imports all of the necessary modules and custom elements to load the page.
 */

/** Necessary imports to load the app */
import './strings.m.js';
import './os_settings_ui/os_settings_ui.js';
// TODO(b/263414034) Determine if these imports are needed here at all,
// or should be moved to lazy_load.ts
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import './device_page/audio.js';
import './device_page/cros_audio_config.js';
import './device_page/customize_mouse_buttons_subpage.js';
import './device_page/customize_pen_buttons_subpage.js';
import './device_page/customize_tablet_buttons_subpage.js';
import './device_page/device_page.js';
import './device_page/display.js';
import './device_page/display_layout.js';
import './device_page/display_overscan_dialog.js';
import './device_page/display_settings_mojo_interface_provider.js';
import './device_page/fake_input_device_data.js';
import './device_page/fake_input_device_settings_provider.js';
import './device_page/fkey_row.js';
import './device_page/input_device_mojo_interface_provider.js';
import './device_page/input_device_settings_types.js';
import './device_page/graphics_tablet_subpage.js';
import './device_page/keyboard.js';
import './device_page/keyboard_six_pack_key_row.js';
import './device_page/per_device_keyboard.js';
import './device_page/per_device_keyboard_remap_keys.js';
import './device_page/per_device_keyboard_subsection.js';
import './device_page/per_device_mouse.js';
import './device_page/per_device_mouse_subsection.js';
import './device_page/per_device_pointing_stick.js';
import './device_page/per_device_pointing_stick_subsection.js';
import './device_page/per_device_touchpad.js';
import './device_page/per_device_touchpad_subsection.js';
import './device_page/pointers.js';
import './device_page/power.js';
import './device_page/keyboard_remap_modifier_key_row.js';
import './device_page/storage.js';
import './device_page/storage_external.js';
import './device_page/storage_external_entry.js';
import './device_page/stylus.js';
import './multidevice_page/multidevice_page.js';
import './nearby_share_page/nearby_share_high_visibility_page.js';
import './os_files_page/google_drive_subpage.js';
import './os_apps_page/android_apps_subpage.js';
import './os_apps_page/app_notifications_page/app_notifications_subpage.js';
import './os_apps_page/app_notifications_page/app_notifications_manager_subpage.js';
import './os_apps_page/app_management_page/app_management_cros_shared_style.css.js';
import './os_apps_page/app_management_page/app_management_cros_shared_vars.css.js';
import './os_apps_page/app_notifications_page/mojo_interface_provider.js';
import './os_apps_page/os_apps_page.js';
import './os_bluetooth_page/os_bluetooth_saved_devices_subpage.js';
import './os_bluetooth_page/os_remove_saved_device_dialog.js';
import './os_bluetooth_page/os_bluetooth_forget_device_dialog.js';
import './os_bluetooth_page/os_bluetooth_true_wireless_images.js';
import './os_bluetooth_page/os_bluetooth_pairing_dialog.js';
import './os_bluetooth_page/os_bluetooth_page.js';
import './os_bluetooth_page/os_bluetooth_summary.js';
import './os_bluetooth_page/os_saved_devices_list.js';
import './os_bluetooth_page/os_saved_devices_list_item.js';
import './os_bluetooth_page/settings_fast_pair_constants.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

/**
 * With the optimize_webui() build step, the generated JS files are bundled
 * into a single JS file. The exports below are necessary so they can be
 * imported into browser tests.
 */
export {getContactManager, observeContactManager, setContactManagerForTesting} from '/shared/nearby_contact_manager.js';
export {NearbyProgressElement} from '/shared/nearby_progress.js';
export * as nearbyShareMojom from '/shared/nearby_share.mojom-webui.js';
export {getNearbyShareSettings, observeNearbyShareSettings, setNearbyShareSettingsForTesting} from '/shared/nearby_share_settings.js';
export {NearbySettings, NearbyShareSettingsMixin} from '/shared/nearby_share_settings_mixin.js';
export {ControlledButtonElement} from '/shared/settings/controls/controlled_button.js';
export {ControlledRadioButtonElement} from '/shared/settings/controls/controlled_radio_button.js';
export {SettingsDropdownMenuElement} from '/shared/settings/controls/settings_dropdown_menu.js';
export {SettingsSliderElement} from '/shared/settings/controls/settings_slider.js';
export {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
export {LifetimeBrowserProxy, LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
export {ProfileInfo, ProfileInfoBrowserProxy, ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
export {PageStatus, StatusAction, StoredAccount, SyncBrowserProxy, SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
export {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
export {ApnDetailDialog} from 'chrome://resources/ash/common/network/apn_detail_dialog.js';
export {AppManagementFileHandlingItemElement} from 'chrome://resources/cr_components/app_management/file_handling_item.js';
export {AppManagementSupportedLinksItemElement} from 'chrome://resources/cr_components/app_management/supported_links_item.js';
export {AppManagementToggleRowElement} from 'chrome://resources/cr_components/app_management/toggle_row.js';
export {LocalizedLinkElement} from 'chrome://resources/cr_components/localized_link/localized_link.js';
export {SettingsPrefsElement} from 'chrome://resources/cr_components/settings_prefs/prefs.js';
export {CrSettingsPrefs} from 'chrome://resources/cr_components/settings_prefs/prefs_types.js';
export {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
export {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
export {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
export {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
export {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
export {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
export {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
export {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
export {CrCardRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_card_radio_button.js';
export {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
export {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
export {CrSearchableDropDownElement} from 'chrome://resources/cr_elements/cr_searchable_drop_down/cr_searchable_drop_down.js';
export {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
export {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
export {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
export {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
export {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
export {CrPolicyIndicatorElement} from 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
export {CrTooltipIconElement} from 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
export {OpenWindowProxy, OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
export {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
export {IronDropdownElement} from 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
export {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
export {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
export {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
export {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
export {resetGlobalScrollTargetForTesting, setGlobalScrollTarget as setGlobalScrollTargetForTesting} from './common/global_scroll_target_mixin.js';
export {LacrosExtensionControlBrowserProxy, LacrosExtensionControlBrowserProxyImpl} from './common/lacros_extension_control_browser_proxy.js';
export {LacrosExtensionControlledIndicatorElement} from './common/lacros_extension_controlled_indicator.js';
export {PrefsState} from './common/types.js';
export {SettingsAudioElement} from './device_page/audio.js';
export {setCrosAudioConfigForTesting} from './device_page/cros_audio_config.js';
export {SettingsDevicePageElement} from './device_page/device_page.js';
export {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, NoteAppInfo, NoteAppLockScreenSupport, setDisplayApiForTesting, StorageSpaceState} from './device_page/device_page_browser_proxy.js';
export {SettingsDisplayElement} from './device_page/display.js';
export {getDisplaySettingsProvider, setDisplaySettingsProviderForTesting} from './device_page/display_settings_mojo_interface_provider.js';
export * as fakeCrosAudioConfig from './device_page/fake_cros_audio_config.js';
export {fakeGraphicsTabletButtonActions, fakeGraphicsTablets, fakeKeyboards, fakeKeyboards2, fakeMice, fakeMice2, fakeMouseButtonActions, fakePointingSticks, fakePointingSticks2, fakeStyluses, fakeTouchpads, fakeTouchpads2} from './device_page/fake_input_device_data.js';
export {FakeInputDeviceSettingsProvider} from './device_page/fake_input_device_settings_provider.js';
export {FkeyRowElement} from './device_page/fkey_row.js';
export {SettingsGraphicsTabletSubpageElement} from './device_page/graphics_tablet_subpage.js';
export {getInputDeviceSettingsProvider, setInputDeviceSettingsProviderForTesting, setupFakeInputDeviceSettingsProvider} from './device_page/input_device_mojo_interface_provider.js';
export {Fkey, GraphicsTablet, InputDeviceSettingsPolicy, Keyboard, KeyEvent, MetaKey, ModifierKey, Mouse, PolicyStatus, ShortcutInputProviderInterface, SimulateRightClickModifier, SixPackKey, SixPackKeyInfo, SixPackShortcutModifier, TopRowActionKey, Vkey} from './device_page/input_device_settings_types.js';
export {KeyboardRemapModifierKeyRowElement} from './device_page/keyboard_remap_modifier_key_row.js';
export {KeyboardSixPackKeyRowElement, sixPackKeyProperties} from './device_page/keyboard_six_pack_key_row.js';
export {SettingsPerDeviceKeyboardElement} from './device_page/per_device_keyboard.js';
export {SettingsPerDeviceKeyboardRemapKeysElement} from './device_page/per_device_keyboard_remap_keys.js';
export {SettingsPerDeviceKeyboardSubsectionElement} from './device_page/per_device_keyboard_subsection.js';
export {SettingsPerDeviceMouseElement} from './device_page/per_device_mouse.js';
export {SettingsPerDeviceMouseSubsectionElement} from './device_page/per_device_mouse_subsection.js';
export {SettingsPerDevicePointingStickElement} from './device_page/per_device_pointing_stick.js';
export {SettingsPerDevicePointingStickSubsectionElement} from './device_page/per_device_pointing_stick_subsection.js';
export {SettingsPerDeviceTouchpadElement} from './device_page/per_device_touchpad.js';
export {SettingsPerDeviceTouchpadSubsectionElement} from './device_page/per_device_touchpad_subsection.js';
export {getShortcutInputProvider} from './device_page/shortcut_input_mojo_interface_provider.js';
export {ensureLazyLoaded} from './ensure_lazy_loaded.js';
export {OsSettingsCellularSetupDialogElement} from './internet_page/cellular_setup_dialog.js';
export {EsimRenameDialogElement} from './internet_page/esim_rename_dialog.js';
export {HotspotConfigDialogElement, WiFiSecurityType} from './internet_page/hotspot_config_dialog.js';
export {HotspotSummaryItemElement} from './internet_page/hotspot_summary_item.js';
export {InternetConfigElement} from './internet_page/internet_config.js';
export {SettingsInternetDetailMenuElement} from './internet_page/internet_detail_menu.js';
export {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page/internet_page_browser_proxy.js';
export {NetworkSummaryElement} from './internet_page/network_summary.js';
export {NetworkSummaryItemElement} from './internet_page/network_summary_item.js';
export {SettingsKerberosPageElement} from './kerberos_page/kerberos_page.js';
export {MainPageContainerElement} from './main_page_container/main_page_container.js';
export {PageDisplayerElement} from './main_page_container/page_displayer.js';
export {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from './metrics_recorder.js';
export * as appNotificationHandlerMojom from './mojom-webui/app_notification_handler.mojom-webui.js';
export * as crosAudioConfigMojom from './mojom-webui/cros_audio_config.mojom-webui.js';
export * as displaySettingsProviderMojom from './mojom-webui/display_settings_provider.mojom-webui.js';
export * as personalizationSearchMojom from './mojom-webui/personalization_search.mojom-webui.js';
export * as routesMojom from './mojom-webui/routes.mojom-webui.js';
export * as searchMojom from './mojom-webui/search.mojom-webui.js';
export * as searchResultIconMojom from './mojom-webui/search_result_icon.mojom-webui.js';
export * as settingMojom from './mojom-webui/setting.mojom-webui.js';
export * as userActionRecorderMojom from './mojom-webui/user_action_recorder.mojom-webui.js';
export {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_page/multidevice_browser_proxy.js';
export {MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubFeatureAccessProhibitedReason, PhoneHubFeatureAccessStatus, PhoneHubPermissionsSetupAction, PhoneHubPermissionsSetupFeatureCombination, PhoneHubPermissionsSetupFlowScreens, PhoneHubPermissionsSetupMode} from './multidevice_page/multidevice_constants.js';
export {NotificationAccessSetupOperationStatus, SettingsMultideviceNotificationAccessSetupDialogElement} from './multidevice_page/multidevice_notification_access_setup_dialog.js';
export {SettingsMultidevicePageElement} from './multidevice_page/multidevice_page.js';
export {PermissionsSetupStatus, SettingsMultidevicePermissionsSetupDialogElement, SetupFlowStatus} from './multidevice_page/multidevice_permissions_setup_dialog.js';
export {Account, NearbyAccountManagerBrowserProxy, NearbyAccountManagerBrowserProxyImpl} from './nearby_share_page/nearby_account_manager_browser_proxy.js';
export {NearbyShareConfirmPageElement} from './nearby_share_page/nearby_share_confirm_page.js';
export {NearbyShareHighVisibilityPageElement} from './nearby_share_page/nearby_share_high_visibility_page.js';
export {getReceiveManager, observeReceiveManager, setReceiveManagerForTesting} from './nearby_share_page/nearby_share_receive_manager.js';
export {dataUsageStringToEnum, NearbyShareDataUsage} from './nearby_share_page/types.js';
export {ChromeVoxSubpageBrowserProxy, ChromeVoxSubpageBrowserProxyImpl} from './os_a11y_page/chromevox_subpage_browser_proxy.js';
export {OsSettingsA11yPageElement} from './os_a11y_page/os_a11y_page.js';
export {OsA11yPageBrowserProxy, OsA11yPageBrowserProxyImpl} from './os_a11y_page/os_a11y_page_browser_proxy.js';
export {SelectToSpeakSubpageBrowserProxy, SelectToSpeakSubpageBrowserProxyImpl} from './os_a11y_page/select_to_speak_subpage_browser_proxy.js';
export {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './os_a11y_page/switch_access_subpage_browser_proxy.js';
export {TextToSpeechSubpageBrowserProxy, TextToSpeechSubpageBrowserProxyImpl} from './os_a11y_page/text_to_speech_subpage_browser_proxy.js';
export {TtsVoiceSubpageBrowserProxy, TtsVoiceSubpageBrowserProxyImpl} from './os_a11y_page/tts_voice_subpage_browser_proxy.js';
export {AboutPageBrowserProxyImpl, BrowserChannel, UpdateStatus} from './os_about_page/about_page_browser_proxy.js';
export {DeviceNameBrowserProxyImpl} from './os_about_page/device_name_browser_proxy.js';
export {DeviceNameState, SetDeviceNameResult} from './os_about_page/device_name_util.js';
export {AndroidAppsBrowserProxy, AndroidAppsBrowserProxyImpl} from './os_apps_page/android_apps_browser_proxy.js';
export {addApp, AppManagementActions, changeApp, removeApp, updateSelectedAppId, updateSubAppToParentAppId} from './os_apps_page/app_management_page/actions.js';
export {AppManagementBrowserProxy, AppManagementComponentBrowserProxy} from './os_apps_page/app_management_page/browser_proxy.js';
export {PluginVmBrowserProxy, PluginVmBrowserProxyImpl} from './os_apps_page/app_management_page/plugin_vm_page/plugin_vm_browser_proxy.js';
export {reduceAction, updateApps} from './os_apps_page/app_management_page/reducers.js';
export {AppManagementPageState, AppManagementStore} from './os_apps_page/app_management_page/store.js';
export {AppManagementStoreMixin} from './os_apps_page/app_management_page/store_mixin.js';
export {setAppNotificationProviderForTesting} from './os_apps_page/app_notifications_page/mojo_interface_provider.js';
export {OsSettingsAppsPageElement} from './os_apps_page/os_apps_page.js';
export {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_page/os_bluetooth_devices_subpage_browser_proxy.js';
export {SettingsBluetoothPageElement} from './os_bluetooth_page/os_bluetooth_page.js';
export {SettingsBluetoothPairingDialogElement} from './os_bluetooth_page/os_bluetooth_pairing_dialog.js';
export {SettingsBluetoothSummaryElement} from './os_bluetooth_page/os_bluetooth_summary.js';
export {FastPairSavedDevice, FastPairSavedDevicesOptInStatus} from './os_bluetooth_page/settings_fast_pair_constants.js';
export {GoogleDriveBrowserProxy, GoogleDrivePageCallbackRouter, GoogleDrivePageHandlerRemote, GoogleDrivePageRemote, Stage} from './os_files_page/google_drive_browser_proxy.js';
export {ConfirmationDialogType, SettingsGoogleDriveSubpageElement} from './os_files_page/google_drive_subpage.js';
export {OneDriveBrowserProxy, OneDrivePageCallbackRouter, OneDrivePageHandlerRemote, OneDrivePageRemote} from './os_files_page/one_drive_browser_proxy.js';
export {createPageAvailability as createPageAvailabilityForTesting, OsPageAvailability} from './os_page_availability.js';
export {AccountManagerSettingsCardElement} from './os_people_page/account_manager_settings_card.js';
export {AdditionalAccountsSettingsCardElement} from './os_people_page/additional_accounts_settings_card.js';
export {OsSettingsPeoplePageElement} from './os_people_page/os_people_page.js';
export {MetricsConsentBrowserProxy, MetricsConsentBrowserProxyImpl, MetricsConsentState} from './os_privacy_page/metrics_consent_browser_proxy.js';
export {OsSettingsPrivacyPageElement} from './os_privacy_page/os_privacy_page.js';
export {DataAccessPolicyState, PeripheralDataAccessBrowserProxy, PeripheralDataAccessBrowserProxyImpl} from './os_privacy_page/peripheral_data_access_browser_proxy.js';
export {OsResetBrowserProxy, OsResetBrowserProxyImpl} from './os_reset_page/os_reset_browser_proxy.js';
export {OsSettingsSearchPageElement} from './os_search_page/os_search_page.js';
export {SearchAndAssistantSettingsCardElement} from './os_search_page/search_and_assistant_settings_card.js';
export {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './os_search_page/search_engines_browser_proxy.js';
export {OsSettingsMainElement} from './os_settings_main/os_settings_main.js';
export {OsSettingsMenuItemElement} from './os_settings_menu/menu_item.js';
export {OsSettingsMenuElement} from './os_settings_menu/os_settings_menu.js';
export {OsSettingsSubpageElement} from './os_settings_page/os_settings_subpage.js';
export {SettingsCardElement} from './os_settings_page/settings_card.js';
export {SettingsIdleLoadElement} from './os_settings_page/settings_idle_load.js';
export {OsSettingsRoutes} from './os_settings_routes.js';
export {OsSettingsSearchBoxElement} from './os_settings_search_box/os_settings_search_box.js';
export {OsSettingsSearchBoxBrowserProxy, OsSettingsSearchBoxBrowserProxyImpl} from './os_settings_search_box/os_settings_search_box_browser_proxy.js';
export {OsSettingsHatsBrowserProxy, OsSettingsHatsBrowserProxyImpl} from './os_settings_ui/os_settings_hats_browser_proxy.js';
export {OsSettingsUiElement} from './os_settings_ui/os_settings_ui.js';
export {OsToolbarElement} from './os_toolbar/os_toolbar.js';
export {ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from './parental_controls_page/parental_controls_browser_proxy.js';
export {SettingsParentalControlsPageElement} from './parental_controls_page/parental_controls_page.js';
export {ParentalControlsSettingsCardElement} from './parental_controls_page/parental_controls_settings_card.js';
export {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_page/personalization_hub_browser_proxy.js';
export {SettingsPersonalizationPageElement} from './personalization_page/personalization_page.js';
export {createRouter as createRouterForTesting, Route, Router, routes} from './router.js';
export {getPersonalizationSearchHandler, setPersonalizationSearchHandlerForTesting} from './search/personalization_search_handler.js';
export {getSettingsSearchHandler, setSettingsSearchHandlerForTesting} from './search/settings_search_handler.js';
export {SettingsSchedulerSliderElement} from './settings_scheduler_slider/settings_scheduler_slider.js';
export {StartupSettingsCardElement} from './system_preferences_page/startup_settings_card.js';
export {StorageAndPowerSettingsCardElement} from './system_preferences_page/storage_and_power_settings_card.js';
export {SettingsSystemPreferencesPageElement} from './system_preferences_page/system_preferences_page.js';

// TODO(b/257329722) After the Jelly experiment is launched, add the CSS link
// element directly to the HTML.
const jellyEnabled = loadTimeData.getBoolean('isJellyEnabled');
if (jellyEnabled) {
  const link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = 'chrome://theme/colors.css?sets=legacy,sys';
  document.head.appendChild(link);
  document.body.classList.add('jelly-enabled');

  const fontLink = document.createElement('link');
  fontLink.rel = 'stylesheet';
  fontLink.href = 'chrome://theme/typography.css';
  document.head.appendChild(fontLink);
}

window.addEventListener('load', () => {
  if (jellyEnabled) {
    ColorChangeUpdater.forDocument().start();
  }
});
