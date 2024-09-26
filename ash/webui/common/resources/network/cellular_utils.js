// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/onc_mojo.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnProperties, DeviceStateProperties, FilterType, NetworkStateProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

/**
 * TODO(b/162365553): Implement Edit mode.
 * @enum {string}
 */
export const ApnDetailDialogMode = {
  CREATE: 'create',
  EDIT: 'edit',
  VIEW: 'view',
};

/**
 * @typedef {{
 *   apn: !ApnProperties,
 *   mode: !ApnDetailDialogMode,
 * }}
 */
export let ApnEventData;

/**
 * Checks if the device has a cellular network with connectionState not
 * kNotConnected.
 * @return {!Promise<boolean>}
 */
export function hasActiveCellularNetwork() {
  const networkConfig =
      MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  return networkConfig
      .getNetworkStateList({
        filter: FilterType.kActive,
        networkType: NetworkType.kCellular,
        limit: NO_LIMIT,
      })
      .then((response) => {
        return response.result.some(network => {
          return network.connectionState !== ConnectionStateType.kNotConnected;
        });
      });
}

/**
 * Returns number of phyical SIM and eSIM slots on the current device
 * @param {!DeviceStateProperties|undefined}
 *     deviceState
 * @return {!{pSimSlots: number, eSimSlots: number}}
 */
export function getSimSlotCount(deviceState) {
  let pSimSlots = 0;
  let eSimSlots = 0;

  if (!deviceState || !deviceState.simInfos) {
    return {pSimSlots, eSimSlots};
  }

  for (const simInfo of deviceState.simInfos) {
    if (simInfo.eid) {
      eSimSlots++;
      continue;
    }
    pSimSlots++;
  }

  return {pSimSlots, eSimSlots};
}

/**
 * Checks if the device is currently connected to a WiFi, Ethernet or Tether
 * network.
 * @return {!Promise<boolean>}
 */
export function isConnectedToNonCellularNetwork() {
  const networkConfig =
      MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  return networkConfig
      .getNetworkStateList({
        filter: FilterType.kActive,
        networkType: NetworkType.kAll,
        limit: NO_LIMIT,
      })
      .then((response) => {
        // Filter for connected non-cellular networks.
        return response.result.some(network => {
          return network.connectionState === ConnectionStateType.kOnline &&
              network.type !== NetworkType.kCellular;
        });
      });
}

/**
 * Determines if the current network is on the active sim slot.
 * @param {?NetworkStateProperties} networkState
 * @param {?DeviceStateProperties} deviceState
 */
export function isActiveSim(networkState, deviceState) {
  if (!networkState || networkState.type !== NetworkType.kCellular) {
    return false;
  }

  const iccid = networkState.typeState.cellular.iccid;
  if (!iccid || !deviceState || !deviceState.simInfos) {
    return false;
  }
  const isActiveSim = deviceState.simInfos.find(simInfo => {
    return simInfo.iccid === iccid && simInfo.isPrimary;
  });
  return !!isActiveSim;
}

/**
 * Returns true if all significant DeviceState fields match. Ignores
 * |scanning| which can be noisy and is handled separately.
 * @param {!OncMojo.DeviceStateProperties} a
 * @param {!OncMojo.DeviceStateProperties} b
 * @return {boolean}
 */
export function deviceStatesMatch(a, b) {
  return a.type === b.type && a.macAddress === b.macAddress &&
      a.simAbsent === b.simAbsent && a.deviceState === b.deviceState &&
      a.managedNetworkAvailable === b.managedNetworkAvailable &&
      OncMojo.ipAddressMatch(a.ipv4Address, b.ipv4Address) &&
      OncMojo.ipAddressMatch(a.ipv6Address, b.ipv6Address) &&
      OncMojo.simLockStatusMatch(a.simLockStatus, b.simLockStatus) &&
      OncMojo.simInfosMatch(a.simInfos, b.simInfos) &&
      a.inhibitReason === b.inhibitReason;
}

/**
 * @param {!NetworkType} type
 * @param {!Array<!DeviceStateProperties>} devices
 * @param {?OncMojo.DeviceStateProperties} deviceState
 * @return {{deviceState: OncMojo.DeviceStateProperties,
 *           shouldGetNetworkDetails: boolean}}
 */
export function processDeviceState(type, devices, deviceState) {
  const newDeviceState = devices.find(device => device.type === type) || null;
  let shouldGetNetworkDetails = false;
  if (!deviceState || !newDeviceState) {
    deviceState = /**@type {?OncMojo.DeviceStateProperties}*/ (newDeviceState);
    shouldGetNetworkDetails = !!deviceState;
  } else if (!deviceStatesMatch(deviceState, newDeviceState)) {
    // Only request a network state update if the deviceState changed.
    shouldGetNetworkDetails =
        deviceState.deviceState !== newDeviceState.deviceState;
    deviceState = /**@type {?OncMojo.DeviceStateProperties}*/ (newDeviceState);
  } else if (deviceState.scanning !== newDeviceState.scanning) {
    // Update just the scanning state to avoid interrupting other parts of
    // the UI (e.g. custom IP addresses or nameservers).
    deviceState.scanning = newDeviceState.scanning;
    // Cellular properties are not updated while scanning (since they
    // may be invalid), so request them on scan completion.
    if (type === NetworkType.kCellular) {
      shouldGetNetworkDetails = true;
    }
  } else if (type === NetworkType.kCellular) {
    // If there are no device state property changes but type is
    // cellular, then always fetch network details. This is because
    // for cellular networks, some shill device level properties are
    // represented at network level in ONC.
    shouldGetNetworkDetails = true;
  }
  return {deviceState, shouldGetNetworkDetails};
}