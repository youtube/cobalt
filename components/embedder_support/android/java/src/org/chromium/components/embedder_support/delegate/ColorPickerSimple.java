// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ListView;

import org.chromium.components.embedder_support.delegate.ColorSuggestionListAdapter.OnColorSuggestionClickListener;

/**
 * Draws a grid of (predefined) colors and allows the user to choose one of
 * those colors.
 */
public class ColorPickerSimple extends ListView implements OnColorSuggestionClickListener {
    private OnColorChangedListener mOnColorChangedListener;

    private static final int[] DEFAULT_COLORS = {Color.RED, Color.CYAN, Color.BLUE, Color.GREEN,
            Color.MAGENTA, Color.YELLOW, Color.BLACK, Color.WHITE};

    private static final int[] DEFAULT_COLOR_LABEL_IDS = {R.string.color_picker_button_red,
            R.string.color_picker_button_cyan, R.string.color_picker_button_blue,
            R.string.color_picker_button_green, R.string.color_picker_button_magenta,
            R.string.color_picker_button_yellow, R.string.color_picker_button_black,
            R.string.color_picker_button_white};

    private ColorSuggestionListAdapter mAdapter;

    public ColorPickerSimple(Context context) {
        super(context);
    }

    public ColorPickerSimple(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ColorPickerSimple(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    /**
     * Initializes the listener and sets the adapter for the given list of suggestions. If the
     * suggestions is null a default set of colors will be used.
     *
     * @param suggestions The list of suggestions that should be displayed.
     * @param onColorChangedListener The listener that gets notified when the user touches
     *                               a color.
     */
    public void init(ColorSuggestion[] suggestions, OnColorChangedListener onColorChangedListener) {
        mOnColorChangedListener = onColorChangedListener;

        if (suggestions == null) {
            suggestions = new ColorSuggestion[DEFAULT_COLORS.length];
            for (int i = 0; i < suggestions.length; ++i) {
                suggestions[i] = new ColorSuggestion(
                        DEFAULT_COLORS[i], getContext().getString(DEFAULT_COLOR_LABEL_IDS[i]));
            }
        }

        mAdapter = new ColorSuggestionListAdapter(getContext(), suggestions);
        mAdapter.setOnColorSuggestionClickListener(this);
        setAdapter(mAdapter);
        setAccessibilityDelegate(new View.AccessibilityDelegate() {
            @Override
            public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfo info) {
                super.onInitializeAccessibilityNodeInfo(host, info);
                info.setCollectionInfo(AccessibilityNodeInfo.CollectionInfo.obtain(
                        DEFAULT_COLORS.length, 1, false));
            }
        });
    }

    @Override
    public void onColorSuggestionClick(ColorSuggestion suggestion) {
        mOnColorChangedListener.onColorChanged(suggestion.mColor);

        assert mAdapter != null;
        mAdapter.setSelectedColor(suggestion.mColor);
    }
}
