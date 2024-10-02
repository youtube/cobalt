// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';
import {Store} from '../../externs/ts/store.js';
import {updateMetadata} from '../../state/actions/all_entries.js';
import {getStore} from '../../state/store.js';

import {DirectoryModel} from './directory_model.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {FileMetadataFormatter} from './ui/file_metadata_formatter.js';
import {ListContainer} from './ui/list_container.js';

/**
 * Controller for list contents update.
 */
export class MetadataUpdateController {
  /**
   * @param {!ListContainer} listContainer
   * @param {!DirectoryModel} directoryModel
   * @param {!MetadataModel} metadataModel
   * @param {!FileMetadataFormatter} fileMetadataFormatter
   */
  constructor(
      listContainer, directoryModel, metadataModel, fileMetadataFormatter) {
    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!MetadataModel} */
    this.metadataModel_ = metadataModel;

    /** @private @const {!ListContainer} */
    this.listContainer_ = listContainer;

    /** @private @const {!FileMetadataFormatter} */
    this.fileMetadataFormatter_ = fileMetadataFormatter;

    /** @private {!Store} */
    this.store_ = getStore();

    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        this.onPreferencesChanged_.bind(this));
    this.onPreferencesChanged_();
    metadataModel.addEventListener(
        'update', this.onCachedMetadataUpdate_.bind(this));

    // Update metadata to change 'Today' and 'Yesterday' dates.
    const today = new Date();
    today.setHours(0);
    today.setMinutes(0);
    today.setSeconds(0);
    today.setMilliseconds(0);
    setTimeout(
        this.dailyUpdateModificationTime_.bind(this),
        today.getTime() + MetadataUpdateController.MILLISECONDS_IN_DAY_ -
            Date.now() + 1000);
  }

  /**
   * Clears metadata cache for the current directory and its decendents.
   */
  refreshCurrentDirectoryMetadata() {
    const entries = this.directoryModel_.getFileList().slice();
    const directoryEntry = this.directoryModel_.getCurrentDirEntry();
    if (!directoryEntry) {
      return;
    }

    // TODO(dgozman): refresh content metadata only when modificationTime
    // changed.
    const isFakeEntry = util.isFakeEntry(directoryEntry);
    const changedEntries =
        (isFakeEntry ? [] : [directoryEntry]).concat(entries);
    this.metadataModel_.notifyEntriesChanged(changedEntries);

    // We don't pass callback here. When new metadata arrives, we have an
    // observer registered to update the UI.
    this.metadataModel_.get(
        changedEntries, this.directoryModel_.getPrefetchPropertyNames());
  }

  /**
   * Handles local metadata changes in the currect directory.
   * @param {Event} event Change event.
   * @private
   */
  onCachedMetadataUpdate_(event) {
    this.updateStore_(event.entries);
    this.listContainer_.dataModel.refreshGroupBySnapshot();
    // TODO(hirono): Specify property name instead of metadata type.
    this.listContainer_.currentView.updateListItemsMetadata(
        'filesystem', event.entries);
    this.listContainer_.currentView.updateListItemsMetadata(
        'external', event.entries);
  }

  /**
   * @private
   */
  dailyUpdateModificationTime_() {
    const entries = /** @type {!Array<!Entry>} */ (
        this.directoryModel_.getFileList().slice());
    this.metadataModel_.get(entries, ['modificationTime']).then(() => {
      this.listContainer_.currentView.updateListItemsMetadata(
          'filesystem', entries);
    });
    setTimeout(
        this.dailyUpdateModificationTime_.bind(this),
        MetadataUpdateController.MILLISECONDS_IN_DAY_);
  }

  /**
   * @private
   */
  onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(prefs => {
      const use12hourClock = !prefs.use24hourClock;
      this.fileMetadataFormatter_.setDateTimeFormat(use12hourClock);
      // TODO(oka): Remove these two lines, and add fileMetadataFormatter to
      // constructor for each field instead.
      this.listContainer_.table.setDateTimeFormat(use12hourClock);
      this.refreshCurrentDirectoryMetadata();
    });
  }

  /**
   * Sends the new metadata to the Store.
   * @private
   */
  updateStore_(entries) {
    const metadata = entries.map(
        e => ({
          entry: e,
          metadata: this.metadataModel_.getCache(
              [e], this.directoryModel_.getPrefetchPropertyNames())[0],
        }));

    this.store_.dispatch(updateMetadata({metadata}));
  }
}

/**
 * Number of milliseconds in a day.
 * @private @const {number}
 */
MetadataUpdateController.MILLISECONDS_IN_DAY_ = 24 * 60 * 60 * 1000;
