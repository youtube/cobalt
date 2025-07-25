// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {isComputersRoot, isFakeEntry, isSameEntry, isTeamDriveRoot} from '../../common/js/entry_utils.js';
import {MockEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {str} from '../../common/js/translations.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {FakeEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {EntryLocationImpl} from './entry_location_impl.js';
import {VolumeInfoImpl} from './volume_info_impl.js';
import {VolumeInfoListImpl} from './volume_info_list_impl.js';
import {volumeManagerFactory} from './volume_manager_factory.js';
import {VolumeManagerImpl} from './volume_manager_impl.js';

export const fakeMyFilesVolumeId =
    VolumeManagerCommon.VolumeType.DOWNLOADS + ':test_mount_path';
export const fakeDriveVolumeId =
    VolumeManagerCommon.VolumeType.DRIVE + ':test_mount_path';

/**
 * Mock class for VolumeManager.
 * @final
 * @implements {VolumeManager}
 */
export class MockVolumeManager {
  constructor() {
    /**
     * @const @type {!import("../../externs/volume_info_list.js").VolumeInfoList}
     */
    this.volumeInfoList = new VolumeInfoListImpl();

    /** @type {!chrome.fileManagerPrivate.DriveConnectionState} */
    this.driveConnectionState = {
      type: /** @type {!chrome.fileManagerPrivate.DriveConnectionStateType} */ (
          'ONLINE'),
    };

    // Create Drive.   Drive attempts to resolve FilesSystemURLs for '/root',
    // '/team_drives' and '/Computers' during initialization. Create a
    // filesystem with those entries now, and mock
    // webkitResolveLocalFileSystemURL.
    const driveFs = new MockFileSystem(
        fakeDriveVolumeId, 'filesystem:' + fakeDriveVolumeId);
    driveFs.populate(['/root/', '/team_drives/', '/Computers/']);

    // Mock window.webkitResolve to return entries.
    const orig = window.webkitResolveLocalFileSystemURL;
    window.webkitResolveLocalFileSystemURL = (url, success) => {
      const rootURL = `filesystem:${fakeDriveVolumeId}`;
      const match = url.match(new RegExp(`^${rootURL}(\/.*)`));
      if (match) {
        const path = /** @type {string} */ (match[1]);
        const entry = driveFs.entries[path];
        if (entry) {
          return setTimeout(success, 0, entry);
        }
      }
      throw new DOMException('Unknown drive url: ' + url, 'NotFoundError');
    };

    // Create Drive, swap entries back in, revert window.webkitResolve.
    const drive = this.createVolumeInfo(
        VolumeManagerCommon.VolumeType.DRIVE, fakeDriveVolumeId,
        str('DRIVE_DIRECTORY_LABEL'));
    /** @type {MockFileSystem} */ (drive.fileSystem)
        .populate(Object.values(driveFs.entries));
    window.webkitResolveLocalFileSystemURL = orig;

    // Create Downloads.
    this.createVolumeInfo(
        VolumeManagerCommon.VolumeType.DOWNLOADS, fakeMyFilesVolumeId,
        str('DOWNLOADS_DIRECTORY_LABEL'));
  }

  getFuseBoxOnlyFilterEnabled() {
    return false;
  }

  getMediaStoreFilesOnlyFilterEnabled() {
    return false;
  }

  dispose() {}

  /**
   * Replaces the VolumeManager singleton with a MockVolumeManager.
   * @param {!MockVolumeManager=} opt_singleton
   */
  static installMockSingleton(opt_singleton) {
    MockVolumeManager.instance_ = opt_singleton || new MockVolumeManager();

    // @ts-ignore: error TS2322: Type '() => Promise<VolumeManager | null>' is
    // not assignable to type '() => Promise<VolumeManager>'.
    volumeManagerFactory.getInstance = () => {
      return Promise.resolve(assert(MockVolumeManager.instance_));
    };
  }

  /**
   * Creates, installs and returns a mock VolumeInfo instance.
   *
   * @param {!VolumeManagerCommon.VolumeType} type
   * @param {string} volumeId
   * @param {string} label
   * @param {string=} providerId
   * @param {string=} remoteMountPath
   *
   * @return {!import("../../externs/volume_info.js").VolumeInfo}
   */
  createVolumeInfo(type, volumeId, label, providerId, remoteMountPath) {
    const volumeInfo = MockVolumeManager.createMockVolumeInfo(
        type, volumeId, label, undefined, providerId, remoteMountPath);
    this.volumeInfoList.add(volumeInfo);
    return volumeInfo;
  }

  /**
   * Obtains location information from an entry.
   * Current implementation can handle only fake entries.
   *
   * @param {!Entry|!FilesAppEntry} entry A fake entry.
   * @return {!EntryLocation|null} Location information.
   */
  getLocationInfo(entry) {
    if (isFakeEntry(entry)) {
      const isReadOnly =
          // @ts-ignore: error TS2339: Property 'rootType' does not exist on
          // type 'FileSystemEntry | FilesAppEntry'.
          entry.rootType !== VolumeManagerCommon.RootType.RECENT &&
          // @ts-ignore: error TS2339: Property 'rootType' does not exist on
          // type 'FileSystemEntry | FilesAppEntry'.
          entry.rootType !== VolumeManagerCommon.RootType.TRASH;
      return new EntryLocationImpl(
          this.volumeInfoList.item(0),
          /** @type {!FakeEntry} */ (entry).rootType, /* isRootType= */ true,
          isReadOnly);
    }

    // @ts-ignore: error TS18047: 'entry.filesystem' is possibly 'null'.
    if (entry.filesystem.name === fakeDriveVolumeId) {
      const volumeInfo = this.volumeInfoList.item(0);
      let rootType = VolumeManagerCommon.RootType.DRIVE;
      let isRootEntry = entry.fullPath === '/root';
      if (entry.fullPath.startsWith('/team_drives')) {
        if (entry.fullPath === '/team_drives') {
          rootType = VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT;
          isRootEntry = true;
        } else {
          rootType = VolumeManagerCommon.RootType.SHARED_DRIVE;
          isRootEntry = isTeamDriveRoot(entry);
        }
      } else if (entry.fullPath.startsWith('/Computers')) {
        if (entry.fullPath === '/Computers') {
          rootType = VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT;
          isRootEntry = true;
        } else {
          rootType = VolumeManagerCommon.RootType.COMPUTER;
          isRootEntry = isComputersRoot(entry);
        }
      } else if (/^\/\.(files|shortcut-targets)-by-id/.test(entry.fullPath)) {
        rootType = VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME;
      }
      return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, true);
    }

    const volumeInfo = this.getVolumeInfo(entry);
    // For filtered out volumes, its volume info won't exist in the volume info
    // list.
    if (!volumeInfo) {
      return null;
    }
    const rootType = VolumeManagerCommon.getRootTypeFromVolumeType(
        assert(volumeInfo.volumeType));
    const isRootEntry = isSameEntry(entry, volumeInfo.fileSystem.root);
    return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, false);
  }

  /**
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
   * @return {?import("../../externs/volume_info.js").VolumeInfo} Volume info.
   */
  getCurrentProfileVolumeInfo(volumeType) {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.profile.isCurrentProfile &&
          volumeInfo.volumeType === volumeType) {
        return volumeInfo;
      }
    }
    return null;
  }

  /**
   * @return {!chrome.fileManagerPrivate.DriveConnectionState} Current drive
   *     connection state.
   */
  getDriveConnectionState() {
    return this.driveConnectionState;
  }

  /**
   * Utility function to create a mock VolumeInfo.
   * @param {!VolumeManagerCommon.VolumeType} type Volume type.
   * @param {string} volumeId Volume id.
   * @param {string=} label Label.
   * @param {string=} devicePath Device path.
   * @param {string=} providerId Provider id.
   * @param {string=} remoteMountPath Remote mount path.
   * @return {!import("../../externs/volume_info.js").VolumeInfo} Created mock
   *     VolumeInfo.
   */
  static createMockVolumeInfo(
      type, volumeId, label, devicePath, providerId, remoteMountPath) {
    const fileSystem = new MockFileSystem(volumeId, 'filesystem:' + volumeId);

    let diskFileSystemType = VolumeManagerCommon.FileSystemType.UNKNOWN;
    if (devicePath && devicePath.startsWith('fusebox')) {
      diskFileSystemType =
          /** @type VolumeManagerCommon.FileSystemType */ ('fusebox');
    }

    let source = VolumeManagerCommon.Source.NETWORK;
    if (type === VolumeManagerCommon.VolumeType.ARCHIVE) {
      source = VolumeManagerCommon.Source.FILE;
    } else if (type === VolumeManagerCommon.VolumeType.REMOVABLE) {
      source = VolumeManagerCommon.Source.DEVICE;
    }

    // If there's no label set it to volumeId to make it shorter to write
    // tests.
    const volumeInfo = new VolumeInfoImpl(
        type,
        volumeId,
        fileSystem,
        '',                                         // error
        '',                                         // deviceType
        devicePath || '',                           // devicePath
        false,                                      // isReadOnly
        false,                                      // isReadOnlyRemovableDevice
        {isCurrentProfile: true, displayName: ''},  // profile
        label || volumeId,                          // label
        providerId,                                 // providerId
        false,                                      // hasMedia
        false,                                      // configurable
        false,                                      // watchable
        source,                                     // source
        diskFileSystemType,                         // diskFileSystemType
        // @ts-ignore: error TS2345: Argument of type '{}' is not assignable to
        // parameter of type 'IconSet'.
        {},               // iconSet
        '',               // driveLabel
        remoteMountPath,  // remoteMountPath
        undefined,        // vmType
    );


    return volumeInfo;
  }

  /**
   * @return {!Promise<!import("../../externs/volume_info.js").VolumeInfo>}
   */
  // @ts-ignore: error TS7006: Parameter 'password' implicitly has an 'any'
  // type.
  async mountArchive(fileUrl, password) {
    throw new Error('Not implemented');
  }

  // @ts-ignore: error TS7006: Parameter 'fileUrl' implicitly has an 'any' type.
  async cancelMounting(fileUrl) {
    throw new Error('Not implemented');
  }

  // @ts-ignore: error TS7006: Parameter 'volumeInfo' implicitly has an 'any'
  // type.
  async unmount(volumeInfo) {
    throw new Error('Not implemented');
  }

  // @ts-ignore: error TS7006: Parameter 'volumeInfo' implicitly has an 'any'
  // type.
  async configure(volumeInfo) {
    throw new Error('Not implemented');
  }

  // @ts-ignore: error TS7006: Parameter 'handler' implicitly has an 'any' type.
  addEventListener(type, handler) {
    throw new Error('Not implemented');
  }

  // @ts-ignore: error TS7006: Parameter 'handler' implicitly has an 'any' type.
  removeEventListener(type, handler) {
    throw new Error('Not implemented');
  }

  /**
   * @return {boolean}
   */
  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  dispatchEvent(event) {
    throw new Error('Not implemented');
  }

  /**
   * @return {boolean}
   */
  hasDisabledVolumes() {
    return false;
  }

  /**
   * @return {boolean}
   */
  // @ts-ignore: error TS7006: Parameter 'volume' implicitly has an 'any' type.
  isDisabled(volume) {
    return false;
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'volumeInfo' implicitly has an 'any'
  // type.
  isAllowedVolume(volumeInfo) {
    return true;
  }
}

