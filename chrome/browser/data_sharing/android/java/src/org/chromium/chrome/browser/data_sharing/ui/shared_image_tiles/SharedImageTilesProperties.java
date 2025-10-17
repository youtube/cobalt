// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of properties used by the SharedImageTiles component. */
@NullMarked
class SharedImageTilesProperties {
    // This will indicate the loading state of the shared_image_tiles view.
    public static final PropertyModel.WritableBooleanPropertyKey IS_LOADING =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<SharedImageTilesConfig>
            VIEW_CONFIG = new PropertyModel.WritableObjectPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey ICON_TILES =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey REMAINING_TILES =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        IS_LOADING, VIEW_CONFIG, ICON_TILES, REMAINING_TILES
    };
}
