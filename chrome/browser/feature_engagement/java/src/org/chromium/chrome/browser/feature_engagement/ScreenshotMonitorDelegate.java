// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_engagement;

/**
 * This class serves as a callback from ScreenshotMonitor.
 */
public interface ScreenshotMonitorDelegate {
    void onScreenshotTaken();
}