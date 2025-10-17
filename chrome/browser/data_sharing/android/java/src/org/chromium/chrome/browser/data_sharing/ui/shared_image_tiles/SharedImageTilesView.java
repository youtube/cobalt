// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

/** View logic for SharedImageTiles component. */
@NullMarked
public class SharedImageTilesView extends LinearLayout {
    private final Context mContext;
    private TextView mCountTileView;
    private LinearLayout mLastButtonTileView;

    /**
     * Constructor for a SharedImageTilesView.
     *
     * @param context The context this view will work in.
     * @param attrs The attribute set for this view.
     */
    public SharedImageTilesView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mCountTileView = findViewById(R.id.tiles_count);
        mLastButtonTileView = findViewById(R.id.last_tile_container);
    }

    void applyConfig(SharedImageTilesConfig config) {
        Resources res = mContext.getResources();
        Pair<Integer, Integer> borderAndTotalIconSize = config.getBorderAndTotalIconSizes(res);
        @Px int borderSize = borderAndTotalIconSize.first;
        @Px int iconTotalSize = borderAndTotalIconSize.second;
        @Px int textPadding = res.getDimensionPixelSize(config.textPaddingDp);

        // Style the icon tiles.
        for (int i = 0; i < getChildCount(); i++) {
            ViewGroup viewGroup = (ViewGroup) getChildAt(i);
            viewGroup.getLayoutParams().height = iconTotalSize;
            viewGroup.setMinimumWidth(iconTotalSize);
            GradientDrawable drawable = (GradientDrawable) viewGroup.getBackground();
            drawable.setColor(config.backgroundColor);
            drawable.setStroke(borderSize, config.borderColor);
        }

        // Style the number tile. Note that textColor must be set after textAppearance in order to
        // override the color of the text.
        mCountTileView.setTextAppearance(config.textStyle);
        mCountTileView.setTextColor(config.textColor);
        mCountTileView.setPadding(
                /* left= */ textPadding, /* top= */ 0, /* right= */ textPadding, /* bottom= */ 0);
    }

    void resetIconTiles(int count) {
        // Remove all child icon views and hide last view, which is the count tile or the button
        // tile.
        for (int i = 0; i < getChildCount() - 1; i++) {
            removeViewAt(i);
        }
        mCountTileView.setVisibility(View.GONE);
        mLastButtonTileView.setVisibility(View.GONE);

        // Add icon views.
        for (int i = 0; i < count; i++) {
            addView(
                    LayoutInflater.from(mContext)
                            .inflate(R.layout.shared_image_tiles_icon, this, false),
                    0);
        }
    }

    void showCountTile(int count) {
        mLastButtonTileView.setVisibility(View.VISIBLE);
        mCountTileView.setVisibility(View.VISIBLE);
        Resources res = mContext.getResources();
        String countText =
                res.getString(R.string.shared_image_tiles_count, Integer.toString(count));
        mCountTileView.setText(countText);
    }
}
