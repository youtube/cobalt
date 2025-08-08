// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.tab_ui.R;

/** Shared utils for {@code Tab<Type>ViewBinder} such as List, Grid, Strip, etc. * */
class TabCardViewBinderUtils {
    /**
     * Returns the checkmark level for selection mode.
     *
     * @param res The resources object.
     * @param isSelected Whether the item is selected.
     */
    static int getCheckmarkLevel(@NonNull Resources res, boolean isSelected) {
        return isSelected
                ? res.getInteger(R.integer.list_item_level_selected)
                : res.getInteger(R.integer.list_item_level_default);
    }

    /**
     * Detaches any views from the container view and sets its visibility to {@link View.GONE}.
     *
     * @param containerView The container view to update.
     */
    static void detachTabGroupColorView(@NonNull FrameLayout containerView) {
        updateTabGroupColorView(containerView, /* viewProvider= */ null);
    }

    /**
     * Attaches the view from the provider to the container view or removes all views of the
     * container if the view provider will not provide a view.
     *
     * @param containerView The container view to update.
     * @param viewProvider The provider of the tab group color view to attach. Passing null will
     *     detach all views from the container.
     */
    static void updateTabGroupColorView(
            @NonNull FrameLayout containerView, @Nullable TabGroupColorViewProvider viewProvider) {
        if (viewProvider == null) {
            containerView.setVisibility(View.GONE);
            containerView.removeAllViews();
        } else {
            containerView.setVisibility(View.VISIBLE);
            View colorView = viewProvider.getLazyView();
            ViewGroup parentView = (ViewGroup) colorView.getParent();
            if (parentView == containerView) return;

            // If the parent view is non-null and is the the expected container view we need to
            // remove it from the old view and attach it to the new view. In-the-wild there appear
            // to be codepaths that rebind all the RV items to new parent view without recycling,
            // but since the color view is not recreated we end up hitting an
            // IllegalStateException.
            if (parentView != null) {
                parentView.removeView(colorView);
                parentView.setVisibility(View.GONE);
            }

            var layoutParams =
                    new FrameLayout.LayoutParams(
                            FrameLayout.LayoutParams.WRAP_CONTENT,
                            FrameLayout.LayoutParams.WRAP_CONTENT);
            layoutParams.gravity = Gravity.CENTER;
            containerView.addView(colorView, layoutParams);
        }
    }

    private TabCardViewBinderUtils() {}
}
