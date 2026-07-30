// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.ON_TILE_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.TILE_DETAILS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.TILE_ICON;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.TILE_TITLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds properties to an {@link AtMemoryBottomSheetSearchTileView}. */
@NullMarked
public class AtMemoryBottomSheetSearchTileViewBinder {
    public static void bind(
            PropertyModel model, AtMemoryBottomSheetSearchTileView view, PropertyKey propertyKey) {
        if (propertyKey == TILE_ICON) {
            view.setIcon(model.get(TILE_ICON));
        } else if (propertyKey == TILE_TITLE) {
            view.setTitle(model.get(TILE_TITLE));
        } else if (propertyKey == TILE_DETAILS) {
            view.setDetails(model.get(TILE_DETAILS));
        } else if (propertyKey == ON_TILE_CLICKED) {
            view.setClickListener(model.get(ON_TILE_CLICKED));
        } else {
            assert false : "Unhandled property: " + propertyKey;
        }
    }
}
