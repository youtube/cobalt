// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.accessibility.AccessibilityEvent;
import android.widget.AdapterView;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * The dropdown popup window for use on Lollipop+. Internally uses an AnchoredPopupWindow
 * anchored to a view to display a list of options.
 */
class DropdownPopupWindowImpl
        implements AnchoredPopupWindow.LayoutObserver, DropdownPopupWindowInterface {
    private final Context mContext;
    private final View mAnchorView;
    private boolean mRtl;
    private int mInitialSelection = -1;
    private OnLayoutChangeListener mLayoutChangeListener;
    private CharSequence mDescription;
    private AnchoredPopupWindow mAnchoredPopupWindow;
    ListAdapter mAdapter;

    private final ListView mListView;
    private Drawable mBackground;
    private int mHorizontalPadding;

    public DropdownPopupWindowImpl(Context context, View anchorView) {
        this(context, anchorView, null);
    }

    /**
     * Creates an DropdownPopupWindowImpl with specified parameters.
     * @param context Application context.
     * @param anchorView Popup view to be anchored.
     * @param visibleWebContentsRectProvider The {@link RectProvider} which will be used for {@link
     *         AnchoredPopupWindow}.
     */
    public DropdownPopupWindowImpl(Context context, View anchorView,
            @Nullable RectProvider visibleWebContentsRectProvider) {
        mContext = context;
        mAnchorView = anchorView;

        mAnchorView.setId(R.id.dropdown_popup_window);
        mAnchorView.setTag(this);

        mLayoutChangeListener = new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (v == mAnchorView) DropdownPopupWindowImpl.this.show();
            }
        };
        mAnchorView.addOnLayoutChangeListener(mLayoutChangeListener);

        PopupWindow.OnDismissListener onDismissLitener = new PopupWindow.OnDismissListener() {
            @Override
            public void onDismiss() {
                mAnchoredPopupWindow.dismiss();
                mAnchorView.removeOnLayoutChangeListener(mLayoutChangeListener);
                mAnchorView.setTag(null);
            }
        };

        mListView = new ListView(context);

        ViewRectProvider rectProvider = new ViewRectProvider(mAnchorView);
        rectProvider.setIncludePadding(true);
        mBackground = AppCompatResources.getDrawable(context, R.drawable.menu_bg_baseline);
        mAnchoredPopupWindow = new AnchoredPopupWindow(context, mAnchorView, mBackground, mListView,
                rectProvider, visibleWebContentsRectProvider);
        mAnchoredPopupWindow.addOnDismissListener(onDismissLitener);
        mAnchoredPopupWindow.setLayoutObserver(this);
        mAnchoredPopupWindow.setElevation(
                context.getResources().getDimensionPixelSize(R.dimen.dropdown_elevation));
        Rect paddingRect = new Rect();
        mBackground.getPadding(paddingRect);
        rectProvider.setInsetPx(0, /* top= */ paddingRect.bottom, 0, /* bottom= */ paddingRect.top);
        mHorizontalPadding = paddingRect.right + paddingRect.left;
        mAnchoredPopupWindow.setPreferredHorizontalOrientation(
                AnchoredPopupWindow.HorizontalOrientation.CENTER);
        mAnchoredPopupWindow.setUpdateOrientationOnChange(true);
        mAnchoredPopupWindow.setOutsideTouchable(true);
    }

    /**
     * Sets the adapter that provides the data and the views to represent the data
     * in this popup window.
     *
     * @param adapter The adapter to use to create this window's content.
     */
    @Override
    public void setAdapter(ListAdapter adapter) {
        mAdapter = adapter;
        mListView.setAdapter(adapter);
        mAnchoredPopupWindow.onRectChanged();
    }

    @Override
    public void onPreLayoutChange(
            boolean positionBelow, int x, int y, int width, int height, Rect anchorRect) {
        mBackground.setBounds(anchorRect);
        mAnchoredPopupWindow.setBackgroundDrawable(
                AppCompatResources.getDrawable(mContext, R.drawable.menu_bg_baseline));
    }

    /**
     * Sets the initial selection.
     *
     * @param initialSelection The index of the initial item to select.
     */
    @Override
    public void setInitialSelection(int initialSelection) {
        mInitialSelection = initialSelection;
    }

    /**
     * Shows the popup. The adapter should be set before calling this method.
     */
    @Override
    public void show() {
        assert mAdapter != null : "Set the adapter before showing the popup.";
        boolean wasShowing = mAnchoredPopupWindow.isShowing();
        mAnchoredPopupWindow.setVerticalOverlapAnchor(false);
        mAnchoredPopupWindow.setHorizontalOverlapAnchor(true);

        int windowWidthPx = mContext.getResources().getDisplayMetrics().widthPixels;
        int contentWidth = measureContentWidth();
        if (windowWidthPx < contentWidth + mHorizontalPadding) {
            mAnchoredPopupWindow.setMaxWidth(windowWidthPx - mHorizontalPadding);
        } else if (mAnchorView.getWidth() < contentWidth) {
            mAnchoredPopupWindow.setMaxWidth(contentWidth + mHorizontalPadding);
        } else {
            mAnchoredPopupWindow.setMaxWidth(mAnchorView.getWidth() + mHorizontalPadding);
        }

        mAnchoredPopupWindow.show();
        mListView.setDividerHeight(0);
        int layoutDirection = mRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR;
        mListView.setLayoutDirection(layoutDirection);
        if (!wasShowing) {
            mListView.setContentDescription(mDescription);
            mListView.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        }
        if (mInitialSelection >= 0) {
            mListView.setSelection(mInitialSelection);
            mInitialSelection = -1;
        }
    }

    /**
     * Set a listener to receive a callback when the popup is dismissed.
     *
     * @param listener Listener that will be notified when the popup is dismissed.
     */
    @Override
    public void setOnDismissListener(PopupWindow.OnDismissListener listener) {
        mAnchoredPopupWindow.addOnDismissListener(listener);
    }

    /**
     * Sets the text direction in the dropdown. Should be called before show().
     * @param isRtl If true, then dropdown text direction is right to left.
     */
    @Override
    public void setRtl(boolean isRtl) {
        mRtl = isRtl;
    }

    /**
     * Disable hiding on outside tap so that tapping on a text input field associated with the popup
     * will not hide the popup.
     */
    @Override
    public void disableHideOnOutsideTap() {
        mAnchoredPopupWindow.setDismissOnTouchInteraction(false);
    }

    /**
     * Sets the content description to be announced by accessibility services when the dropdown is
     * shown.
     * @param description The description of the content to be announced.
     */
    @Override
    public void setContentDescriptionForAccessibility(CharSequence description) {
        mDescription = description;
    }

    /**
     * Sets a listener to receive events when a list item is clicked.
     *
     * @param clickListener Listener to register
     */
    @Override
    public void setOnItemClickListener(AdapterView.OnItemClickListener clickListener) {
        mListView.setOnItemClickListener(clickListener);
    }

    /**
     * Show the popup. Will have no effect if the popup is already showing.
     * Post a {@link #show()} call to the UI thread.
     */
    @Override
    public void postShow() {
        mAnchoredPopupWindow.show();
    }

    /**
     * Disposes of the popup window.
     */
    @Override
    public void dismiss() {
        mAnchoredPopupWindow.dismiss();
    }

    /**
     * @return The {@link ListView} displayed within the popup window.
     */
    @Override
    public ListView getListView() {
        return mListView;
    }

    /**
     * @return Whether the popup is currently showing.
     */
    @Override
    public boolean isShowing() {
        return mAnchoredPopupWindow.isShowing();
    }

    /**
     * Measures the width of the list content. The adapter should not be null.
     * @return The popup window width in pixels.
     */
    private int measureContentWidth() {
        assert mAdapter != null : "Set the adapter before showing the popup.";
        return UiUtils.computeMaxWidthOfListAdapterItems(mAdapter);
    }
}
