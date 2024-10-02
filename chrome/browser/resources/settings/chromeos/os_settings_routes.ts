// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import * as routesMojom from './mojom-webui/routes.mojom-webui.js';

/** Class for navigable routes. */
export class Route {
  depth: number;
  isNavigableDialog: boolean;
  section: string;
  title: string|undefined;
  parent: Route|null;
  path: string;

  constructor(path: string, title?: string) {
    this.path = path;
    this.title = title;
    this.parent = null;
    this.depth = 0;

    /**
     * Whether this route corresponds to a navigable dialog. Those routes must
     * belong to a "section".
     */
    this.isNavigableDialog = false;

    // Below are all legacy properties to provide compatibility with the old
    // routing system.
    this.section = '';
  }

  /**
   * Returns a new Route instance that's a child of this route.
   */
  createChild(path: string, title?: string): Route {
    assert(path);

    // |path| extends this route's path if it doesn't have a leading slash.
    // If it does have a leading slash, it's just set as the child route's path
    const childPath = path[0] === '/' ? path : `${this.path}/${path}`;

    const route = new Route(childPath, title);
    route.parent = this;
    route.section = this.section;
    route.depth = this.depth + 1;
    return route;
  }

  /**
   * Returns a new Route instance that's a child section of this route.
   * TODO(tommycli): Remove once we've obsoleted the concept of sections.
   */
  createSection(path: string, section: string, title?: string): Route {
    const route = this.createChild(path, title);
    route.section = section;
    return route;
  }

  /**
   * Returns the absolute path string for this Route, assuming this function
   * has been called from within chrome://os-settings.
   */
  getAbsolutePath(): string {
    return window.location.origin + this.path;
  }

