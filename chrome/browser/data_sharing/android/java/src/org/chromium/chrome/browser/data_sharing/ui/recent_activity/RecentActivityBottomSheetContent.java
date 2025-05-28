// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** The bottom sheet content that contains a list of recent activities for a collaboration. */
class RecentActivityBottomSheetContent implements BottomSheetContent {
    private final View mContentView;

    /**
     * Constructor.
     *
     * @param view The content view of the bottom sheet.
     */
    public RecentActivityBottomSheetContent(View view) {
        mContentView = view;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @NonNull
    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(
                R.string.data_sharing_recent_activity_bottom_sheet_content_description);
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.data_sharing_recent_activity_bottom_sheet_accessibility_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.data_sharing_recent_activity_bottom_sheet_accessibility_closed;
    }
}
