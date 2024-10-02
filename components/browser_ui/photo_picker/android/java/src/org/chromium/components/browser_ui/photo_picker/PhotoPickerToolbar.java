// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.List;

/**
 * Handles toolbar functionality for the Photo Picker class.
 */
public class PhotoPickerToolbar extends SelectableListToolbar<PickerBitmap> {
    /**
     * A delegate that handles dialog actions.
     */
    public interface PhotoPickerToolbarDelegate {
        /**
         * Called when the back arrow is clicked in the toolbar.
         */
        void onNavigationBackCallback();
    }

    // A delegate to notify when the dialog should close.
    PhotoPickerToolbarDelegate mDelegate;

    public PhotoPickerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set the {@PhotoPickerToolbarDelegate} for this toolbar.
     */
    public void setDelegate(PhotoPickerToolbarDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Shows the Back arrow navigation button in the upper left corner.
     */
    public void showBackArrow() {
        setNavigationButton(NavigationButton.BACK);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        setNavigationContentDescription(R.string.close);
    }

    @Override
    public void onNavigationBack() {
        super.onNavigationBack();
        mDelegate.onNavigationBackCallback();
    }

    @Override
    public void initialize(SelectionDelegate<PickerBitmap> delegate, int titleResId,
            int normalGroupResId, int selectedGroupResId, boolean updateStatusBarColor) {
        super.initialize(
                delegate, titleResId, normalGroupResId, selectedGroupResId, updateStatusBarColor);

        showBackArrow();
    }

    @Override
    public void onSelectionStateChange(List<PickerBitmap> selectedItems) {
        super.onSelectionStateChange(selectedItems);

        int selectCount = selectedItems.size();
        Button done = (Button) findViewById(R.id.done);
        done.setEnabled(selectedItems.size() > 0);

        if (selectCount > 0) {
            ApiCompatibilityUtils.setTextAppearance(
                    done, R.style.TextAppearance_TextMedium_Secondary);
        } else {
            ApiCompatibilityUtils.setTextAppearance(
                    done, R.style.TextAppearance_TextMedium_Disabled);

            showBackArrow();
        }
    }
}