  /**
   * Returns true if this route matches or is an ancestor of the parameter.
   */
  contains(route: Route): boolean {
    for (let curr: Route|null = route; curr !== null; curr = curr.parent) {
      if (this === curr) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns true if this route is a subpage of a section.
   */
  isSubpage(): boolean {
    return !this.isNavigableDialog && !!this.parent && !!this.section &&
        this.parent.section === this.section;
  }
}

interface MinimumRoutes {
  BASIC: Route;
  ADVANCED: Route;
  ABOUT: Route;
}

/**
 * Specifies all possible routes in CrOS Settings. Keep routes alphabetized.
 */
export interface OsSettingsRoutes extends MinimumRoutes {
  A11Y_AUDIO_AND_CAPTIONS: Route;
  A11Y_CURSOR_AND_TOUCHPAD: Route;
  A11Y_DISPLAY_AND_MAGNIFICATION: Route;
  A11Y_KEYBOARD_AND_TEXT_INPUT: Route;
  A11Y_TEXT_TO_SPEECH: Route;
  A11Y_CHROMEVOX: Route;
  A11Y_SELECT_TO_SPEAK: Route;
  ABOUT: Route;
  ABOUT_ABOUT: Route;
  ACCOUNTS: Route;
  ACCOUNT_MANAGER: Route;
  ADVANCED: Route;
  APN: Route;
  APP_NOTIFICATIONS: Route;
  APP_MANAGEMENT: Route;
  APP_MANAGEMENT_DETAIL: Route;
  APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS: Route;
  APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES: Route;
  APPS: Route;
  ANDROID_APPS_DETAILS: Route;
  ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES: Route;
  AUDIO: Route;
  CROSTINI: Route;
  CROSTINI_ANDROID_ADB: Route;
  CROSTINI_DETAILS: Route;
  CROSTINI_DISK_RESIZE: Route;
  CROSTINI_EXPORT_IMPORT: Route;
  CROSTINI_EXTRA_CONTAINERS: Route;
  CROSTINI_PORT_FORWARDING: Route;
  CROSTINI_SHARED_PATHS: Route;
  CROSTINI_SHARED_USB_DEVICES: Route;
  BASIC: Route;
  BLUETOOTH: Route;
  BLUETOOTH_DEVICES: Route;
  BLUETOOTH_DEVICE_DETAIL: Route;
  BLUETOOTH_SAVED_DEVICES: Route;
  BRUSCHETTA_DETAILS: Route;
  BRUSCHETTA_SHARED_USB_DEVICES: Route;
  BRUSCHETTA_SHARED_PATHS: Route;
  CHANGE_PICTURE: Route;
  CUPS_PRINTERS: Route;
  DARK_MODE: Route;
  DATETIME: Route;
  DATETIME_TIMEZONE_SUBPAGE: Route;
  DETAILED_BUILD_INFO: Route;
  DEVICE: Route;
  DISPLAY: Route;
  EXTERNAL_STORAGE_PREFERENCES: Route;
  FINGERPRINT: Route;
  FILES: Route;
  GOOGLE_ASSISTANT: Route;
  GOOGLE_DRIVE: Route;
  HOTSPOT_DETAIL: Route;
  INTERNET: Route;
  INTERNET_NETWORKS: Route;
  KERBEROS: Route;
  KERBEROS_ACCOUNTS_V2: Route;
  KEYBOARD: Route;
  KNOWN_NETWORKS: Route;
  LOCK_SCREEN: Route;
  MANAGE_ACCESSIBILITY: Route;
  MANAGE_SWITCH_ACCESS_SETTINGS: Route;
  MANAGE_TTS_SETTINGS: Route;
  MULTIDEVICE: Route;
  MULTIDEVICE_FEATURES: Route;
  NEARBY_SHARE: Route;
  NETWORK_DETAIL: Route;
  OFFICE: Route;
  ON_STARTUP: Route;
  OS_ACCESSIBILITY: Route;
  OS_LANGUAGES: Route;
  OS_LANGUAGES_EDIT_DICTIONARY: Route;
  OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY: Route;
  OS_LANGUAGES_INPUT: Route;
  OS_LANGUAGES_INPUT_METHOD_OPTIONS: Route;
  OS_LANGUAGES_LANGUAGES: Route;
  OS_LANGUAGES_SMART_INPUTS: Route;
  OS_PRINTING: Route;
  OS_PRIVACY: Route;
  OS_RESET: Route;
  OS_SEARCH: Route;
  OS_SYNC: Route;
  OS_PEOPLE: Route;
  PER_DEVICE_KEYBOARD: Route;
  PER_DEVICE_KEYBOARD_REMAP_KEYS: Route;
  PER_DEVICE_MOUSE: Route;
  PER_DEVICE_POINTING_STICK: Route;
  PER_DEVICE_TOUCHPAD: Route;
  PERSONALIZATION: Route;
  POINTERS: Route;
  POWER: Route;
  PRIVACY: Route;
  PRIVACY_HUB: Route;
  SEARCH: Route;
  SEARCH_SUBPAGE: Route;
  SMART_PRIVACY: Route;
  SMB_SHARES: Route;
  STORAGE: Route;
  STYLUS: Route;
  SYNC: Route;
  SYNC_ADVANCED: Route;
}

function createSection(
    parent: Route, path: string, _section: routesMojom.Section): Route {
  // TODO(khorimoto): Add |section| to the the Route object.
  return parent.createSection(`/${path}`, /*section=*/ path);
}

function createSubpage(
    parent: Route, path: string, _subpage: routesMojom.Subpage): Route {
  // TODO(khorimoto): Add |subpage| to the Route object.
  return parent.createChild('/' + path);
}

/**
 * Creates Route objects for each path corresponding to CrOS settings content.
 */
function createOsSettingsRoutes(): OsSettingsRoutes {
  const r: Partial<OsSettingsRoutes> = {};
  const {Section, Subpage} = routesMojom;

  // Special routes:
  // - BASIC is the main page which loads if no path is provided
  // - ADVANCED is the bottom section of the main page which is not
  //   visible unless the user enables it
  r.BASIC = new Route('/');
  r.ADVANCED = new Route('/advanced');

  // Network section.
  r.INTERNET = createSection(
      r.BASIC, routesMojom.NETWORK_SECTION_PATH, Section.kNetwork);
  // Note: INTERNET_NETWORKS and NETWORK_DETAIL are special cases because they
  // includes several subpages, one per network type. Default to kWifiNetworks
  // and kWifiDetails subpages.
  r.INTERNET_NETWORKS =
      createSubpage(r.INTERNET, 'networks', Subpage.kWifiNetworks);
  r.NETWORK_DETAIL =
      createSubpage(r.INTERNET, 'networkDetail', Subpage.kWifiDetails);
  r.KNOWN_NETWORKS = createSubpage(
      r.INTERNET, routesMojom.KNOWN_NETWORKS_SUBPAGE_PATH,
      Subpage.kKnownNetworks);
  if (loadTimeData.getBoolean('isHotspotEnabled')) {
    r.HOTSPOT_DETAIL =
        createSubpage(r.INTERNET, 'hotspotDetail', Subpage.kHotspotDetails);
  }
  if (loadTimeData.getBoolean('isApnRevampEnabled')) {
    r.APN =
        createSubpage(r.INTERNET, routesMojom.APN_SUBPAGE_PATH, Subpage.kApn);
  }

  // Bluetooth section.
  r.BLUETOOTH = createSection(
      r.BASIC, routesMojom.BLUETOOTH_SECTION_PATH, Section.kBluetooth);
  r.BLUETOOTH_DEVICES = createSubpage(
      r.BLUETOOTH, routesMojom.BLUETOOTH_DEVICES_SUBPAGE_PATH,
      Subpage.kBluetoothDevices);
  r.BLUETOOTH_DEVICE_DETAIL = createSubpage(
      r.BLUETOOTH, routesMojom.BLUETOOTH_DEVICE_DETAIL_SUBPAGE_PATH,
      Subpage.kBluetoothDeviceDetail);
  if (loadTimeData.getBoolean('enableSavedDevicesFlag')) {
    r.BLUETOOTH_SAVED_DEVICES = createSubpage(
        r.BLUETOOTH, routesMojom.BLUETOOTH_SAVED_DEVICES_SUBPAGE_PATH,
        Subpage.kBluetoothSavedDevices);
  }

  // MultiDevice section.
  if (!loadTimeData.getBoolean('isGuest')) {
    r.MULTIDEVICE = createSection(
        r.BASIC, routesMojom.MULTI_DEVICE_SECTION_PATH, Section.kMultiDevice);
    r.MULTIDEVICE_FEATURES = createSubpage(
        r.MULTIDEVICE, routesMojom.MULTI_DEVICE_FEATURES_SUBPAGE_PATH,
        Subpage.kMultiDeviceFeatures);
    if (loadTimeData.getBoolean('isNearbyShareSupported')) {
      r.NEARBY_SHARE = createSubpage(
          r.MULTIDEVICE, routesMojom.NEARBY_SHARE_SUBPAGE_PATH,
          Subpage.kNearbyShare);
    }
  }

  // People section.
  if (!loadTimeData.getBoolean('isGuest')) {
    r.OS_PEOPLE = createSection(
        r.BASIC, routesMojom.PEOPLE_SECTION_PATH, Section.kPeople);
    r.ACCOUNT_MANAGER = createSubpage(
        r.OS_PEOPLE, routesMojom.MY_ACCOUNTS_SUBPAGE_PATH, Subpage.kMyAccounts);
    r.OS_SYNC = createSubpage(
        r.OS_PEOPLE, routesMojom.SYNC_SUBPAGE_PATH, Subpage.kSync);
    r.SYNC = createSubpage(
        r.OS_PEOPLE, routesMojom.SYNC_SETUP_SUBPAGE_PATH, Subpage.kSyncSetup);
  }

  // Kerberos section.
  if (loadTimeData.valueExists('isKerberosEnabled') &&
      loadTimeData.getBoolean('isKerberosEnabled')) {
    r.KERBEROS = createSection(
        r.BASIC, routesMojom.KERBEROS_SECTION_PATH, Section.kKerberos);
    r.KERBEROS_ACCOUNTS_V2 = createSubpage(
        r.KERBEROS, routesMojom.KERBEROS_ACCOUNTS_V2_SUBPAGE_PATH,
        Subpage.kKerberosAccountsV2);
  }

  // Device section.
  r.DEVICE =
      createSection(r.BASIC, routesMojom.DEVICE_SECTION_PATH, Section.kDevice);
  r.POINTERS = createSubpage(
      r.DEVICE, routesMojom.POINTERS_SUBPAGE_PATH, Subpage.kPointers);
  r.KEYBOARD = createSubpage(
      r.DEVICE, routesMojom.KEYBOARD_SUBPAGE_PATH, Subpage.kKeyboard);
  r.STYLUS =
      createSubpage(r.DEVICE, routesMojom.STYLUS_SUBPAGE_PATH, Subpage.kStylus);
  r.DISPLAY = createSubpage(
      r.DEVICE, routesMojom.DISPLAY_SUBPAGE_PATH, Subpage.kDisplay);
  if (loadTimeData.getBoolean('enableAudioSettingsPage')) {
    r.AUDIO =
        createSubpage(r.DEVICE, routesMojom.AUDIO_SUBPAGE_PATH, Subpage.kAudio);
  }
  if (loadTimeData.getBoolean('enableInputDeviceSettingsSplit')) {
    r.PER_DEVICE_KEYBOARD = createSubpage(
        r.DEVICE, routesMojom.PER_DEVICE_KEYBOARD_SUBPAGE_PATH,
        Subpage.kPerDeviceKeyboard);
    r.PER_DEVICE_MOUSE = createSubpage(
        r.DEVICE, routesMojom.PER_DEVICE_MOUSE_SUBPAGE_PATH,
        Subpage.kPerDeviceMouse);
    r.PER_DEVICE_POINTING_STICK = createSubpage(
        r.DEVICE, routesMojom.PER_DEVICE_POINTING_STICK_SUBPAGE_PATH,
        Subpage.kPerDevicePointingStick);
    r.PER_DEVICE_TOUCHPAD = createSubpage(
        r.DEVICE, routesMojom.PER_DEVICE_TOUCHPAD_SUBPAGE_PATH,
        Subpage.kPerDeviceTouchpad);
    r.PER_DEVICE_KEYBOARD_REMAP_KEYS = createSubpage(
        r.PER_DEVICE_KEYBOARD,
        routesMojom.PER_DEVICE_KEYBOARD_REMAP_KEYS_SUBPAGE_PATH,
        Subpage.kPerDeviceKeyboardRemapKeys);
  }
  r.STORAGE = createSubpage(
      r.DEVICE, routesMojom.STORAGE_SUBPAGE_PATH, Subpage.kStorage);
  r.EXTERNAL_STORAGE_PREFERENCES = createSubpage(
      r.STORAGE, routesMojom.EXTERNAL_STORAGE_SUBPAGE_PATH,
      Subpage.kExternalStorage);
  r.POWER =
      createSubpage(r.DEVICE, routesMojom.POWER_SUBPAGE_PATH, Subpage.kPower);

  // Personalization section.
  if (!loadTimeData.getBoolean('isGuest')) {
    r.PERSONALIZATION = createSection(
        r.BASIC, routesMojom.PERSONALIZATION_SECTION_PATH,
        Section.kPersonalization);
  }

  // Search and Assistant section.
  r.OS_SEARCH = createSection(
      r.BASIC, routesMojom.SEARCH_AND_ASSISTANT_SECTION_PATH,
      Section.kSearchAndAssistant);
  r.GOOGLE_ASSISTANT = createSubpage(
      r.OS_SEARCH, routesMojom.ASSISTANT_SUBPAGE_PATH, Subpage.kAssistant);
  r.SEARCH_SUBPAGE = createSubpage(
      r.OS_SEARCH, routesMojom.SEARCH_SUBPAGE_PATH, Subpage.kSearch);

  // Apps section.
  r.APPS = createSection(r.BASIC, routesMojom.APPS_SECTION_PATH, Section.kApps);
  r.APP_NOTIFICATIONS = createSubpage(
      r.APPS, routesMojom.APP_NOTIFICATIONS_SUBPAGE_PATH,
      Subpage.kAppNotifications);
  r.APP_MANAGEMENT = createSubpage(
      r.APPS, routesMojom.APP_MANAGEMENT_SUBPAGE_PATH, Subpage.kAppManagement);
  r.APP_MANAGEMENT_DETAIL = createSubpage(
      r.APP_MANAGEMENT, routesMojom.APP_DETAILS_SUBPAGE_PATH,
      Subpage.kAppDetails);
  if (loadTimeData.valueExists('androidAppsVisible') &&
      loadTimeData.getBoolean('androidAppsVisible')) {
    r.ANDROID_APPS_DETAILS = createSubpage(
        r.APPS, routesMojom.GOOGLE_PLAY_STORE_SUBPAGE_PATH,
        Subpage.kGooglePlayStore);
    if (loadTimeData.valueExists('showArcvmManageUsb') &&
        loadTimeData.getBoolean('showArcvmManageUsb')) {
      r.ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES = createSubpage(
          r.ANDROID_APPS_DETAILS,
          routesMojom.ARC_VM_USB_PREFERENCES_SUBPAGE_PATH,
          Subpage.kArcVmUsbPreferences);
    }
  }
  if (loadTimeData.valueExists('showPluginVm') &&
      loadTimeData.getBoolean('showPluginVm')) {
    r.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS = createSubpage(
        r.APP_MANAGEMENT, routesMojom.PLUGIN_VM_SHARED_PATHS_SUBPAGE_PATH,
        Subpage.kPluginVmSharedPaths);
    r.APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES = createSubpage(
        r.APP_MANAGEMENT, routesMojom.PLUGIN_VM_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kPluginVmUsbPreferences);
  }

  // Accessibility section.
  r.OS_ACCESSIBILITY = createSection(
      r.BASIC, routesMojom.ACCESSIBILITY_SECTION_PATH, Section.kAccessibility);
  r.MANAGE_ACCESSIBILITY = createSubpage(
      r.OS_ACCESSIBILITY, routesMojom.MANAGE_ACCESSIBILITY_SUBPAGE_PATH,
      Subpage.kManageAccessibility);
  r.A11Y_TEXT_TO_SPEECH = createSubpage(
      r.OS_ACCESSIBILITY, routesMojom.TEXT_TO_SPEECH_PAGE_PATH,
      Subpage.kTextToSpeechPage);
  r.A11Y_DISPLAY_AND_MAGNIFICATION = createSubpage(
      r.OS_ACCESSIBILITY, routesMojom.DISPLAY_AND_MAGNIFICATION_SUBPAGE_PATH,
      Subpage.kDisplayAndMagnification);
  r.A11Y_KEYBOARD_AND_TEXT_INPUT = createSubpage(
      r.OS_ACCESSIBILITY, routesMojom.KEYBOARD_AND_TEXT_INPUT_SUBPAGE_PATH,
      Subpage.kKeyboardAndTextInput);
  r.A11Y_CURSOR_AND_TOUCHPAD = createSubpage(
      r.OS_ACCESSIBILITY, routesMojom.CURSOR_AND_TOUCHPAD_SUBPAGE_PATH,
      Subpage.kCursorAndTouchpad);
  r.A11Y_AUDIO_AND_CAPTIONS = createSubpage(
      r.OS_ACCESSIBILITY, routesMojom.AUDIO_AND_CAPTIONS_SUBPAGE_PATH,
      Subpage.kAudioAndCaptions);
  if (loadTimeData.valueExists(
          'isAccessibilityChromeVoxPageMigrationEnabled') &&
      loadTimeData.getBoolean('isAccessibilityChromeVoxPageMigrationEnabled')) {
    r.A11Y_CHROMEVOX = createSubpage(
        r.A11Y_TEXT_TO_SPEECH, routesMojom.CHROME_VOX_SUBPAGE_PATH,
        Subpage.kChromeVox);
  }
  if (loadTimeData.valueExists(
          'isAccessibilitySelectToSpeakPageMigrationEnabled') &&
      loadTimeData.getBoolean(
          'isAccessibilitySelectToSpeakPageMigrationEnabled')) {
    r.A11Y_SELECT_TO_SPEAK = createSubpage(
        r.A11Y_TEXT_TO_SPEECH, routesMojom.SELECT_TO_SPEAK_SUBPAGE_PATH,
        Subpage.kSelectToSpeak);
  }
  r.MANAGE_TTS_SETTINGS = createSubpage(
      loadTimeData.getBoolean('isKioskModeActive') ? r.MANAGE_ACCESSIBILITY :
                                                     r.A11Y_TEXT_TO_SPEECH,
      routesMojom.TEXT_TO_SPEECH_SUBPAGE_PATH, Subpage.kTextToSpeech);
  r.MANAGE_SWITCH_ACCESS_SETTINGS = createSubpage(
      r.A11Y_KEYBOARD_AND_TEXT_INPUT,
      routesMojom.SWITCH_ACCESS_OPTIONS_SUBPAGE_PATH,
      Subpage.kSwitchAccessOptions);

  // Crostini section.
  r.CROSTINI = createSection(
      r.ADVANCED, routesMojom.CROSTINI_SECTION_PATH, Section.kCrostini);
  if (loadTimeData.valueExists('showCrostini') &&
      loadTimeData.getBoolean('showCrostini')) {
    r.CROSTINI_DETAILS = createSubpage(
        r.CROSTINI, routesMojom.CROSTINI_DETAILS_SUBPAGE_PATH,
        Subpage.kCrostiniDetails);
    r.CROSTINI_SHARED_PATHS = createSubpage(
        r.CROSTINI_DETAILS,
        routesMojom.CROSTINI_MANAGE_SHARED_FOLDERS_SUBPAGE_PATH,
        Subpage.kCrostiniManageSharedFolders);
    r.CROSTINI_SHARED_USB_DEVICES = createSubpage(
        r.CROSTINI_DETAILS, routesMojom.CROSTINI_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kCrostiniUsbPreferences);
    if (loadTimeData.valueExists('showCrostiniExportImport') &&
        loadTimeData.getBoolean('showCrostiniExportImport')) {
      r.CROSTINI_EXPORT_IMPORT = createSubpage(
          r.CROSTINI_DETAILS,
          routesMojom.CROSTINI_BACKUP_AND_RESTORE_SUBPAGE_PATH,
          Subpage.kCrostiniBackupAndRestore);
    }
    if (loadTimeData.valueExists('showCrostiniExtraContainers') &&
        loadTimeData.getBoolean('showCrostiniExtraContainers')) {
      r.CROSTINI_EXTRA_CONTAINERS = createSubpage(
          r.CROSTINI_DETAILS,
          routesMojom.CROSTINI_EXTRA_CONTAINERS_SUBPAGE_PATH,
          Subpage.kCrostiniExtraContainers);
    }

    r.CROSTINI_ANDROID_ADB = createSubpage(
        r.CROSTINI_DETAILS,
        routesMojom.CROSTINI_DEVELOP_ANDROID_APPS_SUBPAGE_PATH,
        Subpage.kCrostiniDevelopAndroidApps);
    r.CROSTINI_PORT_FORWARDING = createSubpage(
        r.CROSTINI_DETAILS, routesMojom.CROSTINI_PORT_FORWARDING_SUBPAGE_PATH,
        Subpage.kCrostiniPortForwarding);

    r.BRUSCHETTA_DETAILS = createSubpage(
        r.CROSTINI, routesMojom.BRUSCHETTA_DETAILS_SUBPAGE_PATH,
        Subpage.kBruschettaDetails);
    r.BRUSCHETTA_SHARED_USB_DEVICES = createSubpage(
        r.BRUSCHETTA_DETAILS,
        routesMojom.BRUSCHETTA_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kBruschettaUsbPreferences);
    r.BRUSCHETTA_SHARED_PATHS = createSubpage(
        r.BRUSCHETTA_DETAILS,
        routesMojom.BRUSCHETTA_MANAGE_SHARED_FOLDERS_SUBPAGE_PATH,
        Subpage.kBruschettaManageSharedFolders);
  }

  // Date and Time section.
  r.DATETIME = createSection(
      r.ADVANCED, routesMojom.DATE_AND_TIME_SECTION_PATH, Section.kDateAndTime);
  r.DATETIME_TIMEZONE_SUBPAGE = createSubpage(
      r.DATETIME, routesMojom.TIME_ZONE_SUBPAGE_PATH, Subpage.kTimeZone);

  // Privacy and Security section.
  r.OS_PRIVACY = createSection(
      r.BASIC, routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH,
      Section.kPrivacyAndSecurity);
  r.LOCK_SCREEN = createSubpage(
      r.OS_PRIVACY, routesMojom.SECURITY_AND_SIGN_IN_SUBPAGE_PATH_V2,
      Subpage.kSecurityAndSignInV2);
  r.FINGERPRINT = createSubpage(
      r.LOCK_SCREEN, routesMojom.FINGERPRINT_SUBPAGE_PATH_V2,
      Subpage.kFingerprintV2);
  r.ACCOUNTS = createSubpage(
      r.OS_PRIVACY, routesMojom.MANAGE_OTHER_PEOPLE_SUBPAGE_PATH_V2,
      Subpage.kManageOtherPeopleV2);
  r.SMART_PRIVACY = createSubpage(
      r.OS_PRIVACY, routesMojom.SMART_PRIVACY_SUBPAGE_PATH,
      Subpage.kSmartPrivacy);
  r.PRIVACY_HUB = createSubpage(
      r.OS_PRIVACY, routesMojom.PRIVACY_HUB_SUBPAGE_PATH, Subpage.kPrivacyHub);

  // Languages and Input section.
  r.OS_LANGUAGES = createSection(
      r.ADVANCED, routesMojom.LANGUAGES_AND_INPUT_SECTION_PATH,
      Section.kLanguagesAndInput);
  r.OS_LANGUAGES_LANGUAGES = createSubpage(
      r.OS_LANGUAGES, routesMojom.LANGUAGES_SUBPAGE_PATH, Subpage.kLanguages);
  r.OS_LANGUAGES_INPUT = createSubpage(
      r.OS_LANGUAGES, routesMojom.INPUT_SUBPAGE_PATH, Subpage.kInput);
  r.OS_LANGUAGES_INPUT_METHOD_OPTIONS = createSubpage(
      r.OS_LANGUAGES_INPUT, routesMojom.INPUT_METHOD_OPTIONS_SUBPAGE_PATH,
      Subpage.kInputMethodOptions);
  r.OS_LANGUAGES_EDIT_DICTIONARY = createSubpage(
      r.OS_LANGUAGES_INPUT, routesMojom.EDIT_DICTIONARY_SUBPAGE_PATH,
      Subpage.kEditDictionary);
  r.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY = createSubpage(
      r.OS_LANGUAGES_INPUT,
      routesMojom.JAPANESE_MANAGE_USER_DICTIONARY_SUBPAGE_PATH,
      Subpage.kJapaneseManageUserDictionary);
  r.OS_LANGUAGES_SMART_INPUTS = createSubpage(
      r.OS_LANGUAGES, routesMojom.SMART_INPUTS_SUBPAGE_PATH,
      Subpage.kSmartInputs);


  // Files section.
  if (!loadTimeData.getBoolean('isGuest')) {
    r.FILES = createSection(
        r.ADVANCED, routesMojom.FILES_SECTION_PATH, Section.kFiles);
    r.SMB_SHARES = createSubpage(
        r.FILES, routesMojom.NETWORK_FILE_SHARES_SUBPAGE_PATH,
        Subpage.kNetworkFileShares);
    if (loadTimeData.getBoolean('enableDriveFsBulkPinning')) {
      r.GOOGLE_DRIVE = createSubpage(
          r.FILES, routesMojom.GOOGLE_DRIVE_SUBPAGE_PATH, Subpage.kGoogleDrive);
    }
    r.OFFICE = createSubpage(
        r.FILES, routesMojom.OFFICE_FILES_SUBPAGE_PATH, Subpage.kOfficeFiles);
  }

  // Printing section.
  r.OS_PRINTING = createSection(
      r.ADVANCED, routesMojom.PRINTING_SECTION_PATH, Section.kPrinting);
  r.CUPS_PRINTERS = createSubpage(
      r.OS_PRINTING, routesMojom.PRINTING_DETAILS_SUBPAGE_PATH,
      Subpage.kPrintingDetails);

  // Reset section.
  if (loadTimeData.valueExists('allowPowerwash') &&
      loadTimeData.getBoolean('allowPowerwash')) {
    r.OS_RESET = createSection(
        r.ADVANCED, routesMojom.RESET_SECTION_PATH, Section.kReset);
  }

  // About section. Note that this section is a special case, since it is not
  // part of the main page. In this case, the "About Chrome OS" subpage is
  // implemented using createSection().
  // TODO(khorimoto): Add Section.kAboutChromeOs to Route object.
  r.ABOUT = new Route('/' + routesMojom.ABOUT_CHROME_OS_SECTION_PATH);
  r.ABOUT_ABOUT = r.ABOUT.createSection(
      '/' + routesMojom.ABOUT_CHROME_OS_DETAILS_SUBPAGE_PATH, 'about');
  r.DETAILED_BUILD_INFO = createSubpage(
      r.ABOUT_ABOUT, routesMojom.DETAILED_BUILD_INFO_SUBPAGE_PATH,
      Subpage.kDetailedBuildInfo);

  return r as OsSettingsRoutes;
}

export const routes = createOsSettingsRoutes();
