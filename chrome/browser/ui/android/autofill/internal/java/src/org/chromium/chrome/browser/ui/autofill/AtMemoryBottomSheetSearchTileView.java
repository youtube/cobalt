// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;

/** View for the interactive search tile in the AtMemory bottom sheet. */
@NullMarked
public class AtMemoryBottomSheetSearchTileView extends LinearLayout {
    private ImageView mIconView;
    private TextView mTitleView;
    private TextView mDetailsView;

    public AtMemoryBottomSheetSearchTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView = findViewById(R.id.icon_view);
        mTitleView = findViewById(R.id.title_text);
        mDetailsView = findViewById(R.id.details_text);
    }

    public void setIcon(int resId) {
        mIconView.setImageResource(resId);
    }

    public void setTitle(@Nullable String text) {
        mTitleView.setText(text);
    }

    public void setDetails(@Nullable String text) {
        mDetailsView.setText(text);
        mDetailsView.setVisibility(TextUtils.isEmpty(text) ? View.GONE : View.VISIBLE);
    }

    public void setClickListener(Runnable callback) {
        setOnClickListener(v -> callback.run());
    }
}
