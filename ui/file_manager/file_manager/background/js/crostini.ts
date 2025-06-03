// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles shares for Crostini VMs.
 * Disable type checking for closure, as it is done by the typescript compiler.
 * @suppress {checkTypes}
 */

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../externs/volume_manager.js';

/**
 * Default Crostini VM is 'termina'.
 */
const DEFAULT_VM = 'termina';

/**
 * Plugin VM 'PvmDefault'.
 */
const PLUGIN_VM = 'PvmDefault';

/**
 * Valid root types to their share location.
 */
const VALID_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.DOWNLOADS, 'Downloads'],
  [VolumeManagerCommon.RootType.REMOVABLE, 'Removable'],
  [VolumeManagerCommon.RootType.ANDROID_FILES, 'AndroidFiles'],
  [VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT, 'DriveComputers'],
  [VolumeManagerCommon.RootType.COMPUTER, 'DriveComputers'],
  [VolumeManagerCommon.RootType.DRIVE, 'MyDrive'],
  [VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT, 'TeamDrive'],
  [VolumeManagerCommon.RootType.SHARED_DRIVE, 'TeamDrive'],
  [VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME, 'SharedWithMe'],
  [VolumeManagerCommon.RootType.CROSTINI, 'Crostini'],
  [VolumeManagerCommon.RootType.GUEST_OS, 'GuestOs'],
  [VolumeManagerCommon.RootType.ARCHIVE, 'Archive'],
  [VolumeManagerCommon.RootType.SMB, 'SMB'],
]);

/**
 * Implementation of Crostini shared path state handler.
 */
export class CrostiniImpl {
  /**
   * Keys maintaining enablement state for VMs that is keyed by vm then subkeyed
   * by the container name.
   */
  private enabled_: {[key: string]: {[key: string]: boolean}} = {};

  /**
   * A list of shared paths keyed by the VM name.
   */
  private sharedPaths_: {[key: string]: string[]} = {};

  /**
   * The volume manager instance.
   */
  private volumeManager_: VolumeManager|null = null;

  /**
   * Initialize enabled settings and register for any shared path changes.
   * Must be done after loadTimeData is available.
   */
  initEnabled() {
    const guests = loadTimeData.getValue('VMS_FOR_SHARING');
    for (const guest of guests) {
      this.setEnabled(guest.vmName, guest.containerName, true);
    }
    chrome.fileManagerPrivate.onCrostiniChanged.addListener(
        this.onCrostiniChanged_.bind(this));
  }

  /**
   * Initialize Volume Manager.
   */
  initVolumeManager(volumeManager: VolumeManager) {
    this.volumeManager_ = volumeManager;
  }

  /**
   * Set whether the specified Guest is enabled.
   */
  setEnabled(vmName: string, containerName: string, enabled: boolean) {
    if (!this.enabled_[vmName]) {
      this.enabled_[vmName] = {};
    }
    this.enabled_[vmName]![containerName] = enabled;
  }

  /**
   * Returns true if the specified VM is enabled.
   */
  isEnabled(vmName: string) {
    return (!!this.enabled_[vmName]) &&
        Object.values(this.enabled_[vmName]!).includes(true);
  }

  /**
   * Get the root type for the supplied `entry`.
   */
  private getRoot_(entry: Entry) {
    const info =
        this.volumeManager_ && this.volumeManager_.getLocationInfo(entry);
    return info && info.rootType;
  }

  /**
   * Registers an entry as a shared path for the specified VM.
   */
  registerSharedPath(vmName: string, entry: Entry) {
    const url = entry.toURL();
    // Remove any existing paths that are children of the new path.
    // These paths will still be shared as a result of a parent path being
    // shared, but if the parent is unshared in the future, these children
    // paths should not remain.
    for (const [path, _] of Object.entries(this.sharedPaths_)) {
      if (path.startsWith(url)) {
        this.unregisterSharedPath_(vmName, path);
      }
    }
    if (this.sharedPaths_[url]) {
      this.sharedPaths_[url]!.push(vmName);
    } else {
      this.sharedPaths_[url] = [vmName];
    }
  }

