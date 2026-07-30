// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the interactive search tile displayed in the AtMemoryBottomSheetView. */
@NullMarked
class AtMemoryBottomSheetSearchTileProperties {
    static final ReadableIntPropertyKey TILE_ICON = new ReadableIntPropertyKey();
    static final WritableObjectPropertyKey<@Nullable String> TILE_TITLE =
            new WritableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<@Nullable String> TILE_DETAILS =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<Runnable> ON_TILE_CLICKED =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {TILE_ICON, TILE_TITLE, TILE_DETAILS, ON_TILE_CLICKED};

    private AtMemoryBottomSheetSearchTileProperties() {}
}
