// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

interface ISafeModeService {
    // Sets the WebView SafeMode config in the WebView provider's SafeModeService. This config will
    // apply to all WebView-based apps.
    //
    // `actions` is a collection of Strings, each of which represents a SafeModeAction. Passing an
    // empty list will disable SafeMode.
    //
    // Only certain trusted (Google-managed) services are permitted to call this API, as determined
    // by an allowlist. If the caller's UID does not match a trusted package (determined by both
    // package name and signing certificate, looked up via the system PackageManager),
    // SafeModeService will throw a SecurityException.
    void setSafeMode(in List<String> actions);
}
