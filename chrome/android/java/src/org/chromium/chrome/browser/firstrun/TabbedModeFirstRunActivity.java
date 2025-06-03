// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.AnyRes;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.R;

/**
 * FirstRunActivity variant that fills the whole screen, but displays the content
 * in a dialog-like layout.
 */
public class TabbedModeFirstRunActivity extends FirstRunActivity {
    @Override
    protected View createContentView() {
        return wrapInDialogLayout(super.createContentView());
    }

    /**
     * Wraps contentView into layout that resembles DialogWhenLarge. The layout centers
     * the content and dims the background to simulate a modal dialog.
     */
    private View wrapInDialogLayout(View contentView) {
        ContentLayout contentLayout = new ContentLayout(this);
        contentLayout.addView(contentView);

        contentLayout.setBackgroundResource(R.drawable.bg_white_dialog);
        contentLayout.setClipToOutline(true);

        // We need an outer layout for two things:
        //   * centering the content
        //   * dimming the background
        FrameLayout outerLayout = new FrameLayout(this);
        outerLayout.addView(contentLayout,
                new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));
        outerLayout.setBackgroundResource(R.color.modal_dialog_scrim_color);
        return outerLayout;
    }

    /**
     * Layout that sizes itself according to DialogWhenLarge constraints.
     */
    private static class ContentLayout extends FrameLayout {
        private TypedValue mFixedWidthMajor = new TypedValue();
        private TypedValue mFixedWidthMinor = new TypedValue();
        private TypedValue mFixedHeightMajor = new TypedValue();
        private TypedValue mFixedHeightMinor = new TypedValue();

        public ContentLayout(Context context) {
            super(context);
            fetchConstraints();
        }

        /**
         * Wrapper around Resources.getValue() that translates Resources.NotFoundException
         * into false return value. Otherwise the function returns true.
         */
        private boolean safeGetResourceValue(@AnyRes int id, TypedValue value) {
            try {
                getContext().getResources().getValue(id, value, true);
                return true;
            } catch (Resources.NotFoundException e) {
                return false;
            }
        }

        private void fetchConstraints() {
            // Fetch size constraints. These are copies of corresponding abc_* AppCompat values,
            // because abc_* values are private, and even though corresponding theme attributes
            // are public in AppCompat (e.g. windowFixedWidthMinor), there is no guarantee that
            // AppCompat.DialogWhenLarge is actually defined by AppCompat and not based on
            // system DialogWhenLarge theme.
            // Note that we don't care about the return values, because onMeasure() handles null
            // constraints (and they will be null when the device is not considered "large").
            if (BuildInfo.getInstance().isAutomotive) {
                safeGetResourceValue(R.dimen.dialog_fixed_width_minor_automotive, mFixedWidthMinor);
                safeGetResourceValue(R.dimen.dialog_fixed_width_major_automotive, mFixedWidthMajor);
                safeGetResourceValue(
                        R.dimen.dialog_fixed_height_minor_automotive, mFixedHeightMinor);
                safeGetResourceValue(
                        R.dimen.dialog_fixed_height_major_automotive, mFixedHeightMajor);
            } else {
                safeGetResourceValue(R.dimen.dialog_fixed_width_minor, mFixedWidthMinor);
                safeGetResourceValue(R.dimen.dialog_fixed_width_major, mFixedWidthMajor);
                safeGetResourceValue(R.dimen.dialog_fixed_height_minor, mFixedHeightMinor);
                safeGetResourceValue(R.dimen.dialog_fixed_height_major, mFixedHeightMajor);
            }
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            final DisplayMetrics metrics = getContext().getResources().getDisplayMetrics();
            final boolean isPortrait = metrics.widthPixels < metrics.heightPixels;

            // Constraint handling is adapted from
            // sdk/sources/android-25/android/support/v7/widget/ContentFrameLayout.java.
            final int widthMode = MeasureSpec.getMode(widthMeasureSpec);
            assert widthMode == MeasureSpec.AT_MOST;
            {
                final TypedValue tvw = isPortrait ? mFixedWidthMinor : mFixedWidthMajor;
                int widthSize = MeasureSpec.getSize(widthMeasureSpec);
                if (tvw.type != TypedValue.TYPE_NULL) {
                    assert tvw.type == TypedValue.TYPE_FRACTION;
                    int width = (int) tvw.getFraction(metrics.widthPixels, metrics.widthPixels);
                    widthSize = Math.min(width, widthSize);
                }
                // This either sets the width calculated from the constraints, or simply
                // takes all of the available space (as if MATCH_PARENT was specified).
                // The behavior is similar to how DialogWhenLarge windows are sized - they
                // are either sized by the constraints, or are full screen, but are never
                // sized based on the content size.
                widthMeasureSpec = MeasureSpec.makeMeasureSpec(widthSize, MeasureSpec.EXACTLY);
            }

            // This is similar to the above block that calculates width.
            final int heightMode = MeasureSpec.getMode(heightMeasureSpec);
            assert heightMode == MeasureSpec.AT_MOST;
            {
                final TypedValue tvh = isPortrait ? mFixedHeightMajor : mFixedHeightMinor;
                int heightSize = MeasureSpec.getSize(heightMeasureSpec);
                if (tvh.type != TypedValue.TYPE_NULL) {
                    assert tvh.type == TypedValue.TYPE_FRACTION;

                    // Calculate height from the View's measureSpec to account for larger status
                    // bar and back toolbar on automotive devices.
                    int referenceHeight = BuildInfo.getInstance().isAutomotive
                            ? heightSize
                            : metrics.heightPixels;

                    int height = (int) tvh.getFraction(referenceHeight, referenceHeight);
                    heightSize = Math.min(height, heightSize);
                }
                heightMeasureSpec = MeasureSpec.makeMeasureSpec(heightSize, MeasureSpec.EXACTLY);
            }

            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    public boolean canUseLandscapeLayout() {
        // TabbedModeFirstRunActivity shows FRE in a dialog that always has the portrait
        // orientation, so never use the landscape layout with that activity.
        return false;
    }
}
