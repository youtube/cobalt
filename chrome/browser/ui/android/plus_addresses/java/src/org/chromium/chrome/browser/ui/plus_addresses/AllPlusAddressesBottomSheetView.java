// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.SearchView;
import android.widget.SearchView.OnQueryTextListener;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.LocalizationUtils;

/** Implements the bottom sheet content for the all plus addresses bottom sheet. */
class AllPlusAddressesBottomSheetView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private final RecyclerView mSheetItemListView;
    private final LinearLayout mContentView;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    assert mOnDismissed != null;
                    mOnDismissed.run();
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }

                @Override
                public void onSheetStateChanged(
                        @BottomSheetController.SheetState int newState,
                        @BottomSheetController.StateChangeReason int reason) {
                    super.onSheetStateChanged(newState, reason);
                    if (newState != BottomSheetController.SheetState.HIDDEN) return;
                    // This is a fail-safe for cases where onSheetClosed isn't triggered.
                    mOnDismissed.run();
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    private Runnable mOnDismissed;

    public AllPlusAddressesBottomSheetView(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContentView =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.all_plus_addresses_bottom_sheet, null);
        TextView title = mContentView.findViewById(R.id.sheet_title);
        TextView warning = mContentView.findViewById(R.id.sheet_warning);
        SearchView searchView = mContentView.findViewById(R.id.all_plus_addresses_search_view);
        title.setText(
                context.getString(
                        R.string.plus_address_all_plus_addresses_bottomsheet_title_android));
        warning.setText(
                context.getString(
                        R.string.plus_address_all_plus_addresses_bottomsheet_warning_android));
        searchView.setQueryHint(
                context.getString(
                        R.string.plus_address_all_plus_addresses_bottomsheet_query_hint_android));

        mSheetItemListView = mContentView.findViewById(R.id.sheet_item_list);
        mSheetItemListView.setLayoutManager(
                new LinearLayoutManager(
                        mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));

        // Apply RTL layout changes.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);
    }

    void setVisible(boolean isVisible) {
        if (isVisible) {
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (!mBottomSheetController.requestShowContent(this, true)) {
                assert (mOnDismissed != null);
                mOnDismissed.run();
                mBottomSheetController.removeObserver(mBottomSheetObserver);
            }
        } else {
            mBottomSheetController.hideContent(this, true);
        }
    }

    void setOnQueryChangedCallback(Callback<String> callback) {
        ((SearchView) mContentView.findViewById(R.id.all_plus_addresses_search_view))
                .setOnQueryTextListener(
                        new OnQueryTextListener() {
                            @Override
                            public boolean onQueryTextSubmit(String s) {
                                return false;
                            }

                            @Override
                            public boolean onQueryTextChange(String newString) {
                                callback.onResult(newString);
                                return true;
                            }
                        });
    }

    void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    void setOnDismissedCallback(Runnable onDismissed) {
        mOnDismissed = onDismissed;
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
        return mSheetItemListView.computeVerticalScrollOffset();
    }

    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public @NonNull String getSheetContentDescription(Context context) {
        return context.getString(
                R.string.plus_address_all_plus_addresses_bottomsheet_ax_description);
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.plus_address_all_plus_addresses_bottomsheet_is_opened_ax_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.plus_address_all_plus_addresses_bottomsheet_is_closed_ax_description;
    }
}
