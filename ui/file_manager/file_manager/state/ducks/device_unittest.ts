// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {State} from '../../externs/ts/state.js';
import {constants} from '../../foreground/js/constants.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, waitDeepEquals} from '../for_tests.js';
import {getEmptyState, getEntry, getFileData, getStore, type Store} from '../store.js';

import {updateDeviceConnectionState} from './device.js';
import {addVolume, convertVolumeInfoAndMetadataToVolume} from './volumes.js';

let store: Store;

export function setUp() {
  setUpFileManagerOnWindow();
  store = getStore();
  store.init(getEmptyState());
}

export async function testUpdateDeviceConnection(done: () => void) {
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'odfs', 'OneDrive', '',
      constants.ODFS_EXTENSION_ID, '');
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);
  store.dispatch(addVolume({
    volumeInfo,
    volumeMetadata,
  }));

  const odfsVolume =
      convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata);
  let odfsVolumeFileData = getFileData(store.getState(), odfsVolume.rootKey!)!;
  let odfsVolumeEntry =
      getEntry(store.getState(), odfsVolume.rootKey!) as VolumeEntry;

  assertEquals(
      chrome.fileManagerPrivate.DeviceConnectionState.ONLINE,
      store.getState().device.connection);
  assertFalse(store.getState().volumes[odfsVolume.volumeId].isDisabled);
  assertFalse(odfsVolumeFileData.disabled);
  assertFalse(odfsVolumeEntry.disabled);

  // Dispatch an action to set |connection| for the device to OFFLINE.
  store.dispatch(updateDeviceConnectionState({
    connection: chrome.fileManagerPrivate.DeviceConnectionState.OFFLINE,
  }));

  // Expect the volume to be set to disabled.
  const want: Partial<State> = {
    volumes: {
      [odfsVolume.volumeId]: {
        ...odfsVolume,
        isDisabled: true,
      },
    },
  };
  await waitDeepEquals(store, want, (state) => ({
                                      volumes: state.volumes,
                                    }));
  assertEquals(
      chrome.fileManagerPrivate.DeviceConnectionState.OFFLINE,
      store.getState().device.connection);

  odfsVolumeFileData = getFileData(store.getState(), odfsVolume.rootKey!)!;
  odfsVolumeEntry =
      getEntry(store.getState(), odfsVolume.rootKey!) as VolumeEntry;
  assertTrue(odfsVolumeFileData.disabled);
  assertTrue(odfsVolumeEntry.disabled);

  done();
}
