// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

import {ContentSettingsTypes} from '../site_settings/constants.js';

/**
 * Determine localization string for i18n for a given content settings type.
 * Sorted alphabetically by |ContentSettingsType|.
 */
export function getLocalizationStringForContentType(
    contentSettingsType: ContentSettingsTypes): string|null {
  switch (contentSettingsType) {
    case ContentSettingsTypes.ADS:
      return 'siteSettingsAdsMidSentence';
    case ContentSettingsTypes.AR:
      return 'siteSettingsArMidSentence';
    case ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
      return 'siteSettingsAutomaticDownloadsMidSentence';
    case ContentSettingsTypes.BACKGROUND_SYNC:
      return 'siteSettingsBackgroundSyncMidSentence';
    case ContentSettingsTypes.BLUETOOTH_DEVICES:
      return 'siteSettingsBluetoothDevicesMidSentence';
    case ContentSettingsTypes.BLUETOOTH_SCANNING:
      return 'siteSettingsBluetoothScanningMidSentence';
    case ContentSettingsTypes.CAMERA:
      return 'siteSettingsCameraMidSentence';
    case ContentSettingsTypes.CLIPBOARD:
      return 'siteSettingsClipboardMidSentence';
    case ContentSettingsTypes.COOKIES:
      return 'siteSettingsCookiesMidSentence';
    case ContentSettingsTypes.FEDERATED_IDENTITY_API:
      return 'siteSettingsFederatedIdentityApiMidSentence';
    case ContentSettingsTypes.FILE_SYSTEM_WRITE:
      return 'siteSettingsFileSystemWriteMidSentence';
    case ContentSettingsTypes.GEOLOCATION:
      return 'siteSettingsLocationMidSentence';
    case ContentSettingsTypes.HID_DEVICES:
      return 'siteSettingsHidDevicesMidSentence';
    case ContentSettingsTypes.IDLE_DETECTION:
      return 'siteSettingsIdleDetectionMidSentence';
    case ContentSettingsTypes.IMAGES:
      return 'siteSettingsImagesMidSentence';
    case ContentSettingsTypes.JAVASCRIPT:
      return 'siteSettingsJavascriptMidSentence';
    case ContentSettingsTypes.LOCAL_FONTS:
      return 'siteSettingsFontAccessMidSentence';
    case ContentSettingsTypes.MIC:
      return 'siteSettingsMicMidSentence';
    case ContentSettingsTypes.MIDI_DEVICES:
      return 'siteSettingsMidiDevicesMidSentence';
    case ContentSettingsTypes.MIXEDSCRIPT:
      return 'siteSettingsInsecureContentMidSentence';
    case ContentSettingsTypes.NOTIFICATIONS:
      return 'siteSettingsNotificationsMidSentence';
    case ContentSettingsTypes.PAYMENT_HANDLER:
      return 'siteSettingsPaymentHandlerMidSentence';
    case ContentSettingsTypes.POPUPS:
      return 'siteSettingsPopupsMidSentence';
    case ContentSettingsTypes.PROTECTED_CONTENT:
      return 'siteSettingsProtectedContentMidSentence';
    case ContentSettingsTypes.PROTOCOL_HANDLERS:
      return 'siteSettingsHandlersMidSentence';
    case ContentSettingsTypes.SENSORS:
      return 'siteSettingsSensorsMidSentence';
    case ContentSettingsTypes.SERIAL_PORTS:
      return 'siteSettingsSerialPortsMidSentence';
    case ContentSettingsTypes.SOUND:
      return 'siteSettingsSoundMidSentence';
    case ContentSettingsTypes.USB_DEVICES:
      return 'siteSettingsUsbDevicesMidSentence';
    case ContentSettingsTypes.VR:
      return 'siteSettingsVrMidSentence';
    case ContentSettingsTypes.WINDOW_MANAGEMENT:
      return 'siteSettingsWindowManagementMidSentence';
    case ContentSettingsTypes.ZOOM_LEVELS:
      return 'siteSettingsZoomLevelsMidSentence';
    // The following members do not have a mid-sentence localization.
    case ContentSettingsTypes.ANTI_ABUSE:
    case ContentSettingsTypes.PDF_DOCUMENTS:
    case ContentSettingsTypes.PRIVATE_NETWORK_DEVICES:
    case ContentSettingsTypes.SITE_DATA:
      return null;
    default:
      assertNotReached();
  }
}