/** @private @type {?VolumeManager} */
// @ts-ignore: error TS2341: Property 'instance_' is private and only accessible
// within class 'MockVolumeManager'.
MockVolumeManager.instance_ = null;

MockVolumeManager.prototype.getVolumeInfo =
    VolumeManagerImpl.prototype.getVolumeInfo;

MockVolumeManager.prototype.getDefaultDisplayRoot =
    VolumeManagerImpl.prototype.getDefaultDisplayRoot;

MockVolumeManager.prototype.findByDevicePath =
    VolumeManagerImpl.prototype.findByDevicePath;

MockVolumeManager.prototype.whenVolumeInfoReady =
    VolumeManagerImpl.prototype.whenVolumeInfoReady;

/**
 * Used to override window.webkitResolveLocalFileSystemURL for testing. This
 * emulates the real function by parsing `url` and finding the matching entry
 * in `volumeManager`. E.g. filesystem:downloads:test_mount_path/dir/file.txt
 * will look up the volume with ID 'downloads:test_mount_path' for
 * /dir/file.txt.
 *
 * @param {VolumeManager} volumeManager VolumeManager to resolve URLs with.
 * @param {string} url URL to resolve.
 * @param {function(!MockEntry):void} successCallback Success callback.
 * @param {function(!FileError):void=} errorCallback Error callback.
 */
MockVolumeManager.resolveLocalFileSystemURL =
    (volumeManager, url, successCallback, errorCallback) => {
      const match = url.match(/^filesystem:(\w+):\w+(\/.*)/);
      if (match) {
        const volumeType =
            /** @type {VolumeManagerCommon.VolumeType} */ (match[1]);
        let path = match[2];
        const volume = volumeManager.getCurrentProfileVolumeInfo(volumeType);
        if (volume) {
          // Decode URI in file paths.
          // @ts-ignore: error TS18048: 'path' is possibly 'undefined'.
          path = path.split('/').map(decodeURIComponent).join('/');
          // @ts-ignore: error TS2339: Property 'entries' does not exist on type
          // 'FileSystem'.
          const entry = volume.fileSystem.entries[path];
          if (entry) {
            setTimeout(successCallback, 0, entry);
            return;
          }
        }
      }
      const message =
          `MockVolumeManager.resolveLocalFileSystemURL not found: ${url}`;
      console.warn(message);
      const error = new DOMException(message, 'NotFoundError');
      if (errorCallback) {
        setTimeout(errorCallback, 0, error);
      } else {
        throw error;
      }
    };
