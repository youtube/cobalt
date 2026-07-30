// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageRemote, ProfileData, SwitchToTabInfo, TabSearchApiProxy, TokenRange} from 'chrome://tab-search.top-chrome/tab_search.js';
import {PageCallbackRouter} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestTabSearchApiProxy extends TestBrowserProxy implements
    TabSearchApiProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  private profileData_?: ProfileData;
  private ranges_?: TokenRange[][];
  private isSplit_: boolean = false;

  constructor() {
    super([
      'closeTab',
      'closeTabs',
      'closeWebUiTab',
      'getProfileData',
      'getIsSplit',
      'openRecentlyClosedEntry',
      'replaceActiveSplitTab',
      'switchToTab',
      'saveRecentlyClosedExpandedPref',
      'maybeShowUi',
      'getRangesIgnoringCaseAndAccents',
    ]);

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  closeTab(tabId: number) {
    this.methodCalled('closeTab', [tabId]);
  }

  closeTabs(tabIds: number[]) {
    this.methodCalled('closeTabs', [tabIds]);
  }

  closeWebUiTab() {
    this.methodCalled('closeWebUiTab', []);
  }

  getProfileData() {
    this.methodCalled('getProfileData');
    return Promise.resolve({profileData: this.profileData_!});
  }

  getIsSplit() {
    this.methodCalled('getIsSplit');
    return Promise.resolve({isSplit: this.isSplit_});
  }

  openRecentlyClosedEntry(id: number, withSearch: boolean, isTab: boolean) {
    this.methodCalled('openRecentlyClosedEntry', [id, withSearch, isTab]);
  }

  replaceActiveSplitTab(replacementTabId: number) {
    this.methodCalled('replaceActiveSplitTab', [replacementTabId]);
  }

  switchToTab(info: SwitchToTabInfo) {
    this.methodCalled('switchToTab', [info]);
  }

  saveRecentlyClosedExpandedPref(expanded: boolean) {
    this.methodCalled('saveRecentlyClosedExpandedPref', [expanded]);
  }

  maybeShowUi() {
    this.methodCalled('maybeShowUi');
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }

  setProfileData(profileData: ProfileData) {
    this.profileData_ = profileData;
  }

  setIsSplit(isSplit: boolean) {
    this.isSplit_ = isSplit;
  }

  setRanges(ranges: TokenRange[][]) {
    this.ranges_ = ranges;
  }

  getRangesIgnoringCaseAndAccents(searchText: string, targets: string[]) {
    this.methodCalled('getRangesIgnoringCaseAndAccents', [searchText, targets]);
    // If ranges were explicitly set for the test, use them.
    if (this.ranges_) {
      return Promise.resolve({ranges: this.ranges_});
    }
    // Otherwise, simulate a basic case-insensitive substring search in the
    // mock. This is required because existing WebUI page tests (which mock this
    // proxy) rely on the search actually filtering the tab list when they type
    // in the search box.
    const query = searchText.toLowerCase();
    const ranges = targets.map(target => {
      const targetLower = target.toLowerCase();
      const matchRanges: TokenRange[] = [];
      if (query.length > 0) {
        let idx = targetLower.indexOf(query);
        while (idx !== -1) {
          matchRanges.push({start: idx, length: query.length});
          idx = targetLower.indexOf(query, idx + query.length);
        }
      }
      return matchRanges;
    });
    return Promise.resolve({ranges});
  }
}
