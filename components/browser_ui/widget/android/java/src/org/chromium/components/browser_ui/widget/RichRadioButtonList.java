// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * A composite widget that displays a list of {@link RichRadioButton}s in a single vertical column,
 * managing its own RecyclerView and Adapter.
 */
@NullMarked
public class RichRadioButtonList extends FrameLayout {

    private @Nullable RichRadioButtonAdapter mAdapter;

    private @Nullable List<RichRadioButtonData> mCurrentOptions;
    private final RecyclerView mRecyclerView;
    private boolean mInitialized;

    public RichRadioButtonList(Context context) {
        this(context, null);
    }

    public RichRadioButtonList(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        inflate(context, R.layout.rich_radio_button_list, this);
        mRecyclerView = findViewById(R.id.rich_radio_button_list_recycler_view);
        mRecyclerView.setItemAnimator(null);
    }

    /**
     * Initializes the options to display and configures the list's layout. This method is intended
     * to be called only once after the component is created.
     *
     * @param options The list of RichRadioButtonData items to display.
     * @param listener The listener for selection changes.
     */
    @Initializer
    public void initialize(
            List<RichRadioButtonData> options,
            RichRadioButtonAdapter.OnItemSelectedListener listener) {
        if (mInitialized) {
            throw new IllegalStateException("RichRadioButtonList can only be initialized once.");
        }
        mInitialized = true;

        mCurrentOptions = options;

        int verticalSpacingPx =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.rich_radio_button_list_vertical_spacing);
        RecyclerView.LayoutManager layoutManager =
                new LinearLayoutManager(getContext(), LinearLayoutManager.VERTICAL, false);
        clearItemDecorations();
        mRecyclerView.addItemDecoration(new SimpleItemDecoration(verticalSpacingPx));
        mRecyclerView.setLayoutManager(layoutManager);

        mAdapter = new RichRadioButtonAdapter(mCurrentOptions, listener);
        mRecyclerView.setAdapter(mAdapter);
    }

    /**
     * Sets the initially selected item.
     *
     * @param itemId The ID of the item to select.
     */
    public void setSelectedItem(String itemId) {
        if (!mInitialized) {
            return;
        }
        if (mAdapter != null) {
            mAdapter.setSelectedItem(itemId);
        }
    }

    /** Clears all existing ItemDecorations from the RecyclerView. */
    private void clearItemDecorations() {
        List<RecyclerView.ItemDecoration> decorationsToRemove = new ArrayList<>();
        for (int i = 0; i < mRecyclerView.getItemDecorationCount(); i++) {
            decorationsToRemove.add(mRecyclerView.getItemDecorationAt(i));
        }

        for (RecyclerView.ItemDecoration decoration : decorationsToRemove) {
            mRecyclerView.removeItemDecoration(decoration);
        }
    }

    /** ItemDecoration for spacing between items. */
    private static class SimpleItemDecoration extends RecyclerView.ItemDecoration {
        private final int mVerticalSpaceHeightPx;

        public SimpleItemDecoration(int verticalSpaceHeightPx) {
            mVerticalSpaceHeightPx = verticalSpaceHeightPx;
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            super.getItemOffsets(outRect, view, parent, state);

            int position = parent.getChildAdapterPosition(view);
            if (position == RecyclerView.NO_POSITION || parent.getAdapter() == null) return;

            int itemCount = parent.getAdapter().getItemCount();

            if (position < itemCount - 1) {
                outRect.bottom = mVerticalSpaceHeightPx;
            }
        }
    }

    RecyclerView getRecyclerViewForTesting() {
        return mRecyclerView;
    }

    @Nullable
    RichRadioButtonAdapter getAdapterForTesting() {
        return mAdapter;
    }
}
