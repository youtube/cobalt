// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UUID} from './uuid.mojom-webui.js';

/**
 * Format in a user readable way service UUIDs.
 * @param ?Array<UUID> uuids
 * @return {string}
 */
export function formatServiceUuids(serviceUuids) {
  if (!serviceUuids) {
    return '';
  }
  return serviceUuids.map(service => service.uuid).join(', ');
}

/**
 * Format in a user readable way device manufacturer data map. Keys are
 * Bluetooth company identifiers (unsigned short), values are bytes.
 * @param {Map<string, array<number>>} manufacturerDataMap
 * @return {string}
 */
export function formatManufacturerDataMap(manufacturerDataMap) {
  return Object.entries(manufacturerDataMap)
      .map(([key, value]) => {
        const companyIdentifier = parseInt(key).toString(16).padStart(4, '0');
        const data = value.map(v => v.toString(16).padStart(2, '0')).join('');
        return `0x${companyIdentifier} 0x${data}`;
      })
      .join(' | ');
}
