// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Modes of showing the list of tabs. */
@IntDef({TabListMode.GRID, TabListMode.BOTTOM_STRIP, TabListMode.VERTICAL, TabListMode.NUM_ENTRIES})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface TabListMode {
    int GRID = 0;
    int BOTTOM_STRIP = 1;
    // int CAROUSEL_DEPRECATED = 2;
    // int LIST_DEPRECATED = 3;
    int VERTICAL = 4;
    int NUM_ENTRIES = 5;
}
