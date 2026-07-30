// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class PrefsBrowserProxy implements PrefsBrowserProxy {
  getAllPrefs() {
    return chrome.settingsPrivate.getAllPrefs();
  }

  getPref(name: string) {
    return chrome.settingsPrivate.getPref(name);
  }

  setPref(name: string, value: unknown, pageId?: string) {
    return chrome.settingsPrivate.setPref(name, value, pageId);
  }

  get onPrefsChanged() {
    return chrome.settingsPrivate.onPrefsChanged;
  }

  static getInstance(): PrefsBrowserProxy {
    return instance || (instance = new PrefsBrowserProxy());
  }

  static setInstance(obj: PrefsBrowserProxy) {
    instance = obj;
  }
}

let instance: PrefsBrowserProxy|null = null;
