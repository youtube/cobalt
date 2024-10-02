// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Banner} from '../../externs/banner.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {isAllowedVolume, isBelowThreshold} from './banner_controller.js';

/** @type {!Array<!Banner.AllowedVolume>} */
let allowedVolumes = [];

/**
 * Returns a VolumeInfo with the type and id set.
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {?string=} volumeId
 * @returns {!VolumeInfo}
 */
function createAndSetVolumeInfo(volumeType, volumeId = null) {
  class FakeVolumeInfo {
    constructor() {
      this.volumeType = volumeType;
      this.volumeId = volumeId;
    }
  }

  return /** @type {!VolumeInfo} */ (new FakeVolumeInfo());
}

/**
 * Creates a chrome.fileManagerPrivate.MountPointSizeStats type.
 * @param {number} remainingSize
 * @returns {!chrome.fileManagerPrivate.MountPointSizeStats}
 */
function createRemainingSizeStats(remainingSize) {
  return {
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize,
  };
}

export function tearDown() {
  allowedVolumes = [];
}

/**
 * Test when there are no allowed volume types.
 */
export function testNoAllowedVolumes() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS);

  assertFalse(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test when there is a single allowed volume type matching the current volume.
 */
export function testAllowedVolume() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS);
  allowedVolumes = [{type: VolumeManagerCommon.VolumeType.DOWNLOADS}];

  assertTrue(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test when there are multiple volumes with one being the current volume.
 */
export function testMultipleAllowedVolumes() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS);
  allowedVolumes = [
    {type: VolumeManagerCommon.VolumeType.DOWNLOADS},
    {type: VolumeManagerCommon.VolumeType.ANDROID_FILES},
  ];

  assertTrue(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test when there are multiple volumes but none match the current volume.
 */
export function testMultipleNoAllowedVolumes() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS);
  allowedVolumes = [
    {type: VolumeManagerCommon.VolumeType.ARCHIVE},
    {type: VolumeManagerCommon.VolumeType.ANDROID_FILES},
  ];

  assertFalse(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test multiple identical volume types, with only one allowed volume id.
 */
export function testMultipleAllowedDocumentProviders() {
  const currentVolume = createAndSetVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  allowedVolumes = [
    {
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      id: 'provider_a',
    },
    {type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, id: 'provider_b'},
  ];

  assertTrue(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test multiple identical volume types, with none matching the current volume.
 */
export function testMultipleNoAllowedDocumentProviders() {
  const currentVolume = createAndSetVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  allowedVolumes = [
    {
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      id: 'provider_b',
    },
    {type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, id: 'provider_c'},
  ];

  assertFalse(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test that a single root type (with no volume info) returns true if the
 * allowed volumes only contains root information.
 */
export function testSingleRootTypeNoVolumeTypeOrId() {
  const currentRootType = VolumeManagerCommon.RootType.DOWNLOADS;
  allowedVolumes = [{
    root: VolumeManagerCommon.RootType.DOWNLOADS,
  }];
  assertTrue(isAllowedVolume(
      /* currentVolume */ null, currentRootType, allowedVolumes));
}

/**
 * Test that multiple root types defined in allowed volumes (with one matching
 * the current type) returns true.
 */
export function testMultipleRootTypesNoVolumeTypeOrId() {
  const currentRootType = VolumeManagerCommon.RootType.DOWNLOADS;
  allowedVolumes = [
    {
      root: VolumeManagerCommon.RootType.DOWNLOADS,
    },
    {
      root: VolumeManagerCommon.RootType.DRIVE,
    },
  ];
  assertTrue(isAllowedVolume(
      /* currentVolume */ null, currentRootType, allowedVolumes));
}

/**
 * Test multiple defined root types with none matching the allowed volumes
 * returns false.
 */
export function testMultipleDisallowedRootTypesNoVolumeTypeOrId() {
  const currentRootType = VolumeManagerCommon.RootType.SMB;
  allowedVolumes = [
    {
      root: VolumeManagerCommon.RootType.DOWNLOADS,
    },
    {
      root: VolumeManagerCommon.RootType.DRIVE,
    },
  ];
  assertFalse(isAllowedVolume(
      /* currentVolume */ null, currentRootType, allowedVolumes));
}

/**
 * Test both volume info and root type returns true (for stricter checking of
 * a volume type).)
 */
export function testVolumeTypeAndRootTypeNoId() {
  const currentRootType = VolumeManagerCommon.RootType.SHARED_DRIVE;
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DRIVE);
  allowedVolumes = [{
    type: VolumeManagerCommon.VolumeType.DRIVE,
    root: VolumeManagerCommon.RootType.SHARED_DRIVE,
  }];
  assertTrue(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test that if volume type matches with but not root type should return false.
 */
export function testVolumeTypeMatchesButNotRootType() {
  const currentRootType = VolumeManagerCommon.RootType.SHARED_DRIVE;
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DRIVE);
  allowedVolumes = [{
    type: VolumeManagerCommon.VolumeType.DRIVE,
    root: VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
  }];
  assertFalse(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test that volume type is defined but the root type is null, allowed volumes
 * should allow for stricter checking of a volume type, this should return
 * false.
 */
export function testVolumeTypeDefinedButNotRootType() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DRIVE);
  allowedVolumes = [{
    type: VolumeManagerCommon.VolumeType.DRIVE,
    root: VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
  }];
  assertFalse(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test that root type is defined but not volume type. If the allowed volumes
 * requires both defined, should return false.
 */
export function testRootTypeDefinedButNotVolumeType() {
  const currentRootType = VolumeManagerCommon.RootType.TRASH;
  allowedVolumes = [{
    type: VolumeManagerCommon.VolumeType.TRASH,
    root: VolumeManagerCommon.RootType.TRASH,
  }];
  assertFalse(isAllowedVolume(
      /* currentVolume */ null, currentRootType, allowedVolumes));
}

/**
 * Test that volume type, root type and id are all defined.
 */
export function testVolumeTypeRootTypeAndIdDefined() {
  const currentVolume = createAndSetVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  const currentRootType = VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER;

  allowedVolumes = [{
    type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
    root: VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER,
    id: 'provider_a',
  }];
  assertTrue(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test that volume type, root type are defined but id does not match.
 */
export function testVolumeTypeRootTypeAndIdDefinedNoMatch() {
  const currentVolume = createAndSetVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  const currentRootType = VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER;

  allowedVolumes = [{
    type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
    root: VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER,
    id: 'provider_b',
  }];
  assertFalse(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test multiple volume types, root types and ids defined with one matching.
 */
export function testMultipleDifferentIdsSameVolumeTypeAndRootTypeOneMatches() {
  const currentVolume = createAndSetVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  const currentRootType = VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER;

  allowedVolumes = [
    {
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      root: VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER,
      id: 'provider_b',
    },
    {
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      root: VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER,
      id: 'provider_a',
    },
  ];
  assertTrue(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test multiple volume types, root types and ids defined with none matching.
 */
export function testMultipleDifferentIdsSameVolumeTypeAndRootTypeNoneMatches() {
  const currentVolume = createAndSetVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'provider_c');
  const currentRootType = VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER;

  allowedVolumes = [
    {
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      root: VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER,
      id: 'provider_b',
    },
    {
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      root: VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER,
      id: 'provider_a',
    },
  ];
  assertFalse(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test undefined types return false.
 */
export function testUndefinedThresholdAndSizeStats() {
  const testMinSizeThreshold = {
    type: VolumeManagerCommon.VolumeType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  };
  const testMinRatioThreshold = {
    type: VolumeManagerCommon.VolumeType.DOWNLOADS,
    minSize: 0.1,
  };
  const testSizeStats = {
    totalSize: 20 * 1024 * 1024 * 1024,     // 20 GB
    remainingSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  };
  const testSizeStatsFull = {
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize: 0,                    // full
  };

  assertFalse(isBelowThreshold(undefined, undefined));
  assertFalse(isBelowThreshold(testMinSizeThreshold, undefined));
  assertFalse(isBelowThreshold(testMinRatioThreshold, undefined));
  assertFalse(isBelowThreshold(undefined, testSizeStats));
  assertTrue(isBelowThreshold(testMinRatioThreshold, testSizeStatsFull));
}

/**
 * Test that below, equal to and above the minSize threshold returns correct
 * results.
 */
export function testMinSizeReturnsCorrectly() {
  const createMinSizeThreshold = (minSize) => ({
    type: VolumeManagerCommon.VolumeType.DOWNLOADS,
    minSize,
  });

  // Remaining Size: 512 KB, Min Size: 1 GB
  assertTrue(isBelowThreshold(
      createMinSizeThreshold(1 * 1024 * 1024 * 1024 /* 1 GB */),
      createRemainingSizeStats(512 * 1024 * 1024 /* 512 KB */)));

  // Remaining Size: 1 GB, Min Size: 1 GB
  assertTrue(isBelowThreshold(
      createMinSizeThreshold(1 * 1024 * 1024 * 1024 /* 1 GB */),
      createRemainingSizeStats(1 * 1024 * 1024 * 1024 /* 1 GB */)));

  // Remaining Size: 2 GB, Min Size: 1 GB
  assertFalse(isBelowThreshold(
      createMinSizeThreshold(1 * 1024 * 1024 * 1024 /* 1 GB */),
      createRemainingSizeStats(2 * 1024 * 1024 * 1024 /* 2 GB */)));
}

/**
 * Test that below, equal to and above the minRatio threshold returns correct
 * results.
 */
export function testMinRatioReturnsCorrectly() {
  const createMinRatioThreshold = (minRatio) => ({
    type: VolumeManagerCommon.VolumeType.DOWNLOADS,
    minRatio,
  });

  // Remaining Size Ratio: 0.1, Threshold: 0.2
  assertTrue(isBelowThreshold(
      createMinRatioThreshold(0.2),
      createRemainingSizeStats(2 * 1024 * 1024 * 1024 /* 2 GB */)));

  // Remaining Size Ratio: 0.1, Threshold: 0.1
  assertTrue(isBelowThreshold(
      createMinRatioThreshold(0.1),
      createRemainingSizeStats(2 * 1024 * 1024 * 1024 /* 2 GB */)));

  // Remaining Size Ratio: 0.1, Threshold: 0.05
  assertFalse(isBelowThreshold(
      createMinRatioThreshold(0.05),
      createRemainingSizeStats(2 * 1024 * 1024 * 1024 /* 2 GB */)));
}
