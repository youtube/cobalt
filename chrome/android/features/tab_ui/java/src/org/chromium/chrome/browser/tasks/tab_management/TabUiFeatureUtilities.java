// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Build;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.Locale;
import java.util.Set;

/** A class to handle the state of flags for tab_management. */
public class TabUiFeatureUtilities {
    private static final Set<String> TAB_TEARING_OEM_ALLOWLIST = Set.of("samsung");

    // Cached and fixed values.
    private static boolean sTabListEditorLongPressEntryEnabled;

    /** Set whether the longpress entry for TabListEditor is enabled. Currently only in tests. */
    public static void setTabListEditorLongPressEntryEnabledForTesting(boolean enabled) {
        var oldValue = sTabListEditorLongPressEntryEnabled;
        sTabListEditorLongPressEntryEnabled = enabled;
        ResettersForTesting.register(() -> sTabListEditorLongPressEntryEnabled = oldValue);
    }

    /** Whether the longpress entry for TabListEditor is enabled. Currently only in tests. */
    public static boolean isTabListEditorLongPressEntryEnabled() {
        return sTabListEditorLongPressEntryEnabled;
    }

    /** Returns whether the Grid Tab Switcher UI should use list mode. */
    public static boolean shouldUseListMode() {
        if (ChromeFeatureList.sDisableListTabSwitcher.isEnabled()) {
            return false;
        }
        // Low-end forces list mode.
        return SysUtils.isLowEndDevice() || ChromeFeatureList.sForceListTabSwitcher.isEnabled();
    }

    /**
     * @return whether tab drag as window is enabled.
     */
    public static boolean isTabDragAsWindowEnabled() {
        return ChromeFeatureList.sTabDragDropAsWindowAndroid.isEnabled();
    }

    /** Returns if the tab group pane should be displayed in the hub. */
    public static boolean isTabGroupPaneEnabled() {
        return ChromeFeatureList.sTabGroupPaneAndroid.isEnabled();
    }

    /** Returns whether drag drop from tab strip to create new instance is enabled. */
    public static boolean isTabDragToCreateInstanceSupported() {
        // TODO(crbug/328511660): Add OS version check once available.
        return doesOEMSupportDragToCreateInstance() || !isTabDragAsWindowEnabled();
    }

    /** Returns whether device OEM is allow-listed for tab tearing */
    public static boolean doesOEMSupportDragToCreateInstance() {
        return TAB_TEARING_OEM_ALLOWLIST.contains(Build.MANUFACTURER.toLowerCase(Locale.US));
    }
}
