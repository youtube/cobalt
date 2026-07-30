// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FLYOUT_SUGGESTIONS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.IS_LOADING;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.NOTICE_OK_CLICK_LISTENER;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ON_QUERY_SUBMITTED_CALLBACK;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ON_QUERY_TEXT_CHANGED_CALLBACK;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SHOW_SUGGESTIONS_BACKGROUND;
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
        } else if (propertyKey == ON_QUERY_TEXT_CHANGED_CALLBACK) {
            view.setOnQueryTextChangedCallback(model.get(ON_QUERY_TEXT_CHANGED_CALLBACK));
        } else if (propertyKey == IS_LOADING) {
            view.setIsLoading(model.get(IS_LOADING));
        } else if (propertyKey == SHOW_SUGGESTIONS_BACKGROUND) {
            view.setShowSuggestionsBackground(model.get(SHOW_SUGGESTIONS_BACKGROUND));
        } else if (propertyKey == FLYOUT_SUGGESTIONS) {
            view.setFlyoutSuggestions(model.get(FLYOUT_SUGGESTIONS));
        } else if (propertyKey == IS_NOTICE_VISIBLE) {
            view.setNoticeVisible(model.get(IS_NOTICE_VISIBLE));
        } else if (propertyKey == NOTICE_OK_CLICK_LISTENER) {
            view.setNoticeOkButtonClickListener(model.get(NOTICE_OK_CLICK_LISTENER));
        }
    }
}
