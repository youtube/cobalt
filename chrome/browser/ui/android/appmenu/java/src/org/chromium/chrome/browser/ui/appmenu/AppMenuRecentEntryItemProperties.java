// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Arrays;

/** {@link PropertyKey} list for app menu recent entry items. */
@NullMarked
public class AppMenuRecentEntryItemProperties {
    /** The recently closed entry associated with this item. */
    public static final WritableObjectPropertyKey<Object> RECENT_ENTRY =
            new WritableObjectPropertyKey<>("RECENT_ENTRY");

    /** The instance ID of the window associated with this item if any. */
    public static final WritableIntPropertyKey WINDOW_ID = new WritableIntPropertyKey("WINDOW_ID");

    /** The session tag associated with this item if it is a foreign session. */
    public static final WritableObjectPropertyKey<String> FOREIGN_SESSION_TAG =
            new WritableObjectPropertyKey<>("FOREIGN_SESSION_TAG");

    /** The foreign session tab associated with this item if it is a foreign session. */
    public static final WritableObjectPropertyKey<Object> FOREIGN_SESSION_TAB =
            new WritableObjectPropertyKey<>("FOREIGN_SESSION_TAB");

    public static final PropertyKey[] RECENT_ENTRY_KEYS =
            new PropertyKey[] {RECENT_ENTRY, WINDOW_ID, FOREIGN_SESSION_TAG, FOREIGN_SESSION_TAB};

    public static final PropertyKey[] ALL_KEYS =
            Arrays.copyOf(
                    AppMenuItemProperties.ALL_KEYS,
                    AppMenuItemProperties.ALL_KEYS.length + RECENT_ENTRY_KEYS.length);

    static {
        for (int i = 0; i < RECENT_ENTRY_KEYS.length; i++) {
            ALL_KEYS[ALL_KEYS.length - i - 1] = RECENT_ENTRY_KEYS[i];
        }
    }
}
