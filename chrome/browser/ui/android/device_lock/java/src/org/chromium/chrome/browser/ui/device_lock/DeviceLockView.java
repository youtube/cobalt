// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.components.browser_ui.widget.DualControlLayout;

public class DeviceLockView extends LinearLayout {
    private TextView mTitle;
    private TextView mDescription;
    private TextView mHighlightedNotice;
    private DualControlLayout mButtonBar;
    private Button mContinueButton;
    private Button mDismissButton;

    public DeviceLockView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public static DeviceLockView create(LayoutInflater inflater) {
        DeviceLockView view =
                (DeviceLockView) inflater.inflate(R.layout.device_lock_view, null, false);
        view.setClipToOutline(true);
        return view;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.device_lock_title);
        mDescription = findViewById(R.id.device_lock_description);
        mHighlightedNotice = findViewById(R.id.device_lock_highlighted_notice);

        mDismissButton = DualControlLayout.createButtonForLayout(getContext(), false, "", null);
        mDismissButton.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mContinueButton = DualControlLayout.createButtonForLayout(getContext(), true, "", null);
        mContinueButton.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mButtonBar = findViewById(R.id.dual_control_button_bar);
        mButtonBar.addView(mContinueButton);
        mButtonBar.addView(mDismissButton);
        mButtonBar.setAlignment(DualControlLayout.DualControlLayoutAlignment.APART);
    }

    TextView getTitle() {
        return mTitle;
    }

    TextView getDescription() {
        return mDescription;
    }

    TextView getHighlightedNotice() {
        return mHighlightedNotice;
    }

    TextView getContinueButton() {
        return mContinueButton;
    }

    TextView getDismissButton() {
        return mDismissButton;
    }
}
