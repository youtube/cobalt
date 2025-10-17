// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

/**
 * Class to define the {@link RecyclerView.ItemDecoration} for the instance lists on the instance
 * switcher and target selector dialogs.
 */
@NullMarked
public class DialogListItemDecoration extends RecyclerView.ItemDecoration {
    private final int mVerticalSpacing;

    /**
     * Creates a {@link DialogListItemDecoration}.
     *
     * @param verticalSpacing The spacing between items in pixels.
     */
    public DialogListItemDecoration(@Px int verticalSpacing) {
        mVerticalSpacing = verticalSpacing;
    }

    @Override
    public void getItemOffsets(
            @NonNull Rect outRect,
            @NonNull View view,
            @NonNull RecyclerView parent,
            @NonNull RecyclerView.State state) {
        // The last item does not use additional bottom spacing.
        assumeNonNull(parent.getAdapter());
        outRect.bottom =
                (parent.getChildAdapterPosition(view) != parent.getAdapter().getItemCount() - 1)
                        ? mVerticalSpacing
                        : 0;
    }

    @Override
    public void onDraw(
            @NonNull Canvas c, @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
        int itemCount = assumeNonNull(parent.getAdapter()).getItemCount();
        for (int index = 0; index < parent.getChildCount(); ++index) {
            View child = parent.getChildAt(index);
            int positionInAdapter = parent.getChildAdapterPosition(child);
            child.setBackground(
                    AppCompatResources.getDrawable(
                            parent.getContext(),
                            getBackgroundDrawable(positionInAdapter, itemCount)));
        }
    }

    /**
     * Returns the appropriate item background based on the position of the item in the list. The
     * first item has strongly rounded upper corners, the middle item has weakly rounded corners on
     * the top and the bottom, and the last item has strongly rounded bottom corners. When the list
     * contains a single item, it will use strongly rounded corners on the top and the bottom.
     *
     * @param position The zero-indexed position in the adapter.
     * @param itemCount The number of items in the adapter.
     * @return The resource ID of the item background.
     */
    private static @DrawableRes int getBackgroundDrawable(int position, int itemCount) {
        if (itemCount == 1) {
            return R.drawable.single_list_item_background;
        }

        if (position == 0) {
            return R.drawable.list_item_background_top;
        }

        if (position == itemCount - 1) {
            return R.drawable.list_item_background_bottom;
        }

        return R.drawable.list_item_background_middle;
    }
}
