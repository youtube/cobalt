// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashscreenObserver;

/**
 * This class records cold start WebApk splashscreen metrics starting from the launch of the WebAPK
 * shell.
 */
public class WebApkSplashscreenMetrics implements SplashscreenObserver {
    private final long mShellApkLaunchTimestamp;
    private final long mNewStyleSplashShownTimestamp;

    public WebApkSplashscreenMetrics(
            long shellApkLaunchTimestamp, long newStyleSplashShownTimestamp) {
        mShellApkLaunchTimestamp = shellApkLaunchTimestamp;
        mNewStyleSplashShownTimestamp = newStyleSplashShownTimestamp;
    }

    @Override
    public void onTranslucencyRemoved() {}

    @Override
    public void onSplashscreenHidden(long startTimestamp, long endTimestamp) {
        if (!UmaUtils.hasComeToForegroundWithNative() || UmaUtils.hasComeToBackgroundWithNative()
                || mShellApkLaunchTimestamp == -1) {
            return;
        }

        // commit shown histograms here because native may not be loaded when the
        // splashscreen is shown.
        WebApkUmaRecorder.recordShellApkLaunchToSplashVisible(
                startTimestamp - mShellApkLaunchTimestamp);

        if (mNewStyleSplashShownTimestamp != -1) {
            WebApkUmaRecorder.recordNewStyleShellApkLaunchToSplashVisible(
                    mNewStyleSplashShownTimestamp - mShellApkLaunchTimestamp);
        }
    }
}
