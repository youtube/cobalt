// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.appcompat.widget.SearchView;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.base.Callback;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** View wrapper for the @memory bottom sheet. */
@NullMarked
public class AtMemoryBottomSheetView {
    private final View mContentView;
    private final RecyclerView mRecyclerView;
    private final SearchView mSearchView;

    public AtMemoryBottomSheetView(Context context) {
        mContentView = LayoutInflater.from(context).inflate(R.layout.at_memory_bottom_sheet, null);

        mRecyclerView = mContentView.findViewById(R.id.suggestions_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));
        mRecyclerView.addItemDecoration(new AtMemoryDividerItemDecoration(context));

        mSearchView = mContentView.findViewById(R.id.search_query_input);
    }

    public View getContentView() {
        return mContentView;
    }

    public void setRecyclerViewAdapter(Adapter adapter) {
        mRecyclerView.setAdapter(adapter);
    }

    public void focusSearchArea() {
        // TODO(crbug.com/512802813): Fix cursor not blinking on subsequent openings of the bottom
        // sheet.
        mSearchView.requestFocus();
        // SearchView is a wrapper layout. We must find and pass its focused child (the internal
        // edit text) to show the keyboard.
        View focusedChild = mSearchView.findFocus();
        KeyboardUtils.showKeyboard(focusedChild != null ? focusedChild : mSearchView);
    }

    public void clearSearchText() {
        mSearchView.setQuery("", /* submit= */ false);
    }

    public void setOnQuerySubmittedCallback(Callback<String> callback) {
        mSearchView.setOnQueryTextListener(
                new SearchView.OnQueryTextListener() {
                    @Override
                    public boolean onQueryTextSubmit(String query) {
                        callback.onResult(query);
                        return true;
                    }

                    @Override
                    public boolean onQueryTextChange(String newText) {
                        return false;
                    }
                });
    }

    /** Draws a divider line below each item in the list except for the last item. */
    private static class AtMemoryDividerItemDecoration extends RecyclerView.ItemDecoration {
        private final Drawable mDivider;
        private final int mDividerHeight;

        public AtMemoryDividerItemDecoration(Context context) {
            mDivider = new ColorDrawable(SemanticColorUtils.getDefaultBgColor(context));
            mDividerHeight =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.at_memory_bottom_sheet_divider_height);
        }

        @Override
        public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
            Adapter adapter = parent.getAdapter();
            if (adapter == null) return;

            int left = parent.getPaddingLeft();
            int right = parent.getWidth() - parent.getPaddingRight();
            int childCount = parent.getChildCount();

            for (int i = 0; i < childCount; i++) {
                View child = parent.getChildAt(i);
                int position = parent.getChildAdapterPosition(child);
                if (position == RecyclerView.NO_POSITION
                        || position == adapter.getItemCount() - 1) {
                    continue;
                }

                RecyclerView.LayoutParams params =
                        (RecyclerView.LayoutParams) child.getLayoutParams();
                int top = child.getBottom() + params.bottomMargin;
                int bottom = top + mDividerHeight;

                mDivider.setBounds(left, top, right, bottom);
                mDivider.draw(c);
            }
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            Adapter adapter = parent.getAdapter();
            if (adapter == null) return;

            int position = parent.getChildAdapterPosition(view);
            if (position == RecyclerView.NO_POSITION || position == adapter.getItemCount() - 1) {
                outRect.set(0, 0, 0, 0);
            } else {
                outRect.set(0, 0, 0, mDividerHeight);
            }
        }
    }
}
