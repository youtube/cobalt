// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY;
import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.viewpager.widget.PagerAdapter;
import androidx.viewpager.widget.ViewPager;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;

/**
 * Displays the data provided by the {@link AccessorySheetViewBinder}.
 */
class AccessorySheetView extends LinearLayout {
    private ViewPager mViewPager;
    private FrameLayout mFrameLayout;
    private ImageView mTopShadow;
    private ImageView mKeyboardToggle;
    private TextView mSheetTitle;

    /**
     * Constructor for inflating from XML.
     */
    public AccessorySheetView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mViewPager = findViewById(R.id.keyboard_accessory_sheet);
        mTopShadow = findViewById(R.id.accessory_sheet_shadow);
        mFrameLayout = findViewById(R.id.keyboard_accessory_sheet_frame);
        if (ChromeFeatureList.isEnabled(AUTOFILL_KEYBOARD_ACCESSORY)) {
            mKeyboardToggle = findViewById(R.id.show_keyboard);
            mKeyboardToggle.setImageDrawable(
                    AppCompatResources.getDrawable(getContext(), R.drawable.ic_arrow_back_24dp));
            mSheetTitle = findViewById(R.id.sheet_title);
            findViewById(R.id.sheet_header).setVisibility(View.VISIBLE);
            findViewById(R.id.sheet_header_shadow).setVisibility(View.VISIBLE);
        }

        // Ensure that sub components of the sheet use the RTL direction:
        int layoutDirection = isLayoutRtl() ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR;
        mViewPager.setLayoutDirection(layoutDirection);
    }

    void setAdapter(PagerAdapter adapter) {
        mViewPager.setAdapter(adapter);
    }

    void addOnPageChangeListener(ViewPager.OnPageChangeListener pageChangeListener) {
        mViewPager.addOnPageChangeListener(pageChangeListener);
    }

    void setCurrentItem(int index) {
        mViewPager.setCurrentItem(index);
        // There is a small chance that the function is called too early and ViewPager will reset
        // the current item to 0. Correct that issue by repeating this call past other messages.
        mViewPager.post(() -> mViewPager.setCurrentItem(index));
    }

    ViewPager getViewPager() {
        return mViewPager;
    }

    void setTopShadowVisible(boolean isShadowVisible) {
        mTopShadow.setVisibility(isShadowVisible ? View.VISIBLE : View.INVISIBLE);
    }

    void setFrameHeight(int height) {
        ViewGroup.LayoutParams p = mFrameLayout.getLayoutParams();
        p.height = height;
        mFrameLayout.setLayoutParams(p);
    }

    void setTitle(String title) {
        if (!ChromeFeatureList.isEnabled(AUTOFILL_KEYBOARD_ACCESSORY)) return;
        assert mSheetTitle != null : "setTitle called before view initialized";
        mSheetTitle.setText(title);
    }

    void setShowKeyboardCallback(Runnable runnable) {
        if (!ChromeFeatureList.isEnabled(AUTOFILL_KEYBOARD_ACCESSORY)) return;
        assert mKeyboardToggle != null : "setShowKeyboardCallback called before view initialized";
        mKeyboardToggle.setOnClickListener(runnable == null ? null : view -> runnable.run());
    }
}
