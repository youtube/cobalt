// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {assert} from 'chrome://resources/js/assert.js';

import {promisify} from '../../common/js/api.js';
import {getRootType, isComputersRoot, isFakeEntry, isSameEntry, isSameFileSystem, isTeamDriveRoot} from '../../common/js/entry_utils.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import type {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {removeVolume} from '../../state/ducks/volumes.js';
import {getStore} from '../../state/store.js';

import {EntryLocationImpl} from './entry_location_impl.js';
import {VolumeInfoListImpl} from './volume_info_list_impl.js';
import {volumeManagerUtil} from './volume_manager_util.js';

type VolumeAlreadyMountedEvent = Event&{
  volumeId: string,
};

type RequestSuccessCallback = (volumeInfo?: VolumeInfo) => void;
type RequestErrorCallback = (error: VolumeManagerCommon.VolumeError) => void;
interface Request {
  successCallbacks: RequestSuccessCallback[];
  errorCallbacks: RequestErrorCallback[];
  timeout: number;
}

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 */
export class VolumeManagerImpl extends EventTarget implements VolumeManager {
  volumeInfoList = new VolumeInfoListImpl();

  /**
   * The list of archives requested to mount. We will show contents once
   * archive is mounted, but only for mounts from within this filebrowser tab.
   * TODO: Add interface to replace `any` below.
   */
  private requests_: Record<string, Request> = {};

