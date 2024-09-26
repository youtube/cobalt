// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bridge between Java and native SafeBrowsing code to launch the Safe Browsing settings page.
 */
public class SafeBrowsingSettingsLauncher {
    private SafeBrowsingSettingsLauncher() {}

    @CalledByNative
    private static void showSafeBrowsingSettings(
            WebContents webContents, @SettingsAccessPoint int accessPoint) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        Context currentContext = window.getContext().get();
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(currentContext, SafeBrowsingSettingsFragment.class,
                SafeBrowsingSettingsFragment.createArguments(accessPoint));
    }
}
