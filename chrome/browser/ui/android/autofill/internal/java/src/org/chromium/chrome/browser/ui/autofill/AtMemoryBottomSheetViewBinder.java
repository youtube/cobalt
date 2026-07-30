// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ON_QUERY_SUBMITTED_CALLBACK;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds properties for the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetViewBinder {
    static void bind(PropertyModel model, AtMemoryBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == VISIBLE) {
            if (model.get(VISIBLE)) {
                view.clearSearchText();
                view.focusSearchArea();
            }
        } else if (propertyKey == ON_QUERY_SUBMITTED_CALLBACK) {
            view.setOnQuerySubmittedCallback(model.get(ON_QUERY_SUBMITTED_CALLBACK));
        }
    }
}
