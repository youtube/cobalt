// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

function isFlagEnabled(flagName: string) {
  return loadTimeData.isInitialized() && loadTimeData.valueExists(flagName) &&
      loadTimeData.getBoolean(flagName);
}

/**
 * Whether the Files app integration with DLP (Data Loss Prevention) is enabled.
 */
export function isDlpEnabled() {
  return isFlagEnabled('DLP_ENABLED');
}

/**
 * Returns true if FuseBoxDebug flag is enabled.
 */
export function isFuseBoxDebugEnabled() {
  return isFlagEnabled('FUSEBOX_DEBUG');
}

/**
 * Returns true if GuestOsFiles flag is enabled.
 */
export function isGuestOsEnabled() {
  return isFlagEnabled('GUEST_OS');
}

/**
 * Returns true if Jelly flag is enabled.
 */
export function isJellyEnabled() {
  return isFlagEnabled('JELLY');
}

/**
 * Returns true if the cros-components flag is enabled.
 */
export function isCrosComponentsEnabled() {
  return isFlagEnabled('CROS_COMPONENTS');
}

/**
 * Returns true if DriveFsMirroring flag is enabled.
 */
export function isMirrorSyncEnabled() {
  return isFlagEnabled('DRIVEFS_MIRRORING');
}

export function isGoogleOneOfferFilesBannerEligibleAndEnabled() {
  return isFlagEnabled('ELIGIBLE_AND_ENABLED_GOOGLE_ONE_OFFER_FILES_BANNER');
}

/**
 * Returns true if FilesSinglePartitionFormat flag is enabled.
 */
export function isSinglePartitionFormatEnabled() {
  return isFlagEnabled('FILES_SINGLE_PARTITION_FORMAT_ENABLED');
}

/**
 * Returns true if InlineSyncStatus feature flag is enabled.
 */
export function isInlineSyncStatusEnabled() {
  return isFlagEnabled('INLINE_SYNC_STATUS');
}

/**
 * Returns true if FilesDriveShortcuts flag is enabled.
 */
export function isDriveShortcutsEnabled() {
  return isFlagEnabled('DRIVE_SHORTCUTS');
}

/**
 * Returns whether the DriveFsBulkPinning feature flag is enabled.
 */
export function isDriveFsBulkPinningEnabled() {
  return isFlagEnabled('DRIVE_FS_BULK_PINNING');
}

/**
 * Whether the new directory tree flag is enabled.
 */
export function isNewDirectoryTreeEnabled() {
  return isFlagEnabled('NEW_DIRECTORY_TREE');
}

export function isArcUsbStorageUIEnabled() {
  return isFlagEnabled('ARC_USB_STORAGE_UI_ENABLED');
}

export function isArcVmEnabled() {
  return isFlagEnabled('ARC_VM_ENABLED');
}

export function isPluginVmEnabled() {
  return isFlagEnabled('PLUGIN_VM_ENABLED');
}
