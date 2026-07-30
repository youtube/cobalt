// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;

/** Collection of utility methods for native interaction with {@link Tab} objects. */
@NullMarked
public class TabNativeUtils {
    // Do not instantiate.
    private TabNativeUtils() {}

    /**
     * Returns the native pointer representing the native side of the given {@link Tab} object.
     *
     * @param tab The Tab to get the native pointer for.
     * @return The native pointer, or 0 if the tab has no native counterpart.
     */
    public static long getNativePtr(Tab tab) {
        if (tab instanceof TabInternal) {
            return ((TabInternal) tab).getNativePtr();
        }
        return 0;
    }
}
