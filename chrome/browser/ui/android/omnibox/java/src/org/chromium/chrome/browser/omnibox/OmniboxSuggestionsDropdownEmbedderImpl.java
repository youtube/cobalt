// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.WindowInsets;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.display.DisplayUtil;

/**
 * Implementation of {@link OmniboxSuggestionsDropdownEmbedder} that positions it using an "anchor"
 * and "horizontal alignment" view.
 * */
class OmniboxSuggestionsDropdownEmbedderImpl implements OmniboxSuggestionsDropdownEmbedder,
                                                        OnLayoutChangeListener,
                                                        OnGlobalLayoutListener, ComponentCallbacks {
    private final ObservableSupplierImpl<OmniboxAlignment> mOmniboxAlignmentSupplier =
            new ObservableSupplierImpl<>();
    private final @NonNull WindowAndroid mWindowAndroid;
    private final @NonNull WindowDelegate mWindowDelegate;
    private final @NonNull View mAnchorView;
    private final @NonNull View mHorizontalAlignmentView;
    private final @NonNull Context mContext;
    // Reusable int array to pass to positioning methods that operate on a two element int array.
    // Keeping it as a member lets us avoid allocating a temp array every time.
    private final int[] mPositionArray = new int[2];
    private int mVerticalOffsetInWindow;
    private int mWindowWidthDp;
    private int mWindowHeightDp;
    private WindowInsetsCompat mWindowInsetsCompat;
    private DeferredIMEWindowInsetApplicationCallback mDeferredIMEWindowInsetApplicationCallback;

    /**
     *
     * @param windowAndroid Window object in which the dropdown will be displayed.
     * @param windowDelegate Delegate object for performing window operations.
     * @param anchorView View to which the dropdown should be "anchored" i.e. vertically positioned
     *         next to and matching the width of. This must be a descendant of the top-level content
     *         (android.R.id.content) view.
     * @param horizontalAlignmentView View to which the dropdown should be horizontally aligned when
     *         its width is smaller than the anchor view. This must be a descendant of the anchor
     *         view.
     */
    OmniboxSuggestionsDropdownEmbedderImpl(@NonNull WindowAndroid windowAndroid,
            @NonNull WindowDelegate windowDelegate, @NonNull View anchorView,
            @NonNull View horizontalAlignmentView) {
        mWindowAndroid = windowAndroid;
        mWindowDelegate = windowDelegate;
        mAnchorView = anchorView;
        mHorizontalAlignmentView = horizontalAlignmentView;
        mContext = mAnchorView.getContext();
        mContext.registerComponentCallbacks(this);
        Configuration configuration = mContext.getResources().getConfiguration();
        mWindowWidthDp = configuration.smallestScreenWidthDp;
        mWindowHeightDp = configuration.screenHeightDp;
        recalculateOmniboxAlignment();
    }

    @Override
    public OmniboxAlignment addAlignmentObserver(Callback<OmniboxAlignment> obs) {
        return mOmniboxAlignmentSupplier.addObserver(obs);
    }

    @Override
    public void removeAlignmentObserver(Callback<OmniboxAlignment> obs) {
        mOmniboxAlignmentSupplier.removeObserver(obs);
    }

    @Nullable
    @Override
    public OmniboxAlignment getCurrentAlignment() {
        return mOmniboxAlignmentSupplier.get();
    }

    @Override
    public boolean isTablet() {
        if (OmniboxFeatures.shouldAdaptToNarrowTabletWindows()) {
            return mWindowWidthDp >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP
                    && DeviceFormFactor.isWindowOnTablet(mWindowAndroid);
        } else {
            return DeviceFormFactor.isWindowOnTablet(mWindowAndroid);
        }
    }

    @Override
    public void onAttachedToWindow() {
        mAnchorView.addOnLayoutChangeListener(this);
        mHorizontalAlignmentView.addOnLayoutChangeListener(this);
        mAnchorView.getViewTreeObserver().addOnGlobalLayoutListener(this);
        if (OmniboxFeatures.omniboxConsumesImeInsets()) {
            mDeferredIMEWindowInsetApplicationCallback =
                    new DeferredIMEWindowInsetApplicationCallback(
                            this::recalculateOmniboxAlignment);
            mDeferredIMEWindowInsetApplicationCallback.attach(mWindowAndroid);
        }
        onConfigurationChanged(mContext.getResources().getConfiguration());
        recalculateOmniboxAlignment();
    }

    @Override
    public void onDetachedFromWindow() {
        mAnchorView.removeOnLayoutChangeListener(this);
        mHorizontalAlignmentView.removeOnLayoutChangeListener(this);
        mAnchorView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
        if (mDeferredIMEWindowInsetApplicationCallback != null) {
            mDeferredIMEWindowInsetApplicationCallback.detach();
            mDeferredIMEWindowInsetApplicationCallback = null;
        }
    }

    @Override
    @NonNull
    public WindowDelegate getWindowDelegate() {
        return mWindowDelegate;
    }

    // View.OnLayoutChangeListener
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        recalculateOmniboxAlignment();
    }

    // OnGlobalLayoutListener
    @Override
    public void onGlobalLayout() {
        if (offsetInWindowChanged(mAnchorView) || insetsHaveChanged(mAnchorView)) {
            recalculateOmniboxAlignment();
        }
    }

    // ComponentCallbacks
    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        int windowWidth = newConfig.screenWidthDp;
        int windowHeight = newConfig.screenHeightDp;
        if (windowWidth == mWindowWidthDp && mWindowHeightDp == windowHeight) return;
        mWindowWidthDp = windowWidth;
        mWindowHeightDp = windowHeight;

        if (OmniboxFeatures.shouldAdaptToNarrowTabletWindows()
                || OmniboxFeatures.omniboxConsumesImeInsets()) {
            recalculateOmniboxAlignment();
        }
    }

    @Override
    public void onLowMemory() {}

    /**
     * Recalculates the desired alignment of the omnibox and sends the updated alignment data to
     * any observers. Currently will send an update message unconditionally. This method is called
     * during layout and should avoid memory allocations other than the necessary new
     * OmniboxAlignment().
     *  The method aligns the omnibox dropdown as follows:
     *  Case 1: Omnibox revamp enabled on tablet window.
     *  | anchor  [  alignment  ]       |
     *            |  dropdown   |
     *  Case 2: Omnibox revamp disabled on tablet window.
     *  | anchor    [alignment]         |
     *  |{pad_left} dropdown {pad_right}|
     *  Case 3: Phone window. Full width and no padding.
     *  | anchor     [alignment]        |
     *  |           dropdown            |
     */
    void recalculateOmniboxAlignment() {
        View contentView = mAnchorView.getRootView().findViewById(android.R.id.content);
        ViewUtils.getRelativeLayoutPosition(contentView, mAnchorView, mPositionArray);
        int top = mPositionArray[1] + mAnchorView.getMeasuredHeight();
        int left;
        int width;
        int paddingLeft;
        int paddingRight;
        if (isTablet()) {
            ViewUtils.getRelativeLayoutPosition(
                    mAnchorView, mHorizontalAlignmentView, mPositionArray);
            if (OmniboxFeatures.shouldShowModernizeVisualUpdate(mContext)) {
                // Case 1: tablets with revamp enabled. Width equal to alignment view and left
                // equivalent to left of alignment view.
                left = mPositionArray[0];
                width = mHorizontalAlignmentView.getMeasuredWidth();
                paddingLeft = 0;
                paddingRight = 0;
            } else {
                // Case 2: tablets with revamp disabled. Full bleed width with padding to align
                // suggestions to the alignment view.
                left = 0;
                width = mAnchorView.getMeasuredWidth();
                paddingLeft = mPositionArray[0];
                paddingRight = mAnchorView.getMeasuredWidth()
                        - mHorizontalAlignmentView.getMeasuredWidth() - mPositionArray[0];
            }
        } else {
            // Case 3: phones or phone-sized windows on tablets. Full bleed width with no padding or
            // positioning adjustments.
            left = 0;
            width = mAnchorView.getMeasuredWidth();
            paddingLeft = 0;
            paddingRight = 0;
        }

        // The height param shouldn't be used with omniboxConsumesImeInsets() off; -1 is a sentinel
        // value that will reveal a problem quickly.
        int height = -1;
        if (OmniboxFeatures.omniboxConsumesImeInsets()) {
            int mKeyboardHeight = mDeferredIMEWindowInsetApplicationCallback != null
                    ? mDeferredIMEWindowInsetApplicationCallback.getCurrentKeyboardHeight()
                    : 0;
            height = DisplayUtil.dpToPx(mWindowAndroid.getDisplay(), mWindowHeightDp) - top
                    - mKeyboardHeight;
        }

        // TODO(pnoland@, https://crbug.com/1416985): avoid pushing changes that are identical to
        // the previous alignment value.
        OmniboxAlignment omniboxAlignment =
                new OmniboxAlignment(left, top, width, height, paddingLeft, paddingRight);
        mOmniboxAlignmentSupplier.set(omniboxAlignment);
    }

    /**
     * Returns whether the given view's position in the window has changed since the last call to
     * offsetInWindowChanged().
     */
    private boolean offsetInWindowChanged(View view) {
        view.getLocationInWindow(mPositionArray);
        boolean result = mVerticalOffsetInWindow != mPositionArray[1];
        mVerticalOffsetInWindow = mPositionArray[1];
        return result;
    }

    /**
     * Returns whether the window insets corresponding to the given view have changed since the
     * last call to insetsHaveChanged().
     */
    private boolean insetsHaveChanged(View view) {
        WindowInsets rootWindowInsets = view.getRootWindowInsets();
        if (rootWindowInsets == null) return false;
        WindowInsetsCompat windowInsetsCompat =
                WindowInsetsCompat.toWindowInsetsCompat(view.getRootWindowInsets(), view);
        boolean result = !windowInsetsCompat.equals(mWindowInsetsCompat);
        mWindowInsetsCompat = windowInsetsCompat;
        return result;
    }
}
