// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Properties defined here reflect the visible state of the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetProperties {
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey IS_LOADING = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey SHOW_SUGGESTIONS_BACKGROUND =
            new WritableBooleanPropertyKey();
    static final ReadableObjectPropertyKey<Callback<String>> ON_QUERY_TEXT_CHANGED_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<Callback<String>> ON_QUERY_SUBMITTED_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<List<AutofillSuggestion>> FLYOUT_SUGGESTIONS =
            new WritableObjectPropertyKey<>();

    static final WritableBooleanPropertyKey IS_NOTICE_VISIBLE = new WritableBooleanPropertyKey();
    static final ReadableObjectPropertyKey<Runnable> NOTICE_OK_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE,
        ON_QUERY_SUBMITTED_CALLBACK,
        IS_LOADING,
        SHOW_SUGGESTIONS_BACKGROUND,
        ON_QUERY_TEXT_CHANGED_CALLBACK,
        FLYOUT_SUGGESTIONS,
        IS_NOTICE_VISIBLE,
        NOTICE_OK_CLICK_LISTENER
    };

    private AtMemoryBottomSheetProperties() {}
}