  // The status should be merged into VolumeManager.
  // TODO(hidehiko): Remove them after the migration.
  /**
   * Connection state of the Drive.
   */
  private driveConnectionState_:
      chrome.fileManagerPrivate.DriveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
    reason: chrome.fileManagerPrivate.DriveOfflineReason.NO_SERVICE,
  };

  /**
   * Holds the resolver for the `waitForInitialization_` promise.
   */
  private finishInitialization_: (() => void)|null = null;

  /**
   * Promise used to wait for the initialize() method to finish.
   */
  private waitForInitialization_: Promise<void> =
      new Promise(resolve => this.finishInitialization_ = resolve);

  constructor() {
    super();

    chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(
        this.onDriveConnectionStatusChanged_.bind(this));
    this.onDriveConnectionStatusChanged_();

    // Subscribe to mount event as early as possible, but after the
    // waitForInitialization_ above.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
  }

  getFuseBoxOnlyFilterEnabled(): boolean {
    return false;
  }

  getMediaStoreFilesOnlyFilterEnabled(): boolean {
    return false;
  }

  dispose(): void {}

  /**
   * Invoked when the drive connection status is changed.
   */
  private onDriveConnectionStatusChanged_() {
    chrome.fileManagerPrivate.getDriveConnectionState(state => {
      this.driveConnectionState_ = state;
      dispatchSimpleEvent(this, 'drive-connection-changed');
    });
  }

  getDriveConnectionState(): chrome.fileManagerPrivate.DriveConnectionState {
    return this.driveConnectionState_;
  }

  /**
   * Adds new volume info from the given volumeMetadata. If the corresponding
   * volume info has already been added, the volumeMetadata is ignored.
   */
  private addVolumeInfo_(volumeInfo: VolumeInfo): VolumeInfo {
    const volumeType = volumeInfo.volumeType;

    // We don't show Downloads and Drive on volume list if they have
    // mount error, since users can do nothing in this situation. We
    // show Removable and Provided volumes regardless of mount error
    // so that users can unmount or format the volume.
    // TODO(fukino): Once the Files app gets ready, show erroneous
    // Drive volume so that users can see auth warning banner on the
    // volume. crbug.com/517772.
    let shouldShow = true;
    switch (volumeType) {
      case VolumeManagerCommon.VolumeType.DOWNLOADS:
      case VolumeManagerCommon.VolumeType.DRIVE:
        shouldShow = !!volumeInfo.fileSystem;
        break;
    }

    if (!shouldShow) {
      return volumeInfo;
    }

    if (this.volumeInfoList.findIndex(volumeInfo.volumeId) === -1) {
      this.volumeInfoList.add(volumeInfo);

      // Update the network connection status, because until the drive
      // is initialized, the status is set to not ready.
      // TODO(mtomasz): The connection status should be migrated into
      // chrome.fileManagerPrivate.VolumeMetadata.
      if (volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
        this.onDriveConnectionStatusChanged_();
      }
    } else if (volumeType === VolumeManagerCommon.VolumeType.REMOVABLE) {
      // Update for remounted USB external storage, because they were
      // remounted to switch read-only policy.
      this.volumeInfoList.add(volumeInfo);
    }

    return volumeInfo;
  }

  /**
   * Initializes mount points.
   */
  async initialize(): Promise<void> {
    let finished = false;
    /**
     * Resolves the initialization promise to unblock any code awaiting for
     * it.
     */
    const finishInitialization = () => {
      if (finished) {
        return;
      }
      finished = true;
      console.warn('Volumes initialization finished');
      if (this.finishInitialization_) {
        this.finishInitialization_();
      }
    };

    try {
      console.warn('Getting volumes');
      let volumeMetadataList: chrome.fileManagerPrivate.VolumeMetadata[] =
          await promisify(chrome.fileManagerPrivate.getVolumeMetadataList);
      if (!volumeMetadataList) {
        console.warn('Cannot get volumes');
        finishInitialization();
        return;
      }
      volumeMetadataList = volumeMetadataList.filter(volume => !volume.hidden);
      console.debug(`There are ${volumeMetadataList.length} volumes`);

      let counter = 0;

      // Create VolumeInfo for each volume.
      volumeMetadataList.map(async (volumeMetadata, idx) => {
        const volumeId = volumeMetadata.volumeId;
        let volumeInfo = null;
        try {
          console.debug(`Initializing volume #${idx} '${volumeId}'`);
          // createVolumeInfo() requests the filesystem and resolve its root,
          // after that it only creates a VolumeInfo.
          volumeInfo = await volumeManagerUtil.createVolumeInfo(volumeMetadata);
          // Add addVolumeInfo_() changes the VolumeInfoList which propagates
          // to the foreground.
          this.addVolumeInfo_(volumeInfo);
          console.debug(`Initialized volume #${idx} ${volumeId}'`);
        } catch (error) {
          console.warn(`Error initiliazing #${idx} ${volumeId}: ${error}`);
        } finally {
          counter += 1;
          // Finish after all volumes have been processed, or at least Downloads
          // or Drive.
          const isDriveOrDownloads = volumeInfo &&
              (volumeInfo.volumeType ==
                   VolumeManagerCommon.VolumeType.DOWNLOADS ||
               volumeInfo.volumeType == VolumeManagerCommon.VolumeType.DRIVE);
          if (counter === volumeMetadataList.length || isDriveOrDownloads) {
            finishInitialization();
          }
        }
      });

      // At this point the volumes are still initializing.
      console.warn(
          `Queued the initialization of all ` +
          `${volumeMetadataList.length} volumes`);

      if (volumeMetadataList.length === 0) {
        finishInitialization();
      }
    } catch (error) {
      finishInitialization();
      throw error;
    }
  }

  /**
   * Event handler called when some volume was mounted or unmounted.
   */
  private async onMountCompleted_(
      event: chrome.fileManagerPrivate.MountCompletedEvent) {
    // Wait for the initialization to guarantee that the initialize() runs for
    // some volumes before any mount event, because the mounted volume can be
    // unresponsive, getting stuck when resolving the root in the method
    // createVolumeInfo(). crbug.com/504366
    await this.waitForInitialization_;

    const {eventType, status, volumeMetadata} = event;
    const {sourcePath = '', volumeId} = volumeMetadata;

    switch (eventType) {
      case 'mount': {
        const requestKey = this.makeRequestKey_('mount', sourcePath);

        switch (status) {
          case 'success':
          case VolumeManagerCommon.VolumeError.UNKNOWN_FILESYSTEM:
          case VolumeManagerCommon.VolumeError.UNSUPPORTED_FILESYSTEM: {
            console.debug(`Mounted '${sourcePath}' as '${volumeId}'`);
            if (volumeMetadata.hidden) {
              console.debug(`Mount discarded for hidden volume: '${volumeId}'`);
              this.finishRequest_(requestKey, status);
              return;
            }

            let volumeInfo;
            try {
              volumeInfo =
                  await volumeManagerUtil.createVolumeInfo(volumeMetadata);
            } catch (error: any) {
              console.warn(
                  'Unable to create volumeInfo for ' +
                  `${volumeId} mounted on ${sourcePath}.` +
                  `Mount status: ${status}. Error: ${error.stack || error}.`);
              this.finishRequest_(requestKey, status);
              throw (error);
            }
            this.addVolumeInfo_(volumeInfo);
            this.finishRequest_(requestKey, status, volumeInfo);
            return;
          }

          case VolumeManagerCommon.VolumeError.PATH_ALREADY_MOUNTED: {
            console.warn(
                `Cannot mount (redacted): Already mounted as '${volumeId}'`);
            console.debug(`Cannot mount '${sourcePath}': Already mounted as '${
                volumeId}'`);
            const navigationEvent =
                new Event(VolumeManagerCommon.VOLUME_ALREADY_MOUNTED) as
                VolumeAlreadyMountedEvent;
            navigationEvent.volumeId = volumeId;
            this.dispatchEvent(navigationEvent);
            this.finishRequest_(requestKey, status);
            return;
          }

          case VolumeManagerCommon.VolumeError.NEED_PASSWORD:
          case VolumeManagerCommon.VolumeError.CANCELLED:
          default:
            console.warn('Cannot mount (redacted):', status);
            console.debug(`Cannot mount '${sourcePath}':`, status);
            this.finishRequest_(requestKey, status);
            return;
        }
      }

      case 'unmount': {
        const requestKey = this.makeRequestKey_('unmount', volumeId);
        const volumeInfoIndex = this.volumeInfoList.findIndex(volumeId);
        const volumeInfo = volumeInfoIndex !== -1 ?
            this.volumeInfoList.item(volumeInfoIndex) :
            null;

        switch (status) {
          case 'success': {
            const requested = requestKey in this.requests_;
            if (!requested && volumeInfo) {
              console.debug(`Unmounted '${volumeId}' without request`);
              this.dispatchEvent(new CustomEvent(
                  'externally-unmounted', {detail: volumeInfo}));
            } else {
              console.debug(`Unmounted '${volumeId}'`);
            }
            getStore().dispatch(removeVolume({volumeId}));
            this.volumeInfoList.remove(volumeId);
            this.finishRequest_(requestKey, status);
            return;
          }

          default:
            console.warn('Cannot unmount (redacted):', status);
            console.debug(`Cannot unmount '${volumeId}':`, status);
            this.finishRequest_(requestKey, status);
            return;
        }
      }
    }
  }

  /**
   * Creates string to match mount events with requests.
   * @param requestType 'mount' | 'unmount'. TODO(hidehiko): Replace by enum.
   * @param argument Argument describing the request, eg. source file
   *     path of the archive to be mounted, or a volumeId for unmounting.
   * @return Key for |this.requests_|.
   */
  private makeRequestKey_(requestType: string, argument: string): string {
    return requestType + ':' + argument;
  }

  async mountArchive(fileUrl: string, password?: string): Promise<VolumeInfo> {
    const path: string =
        await promisify(chrome.fileManagerPrivate.addMount, fileUrl, password);
    console.debug(`Mounting '${path}'`);
    const key = this.makeRequestKey_('mount', path);
    return this.startRequest_(key);
  }

  async cancelMounting(fileUrl: string): Promise<void> {
    console.debug(`Cancelling mounting archive at '${fileUrl}'`);
    return promisify(chrome.fileManagerPrivate.cancelMounting, fileUrl);
  }

  async unmount({volumeId}: VolumeInfo): Promise<void> {
    console.debug(`Unmounting '${volumeId}'`);
    const key = this.makeRequestKey_('unmount', volumeId);
    const request = this.startRequest_(key);
    await promisify(chrome.fileManagerPrivate.removeMount, volumeId);
    await request;
  }

  configure(volumeInfo: VolumeInfo): Promise<void> {
    return promisify(
        chrome.fileManagerPrivate.configureVolume, volumeInfo.volumeId);
  }

  getVolumeInfo(entry: Entry|FilesAppEntry): VolumeInfo|null {
    if (!entry) {
      console.warn(`Invalid entry passed to getVolumeInfo: ${entry}`);
      return null;
    }

    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.fileSystem &&
          isSameFileSystem(volumeInfo.fileSystem, entry.filesystem)) {
        return volumeInfo;
      }
      // Additionally, check fake entries.
      for (const key in volumeInfo.fakeEntries) {
        const fakeEntry = volumeInfo.fakeEntries[key];
        if (isSameEntry(fakeEntry, entry)) {
          return volumeInfo;
        }
      }
    }

    return null;
  }

  getCurrentProfileVolumeInfo(volumeType: VolumeManagerCommon.VolumeType):
      VolumeInfo|null {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.profile.isCurrentProfile &&
          volumeInfo.volumeType === volumeType) {
        return volumeInfo;
      }
    }
    return null;
  }

  getLocationInfo(entry: Entry|FilesAppEntry): EntryLocation|null {
    if (!entry) {
      console.warn(`Invalid entry passed to getLocationInfo: ${entry}`);
      return null;
    }

    const volumeInfo = this.getVolumeInfo(entry);

    if (isFakeEntry(entry)) {
      const rootType = getRootType(entry);
      assert(rootType);

      // Aggregated views like RECENTS and TRASH exist as fake entries but may
      // actually defer their logic to some underlying implementation or
      // delegate to the location filesystem.
      let isReadOnly = true;
      if (rootType === VolumeManagerCommon.RootType.RECENT ||
          rootType === VolumeManagerCommon.RootType.TRASH) {
        isReadOnly = false;
      }
      return new EntryLocationImpl(
          volumeInfo, rootType, true /* The entry points a root directory. */,
          isReadOnly);
    }

    if (!volumeInfo) {
      return null;
    }

    let rootType;
    let isReadOnly;
    let isRootEntry;
    if (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
      // For Drive, the roots are /root, /team_drives, /Computers and /other,
      // instead of /. Root URLs contain trailing slashes.
      if (entry.fullPath == '/root' || entry.fullPath.indexOf('/root/') === 0) {
        rootType = VolumeManagerCommon.RootType.DRIVE;
        isReadOnly = volumeInfo.isReadOnly;
        isRootEntry = entry.fullPath === '/root';
      } else if (
          entry.fullPath == VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH ||
          entry.fullPath.indexOf(
              VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH + '/') === 0) {
        if (entry.fullPath ==
            VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH) {
          rootType = VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT;
          isReadOnly = true;
          isRootEntry = true;
        } else {
          rootType = VolumeManagerCommon.RootType.SHARED_DRIVE;
          if (isTeamDriveRoot(entry)) {
            isReadOnly = false;
            isRootEntry = true;
          } else {
            // Regular files/directories under Shared Drives.
            isRootEntry = false;
            isReadOnly = volumeInfo.isReadOnly;
          }
        }
      } else if (
          entry.fullPath == VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH ||
          entry.fullPath.indexOf(
              VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH + '/') === 0) {
        if (entry.fullPath == VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH) {
          rootType = VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT;
          isReadOnly = true;
          isRootEntry = true;
        } else {
          rootType = VolumeManagerCommon.RootType.COMPUTER;
          if (isComputersRoot(entry)) {
            isReadOnly = true;
            isRootEntry = true;
          } else {
            // Regular files/directories under a Computer entry.
            isRootEntry = false;
            isReadOnly = volumeInfo.isReadOnly;
          }
        }
      } else if (
          entry.fullPath === '/.files-by-id' ||
          entry.fullPath.indexOf('/.files-by-id/') === 0) {
        rootType = VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME;

        // /.files-by-id/<id> is read-only, but /.files-by-id/<id>/foo is
        // read-write.
        isReadOnly = entry.fullPath.split('/').length < 4;
        isRootEntry = entry.fullPath === '/.files-by-id';
      } else if (
          entry.fullPath === '/.shortcut-targets-by-id' ||
          entry.fullPath.indexOf('/.shortcut-targets-by-id/') === 0) {
        rootType = VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME;

        // /.shortcut-targets-by-id/<id> is read-only, but
        // /.shortcut-targets-by-id/<id>/foo is read-write.
        isReadOnly = entry.fullPath.split('/').length < 4;
        isRootEntry = entry.fullPath === '/.shortcut-targets-by-id';
      } else if (
          entry.fullPath === '/.Trash-1000' ||
          entry.fullPath.indexOf('/.Trash-1000/') === 0) {
        // Drive uses "$topdir/.Trash-$uid" as the trash dir as per XDG spec.
        // User chronos is always uid 1000.
        rootType = VolumeManagerCommon.RootType.TRASH;
        isReadOnly = false;
        isRootEntry = entry.fullPath === '/.Trash-1000';
      } else {
        // Accessing Drive files outside of /drive/root and /drive/other is not
        // allowed, but can happen. Therefore returning null.
        return null;
      }
    } else {
      assert(volumeInfo.volumeType);
      rootType =
          VolumeManagerCommon.getRootTypeFromVolumeType(volumeInfo.volumeType);
      isRootEntry = isSameEntry(entry, volumeInfo.fileSystem.root);
      // Although "Play files" root directory is writable in file system level,
      // we prohibit write operations on it in the UI level to avoid confusion.
      // Users can still have write access in sub directories like
      // /Play files/Pictures, /Play files/DCIM, etc...
      if (volumeInfo.volumeType ==
              VolumeManagerCommon.VolumeType.ANDROID_FILES &&
          isRootEntry) {
        isReadOnly = true;
      } else {
        isReadOnly = volumeInfo.isReadOnly;
      }
    }

    return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, isReadOnly);
  }

  findByDevicePath(devicePath: string): VolumeInfo|null {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.devicePath && volumeInfo.devicePath === devicePath) {
        return volumeInfo;
      }
    }
    return null;
  }

  whenVolumeInfoReady(volumeId: string): Promise<VolumeInfo> {
    return new Promise((fulfill) => {
      const handler = () => {
        const index = this.volumeInfoList.findIndex(volumeId);
        if (index !== -1) {
          fulfill(this.volumeInfoList.item(index));
          this.volumeInfoList.removeEventListener('splice', handler);
        }
      };
      this.volumeInfoList.addEventListener('splice', handler);
      handler();
    });
  }

  getDefaultDisplayRoot(
      callback: ((arg0: DirectoryEntry|FilesAppDirEntry|null) => void)) {
    console.warn('Unexpected call to VolumeManagerImpl.getDefaultDisplayRoot');
    callback(null);
  }

  /**
   * @param key Key produced by |makeRequestKey_|.
   * @return Fulfilled on success, otherwise rejected with a
   *     VolumeManagerCommon.VolumeError.
   */
  private startRequest_(key: string): Promise<VolumeInfo> {
    return new Promise((successCallback, errorCallback) => {
      if (key in this.requests_) {
        const request = this.requests_[key]!;
        request.successCallbacks.push(
            successCallback as RequestSuccessCallback);
        request.errorCallbacks.push(errorCallback);
      } else {
        this.requests_[key] = {
          successCallbacks: [successCallback as RequestSuccessCallback],
          errorCallbacks: [errorCallback],

          timeout: setTimeout(
              this.onTimeout_.bind(this, key), volumeManagerUtil.TIMEOUT),
        };
      }
    });
  }

  /**
   * Called if no response received in |TIMEOUT|.
   * @param key Key produced by |makeRequestKey_|.
   */
  private onTimeout_(key: string) {
    this.invokeRequestCallbacks_(
        this.requests_[key]!, VolumeManagerCommon.VolumeError.TIMEOUT);
    delete this.requests_[key];
  }

  /**
   * @param key Key produced by |makeRequestKey_|.
   * @param status Status received from the API.
   * @param volumeInfo Volume info of the mounted volume.
   */
  private finishRequest_(
      key: string, status: VolumeManagerCommon.VolumeError|string,
      volumeInfo?: VolumeInfo) {
    const request = this.requests_[key];
    if (!request) {
      return;
    }

    clearTimeout(request.timeout);
    this.invokeRequestCallbacks_(request, status, volumeInfo);
    delete this.requests_[key];
  }

  /**
   * @param request Structure created in |startRequest_|.
   * @param status If status === 'success' success callbacks are called.
   * @param volumeInfo Volume info of the mounted volume.
   */
  private invokeRequestCallbacks_(
      request: Request, status: VolumeManagerCommon.VolumeError,
      volumeInfo?: VolumeInfo) {
    if (status === 'success') {
      request.successCallbacks.map(cb => cb(volumeInfo));

    } else {
      volumeManagerUtil.validateError(status);
      request.errorCallbacks.map(cb => cb(status));
    }
  }

  hasDisabledVolumes(): boolean {
    return false;
  }

  isDisabled(_volume: VolumeManagerCommon.VolumeType): boolean {
    return false;
  }

  isAllowedVolume(_volumeInfo: VolumeInfo): boolean {
    return true;
  }
}
