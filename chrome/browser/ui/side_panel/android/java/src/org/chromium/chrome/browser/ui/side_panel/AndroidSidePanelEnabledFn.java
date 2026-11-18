// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Checks all conditions for whether to enable the Android Side Panel.
 *
 * <p>This is the sole authority that decides whether to enable Android Side Panel.
 */
@NullMarked
public final class AndroidSidePanelEnabledFn {
    private AndroidSidePanelEnabledFn() {}

    /** Returns true if the Android Side Panel should be enabled. */
    @CalledByNative
    public static boolean isEnabled() {
        // TODO(crbug.com/497862593): See if the cached flag can be instantiated inside this class.
        return ChromeFeatureList.sEnableAndroidSidePanel.isEnabled();
    }

    /**
     * Returns true if the pure-Java dev feature should be enabled.
     *
     * <p>The pure-Java dev feature was created before the C++ side panel infrastructure existed on
     * Android, and it's used for testing side panel UI (1) during development and (2) in Java
     * integration tests that don't care about the side panel state management in C++.
     */
    public static boolean isPureJavaDevFeatureEnabled() {
        if (!isDevFeatureEnabled()) {
            return false;
        }

        String devFeatureScope =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL_DEV_FEATURE, "scope");
        return !"tab".equals(devFeatureScope);
    }

    /**
     * Returns true if the tab-scoped dev feature should be enabled.
     *
     * <p>The tab-scoped dev feature is an E2E feature that uses both the Java UI code and the C++
     * side panel state management.
     */
    public static boolean isTabScopedDevFeatureEnabled() {
        if (!isDevFeatureEnabled()) {
            return false;
        }

        String devFeatureScope =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL_DEV_FEATURE, "scope");
        return "tab".equals(devFeatureScope);
    }

    private static boolean isDevFeatureEnabled() {
        return ChromeFeatureList.sEnableAndroidSidePanel.isEnabled()
                && ChromeFeatureList.sEnableAndroidSidePanelDevFeature.isEnabled();
    }
}