  /**
   * Unregisters path as a shared path from the specified VM.
   */
  private unregisterSharedPath_(vmName: string, path: string) {
    const vms = this.sharedPaths_[path];
    if (vms) {
      const newVms = vms.filter(vm => vm != vmName);
      if (newVms.length > 0) {
        this.sharedPaths_[path] = newVms;
      } else {
        delete this.sharedPaths_[path];
      }
    }
  }

  /**
   * Unregisters entry as a shared path from the specified VM.
   */
  unregisterSharedPath(vmName: string, entry: Entry) {
    this.unregisterSharedPath_(vmName, entry.toURL());
  }

  /**
   * Handles events for enable/disable, share/unshare.
   */
  private onCrostiniChanged_(event: chrome.fileManagerPrivate.CrostiniEvent) {
    const CrostiniEventType = chrome.fileManagerPrivate.CrostiniEventType;
    switch (event.eventType) {
      case CrostiniEventType.ENABLE:
        this.setEnabled(event.vmName, event.containerName, true);
        break;
      case CrostiniEventType.DISABLE:
        this.setEnabled(event.vmName, event.containerName, false);
        break;
      case CrostiniEventType.SHARE:
        for (const entry of event.entries) {
          this.registerSharedPath(event.vmName, assert(entry));
        }
        break;
      case CrostiniEventType.UNSHARE:
        for (const entry of event.entries) {
          this.unregisterSharedPath(event.vmName, assert(entry));
        }
        break;
    }
  }

  /**
   * Returns true if entry is shared with the specified VM. Returns true if path
   * is shared either by a direct share or from one of its ancestor directories.
   */
  isPathShared(vmName: string, entry: Entry) {
    // Check path and all ancestor directories.
    let path = entry.toURL();
    let root = path;
    if (entry && entry.filesystem && entry.filesystem.root) {
      root = entry.filesystem.root.toURL();
    }

    while (path.length > root.length) {
      const vms = this.sharedPaths_[path];
      if (vms && vms.includes(vmName)) {
        return true;
      }
      path = path.substring(0, path.lastIndexOf('/'));
    }
    const rootVms = this.sharedPaths_[root];
    return !!rootVms && rootVms.includes(vmName);
  }

  /**
   * Returns true if entry can be shared with the specified VM.
   */
  canSharePath(vmName: string, entry: Entry, persist: boolean) {
    if (!this.isEnabled(vmName)) {
      return false;
    }

    // Only directories for persistent shares.
    if (persist && !entry.isDirectory) {
      return false;
    }

    const root = this.getRoot_(entry);

    // TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
    // Disallow Computers Grand Root, and Computer Root.
    if (root === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
        (root === VolumeManagerCommon.RootType.COMPUTER &&
         entry.fullPath.split('/').length <= 3)) {
      return false;
    }

    // TODO(crbug.com/958840): Sharing Play files root is disallowed until
    // we can ensure it will not also share Downloads.
    if (root === VolumeManagerCommon.RootType.ANDROID_FILES &&
        entry.fullPath === '/') {
      return false;
    }

    // Special case to disallow PluginVm sharing on /MyFiles/PluginVm and
    // subfolders since it gets shared by default.
    if (vmName === PLUGIN_VM &&
        root === VolumeManagerCommon.RootType.DOWNLOADS &&
        entry.fullPath.split('/')[1] === PLUGIN_VM) {
      return false;
    }

    // Disallow sharing LinuxFiles with itself.
    if (vmName === DEFAULT_VM &&
        root === VolumeManagerCommon.RootType.CROSTINI) {
      return false;
    }

    // Cannot share root of Shared with me since it represents 2 dirs:
    // `.files-by-id` and `.shortcut-targets-by-id`.
    if (root === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME &&
        entry.fullPath === '/') {
      return false;
    }

    return VALID_ROOT_TYPES_FOR_SHARE.has(root || '');
  }
}
