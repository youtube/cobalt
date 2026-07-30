// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;

import java.util.List;

/** View wrapper for the @memory bottom sheet. */
@NullMarked
public class AtMemoryBottomSheetView {
    private final View mContentView;
    private final AtMemoryHomeView mHomeView;
    private final AtMemoryFlyoutView mFlyoutView;

    public AtMemoryBottomSheetView(Context context) {
        mContentView = LayoutInflater.from(context).inflate(R.layout.at_memory_bottom_sheet, null);

        mHomeView = mContentView.findViewById(R.id.at_memory_home_screen);
        mFlyoutView = mContentView.findViewById(R.id.at_memory_flyout_screen);
    }

    public View getContentView() {
        return mContentView;
    }

    public void setRecyclerViewAdapter(Adapter adapter) {
        mHomeView.setRecyclerViewAdapter(adapter);
    }

    public void focusSearchArea() {
        mHomeView.focusSearchArea();
    }

    public void clearSearchText() {
        mHomeView.clearSearchText();
    }

    public void setOnQuerySubmittedCallback(Callback<String> callback) {
        mHomeView.setOnQuerySubmittedCallback(callback);
    }

    public void setOnQueryTextChangedCallback(Callback<String> callback) {
        mHomeView.setOnQueryTextChangedCallback(callback);
    }

    public void setIsLoading(boolean isLoading) {
        mHomeView.setIsLoading(isLoading);
    }

    public void setShowSuggestionsBackground(boolean showBackground) {
        mHomeView.setShowSuggestionsBackground(showBackground);
    }

    public void hideKeyboardAndClearFocus() {
        mHomeView.hideKeyboardAndClearFocus();
    }

    public void setFlyoutSuggestions(List<AutofillSuggestion> suggestions) {
        mFlyoutView.setSuggestions(suggestions);
    }

    public void setNoticeVisible(boolean visible) {
        mHomeView.setNoticeVisible(visible);
    }

    public void setNoticeOkButtonClickListener(Runnable onClick) {
        mHomeView.setNoticeOkClickListener(onClick);
    }
}
