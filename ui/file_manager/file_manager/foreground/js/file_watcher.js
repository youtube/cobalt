// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {AsyncQueue} from '../../common/js/async_util.js';
import {util} from '../../common/js/util.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';

/** Watches for changes in the tracked directory. */
export class FileWatcher extends EventTarget {
  constructor() {
    super();

    this.queue_ = new AsyncQueue();
    this.watchedDirectoryEntry_ = null;

    this.onDirectoryChangedBound_ = this.onDirectoryChanged_.bind(this);
    chrome.fileManagerPrivate.onDirectoryChanged.addListener(
        this.onDirectoryChangedBound_);
  }

  /**
   * Stops watching (must be called before page unload).
   */
  dispose() {
    chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
        this.onDirectoryChangedBound_);
    if (this.watchedDirectoryEntry_) {
      this.resetWatchedEntry_();
    }
  }

  /**
   * Called when a file in the watched directory is changed.
   * @param {chrome.fileManagerPrivate.FileWatchEvent} event Change event.
   * @private
   */
  onDirectoryChanged_(event) {
    const fireWatcherDirectoryChanged = changedFiles => {
      const e = new Event('watcher-directory-changed');

      if (changedFiles) {
        e.changedFiles = changedFiles;
      }

      this.dispatchEvent(e);
    };

    if (this.watchedDirectoryEntry_) {
      const eventURL = event.entry.toURL();
      const watchedDirURL = this.watchedDirectoryEntry_.toURL();

      if (eventURL === watchedDirURL) {
        fireWatcherDirectoryChanged(event.changedFiles);
      } else if (watchedDirURL.startsWith(eventURL)) {
        // When watched directory is deleted by the change in parent directory,
        // notify it as watcher directory changed.
        this.watchedDirectoryEntry_.getDirectory(
            this.watchedDirectoryEntry_.fullPath, {create: false}, null, () => {
              fireWatcherDirectoryChanged(null);
            });
      }
    }
  }

  /**
   * Changes the watched directory. In case of a fake entry, the watch is
   * just released, since there is no reason to track a fake directory.
   *
   * @param {!DirectoryEntry|!FilesAppEntry} entry Directory entry to be
   *     tracked, or the fake entry.
   * @return {!Promise}
   */
  changeWatchedDirectory(entry) {
    if (!util.isFakeEntry(entry)) {
      return this.changeWatchedEntry_(
          /** @type {!DirectoryEntry} */ (util.unwrapEntry(entry)));
    } else {
      return this.resetWatchedEntry_();
    }
  }

  /**
   * Resets the watched entry. It's a best effort method.
   * @return {!Promise}
   * @private
   */
  resetWatchedEntry_() {
    // Run the tasks in the queue to avoid races.
    return new Promise((fulfill, reject) => {
      this.queue_.run(callback => {
        // Release the watched directory.
        if (this.watchedDirectoryEntry_) {
          chrome.fileManagerPrivate.removeFileWatch(
              this.watchedDirectoryEntry_, result => {
                if (chrome.runtime.lastError) {
                  console.warn(`Cannot remove watcher for (redacted): ${
                      chrome.runtime.lastError.message}`);
                  console.info(`Cannot remove watcher for '${
                      this.watchedDirectoryEntry_.toURL()}': ${
                      chrome.runtime.lastError.message}`);
                }
                // Even on error reset the watcher locally, so at least the
                // notifications are discarded.
                this.watchedDirectoryEntry_ = null;
                fulfill();
                callback();
              });
        } else {
          fulfill();
          callback();
        }
      });
    });
  }

  /**
   * Sets the watched entry to the passed directory. It's a best effort method.
   * @param {!DirectoryEntry} entry Directory to be watched.
   * @return {!Promise}
   * @private
   */
  changeWatchedEntry_(entry) {
    return new Promise((fulfill, reject) => {
      const setEntryClosure = () => {
        // Run the tasks in the queue to avoid races.
        this.queue_.run(callback => {
          chrome.fileManagerPrivate.addFileWatch(entry, result => {
            if (chrome.runtime.lastError) {
              // Most probably setting the watcher is not supported on the
              // file system type.
              console.info(`Cannot add watcher for '${entry.toURL()}': ${
                  chrome.runtime.lastError.message}`);
              this.watchedDirectoryEntry_ = null;
              fulfill();
            } else {
              this.watchedDirectoryEntry_ = assert(entry);
              fulfill();
            }
            callback();
          });
        });
      };

      // Reset the watched directory first, then set the new watched directory.
      return this.resetWatchedEntry_().then(setEntryClosure);
    });
  }

  /**
   * @return {DirectoryEntry} Current watched directory entry.
   */
  getWatchedDirectoryEntry() {
    return this.watchedDirectoryEntry_;
  }
}
