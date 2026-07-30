// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of ContactManagerInterface for testing.
 */

import 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

/**
 * Fake implementation of ContactManagerInterface
 */
export class FakeContactManager {
  constructor() {
    /**
     * Restore ContactRecord type when migrated to TS.
     * @private {?Array<!Object>}
     */
    this.contactRecords_ = null;
    /** @private {!Array<!string>} */
    this.allowedContacts_ = [];
    /** @private {number} */
    this.numUnreachable_ = 3;
    /**
     * Restore DownloadContactsObserverInterface type when migrated to TS.
     * @private {?Object}
     */
    this.observer_;
    /** @private {Object} */
    this.$ = {
      close() {},
    };
    /** @private {boolean} */
    this.downloadContactsCalled_ = false;
  }

  /**
   * Restore DownloadContactsObserverInterface type when migrated to TS.
   * @param {!Object} observer
   */
  addDownloadContactsObserver(observer) {
    // Just support a single observer for testing.
    this.observer_ = observer;
  }

  downloadContacts() {
    // This does nothing intentionally, call failDownload() or
    // completeDownload() to simulate a response.
    this.downloadContactsCalled = true;
  }

  /**
   * @param {!Array<!string>} allowedContacts
   */
  setAllowedContacts(allowedContacts) {
    this.allowedContacts = allowedContacts;
  }

  /**
   * @param {number} numUnreachable
   */
  setNumUnreachable(numUnreachable) {
    this.numUnreachable_ = numUnreachable;
  }

  setupContactRecords() {
    this.contactRecords = [
      {
        id: '1',
        personName: 'Jane Doe',
        identifiers: [
          {accountName: 'jane@gmail.com'},
          {phoneNumber: '555-1212'},
        ],
      },
      {
        id: '2',
        personName: 'John Smith',
        identifiers: [
          {phoneNumber: '555-5555'},
          {accountName: 'smith@google.com'},
        ],
      },
    ];
    this.allowedContacts = ['1'];
  }

  failDownload() {
    this.observer_.onContactsDownloadFailed();
  }

  completeDownload() {
    this.observer_.onContactsDownloaded(
        this.allowedContacts, this.contactRecords || [],
        /*num_unreachable_contacts_filtered_out=*/ this.numUnreachable_);
  }

  /** @return {?Array<!Object>} */
  get contactRecords() {
    return this.contactRecords_ || null;
  }
  /** @param {?Array<!Object>} val */
  set contactRecords(val) {
    this.contactRecords_ = val;
  }

  /** @return {!Array<!string>} */
  get allowedContacts() {
    return this.allowedContacts_;
  }
  /** @param {!Array<!string>} val */
  set allowedContacts(val) {
    this.allowedContacts_ = val || [];
  }

  /** @return {boolean} */
  get downloadContactsCalled() {
    return !!this.downloadContactsCalled_;
  }
  /** @param {boolean} val */
  set downloadContactsCalled(val) {
    this.downloadContactsCalled_ = val;
  }
}
