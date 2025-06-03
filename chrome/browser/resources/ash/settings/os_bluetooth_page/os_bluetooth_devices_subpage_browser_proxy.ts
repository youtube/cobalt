// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

export interface OsBluetoothDevicesSubpageBrowserProxy {
  /**
   * Requests whether the Fast Pair feature is supported by the device.
   * Returned by the 'fast-pair-device-supported' WebUI listener event.
   */
  requestFastPairDeviceSupport(): void;

  /**
   * Requests Fast Pair Saved Devices opt-in status and list of devices.
   * Returned by the 'fast-pair-saved-devices-opt-in-status' and the
   * 'fast-pair-saved-devices-list' WebUI listener event.
   */
  requestFastPairSavedDevices(): void;

  /**
   * Invokes the removal of a Fast Pair device by the account key
   * |accountKey| from a user's account.
   */
  deleteFastPairSavedDevice(accountKey: string): void;

  /**
   * Triggers Bluetooth revamp Hats survey. If user is selected Hats survey
   * would be shown after a 5 minute delay
   */
  showBluetoothRevampHatsSurvey(): void;
}

let instance: OsBluetoothDevicesSubpageBrowserProxy|null = null;

export class OsBluetoothDevicesSubpageBrowserProxyImpl implements
    OsBluetoothDevicesSubpageBrowserProxy {
  static getInstance(): OsBluetoothDevicesSubpageBrowserProxy {
    return instance ||
        (instance = new OsBluetoothDevicesSubpageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: OsBluetoothDevicesSubpageBrowserProxy):
      void {
    instance = obj;
  }

  requestFastPairDeviceSupport(): void {
    chrome.send('requestFastPairDeviceSupportStatus');
  }

  requestFastPairSavedDevices(): void {
    chrome.send('loadSavedDevicePage');
  }

  deleteFastPairSavedDevice(accountKey: string): void {
    chrome.send('removeSavedDevice', [accountKey]);
  }

  showBluetoothRevampHatsSurvey(): void {
    if (loadTimeData.getBoolean('bluetoothRevampHatsSurveyFlag')) {
      chrome.send('showBluetoothRevampHatsSurvey');
    }
  }
}
