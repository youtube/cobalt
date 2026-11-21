// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.content.Context;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ImeAdapter;

/**
 * Invisible View hosted inside XrHostActivity that overrides onCreateInputConnection to delegate to
 * the browser's ImeAdapter.
 */
@NullMarked
public class XrHostProxyInputView extends View {
    private static final String TAG = "XrInput";
    private static final boolean DEBUG_LOGS = false;

    private @Nullable ImeAdapter mImeAdapter;

    public XrHostProxyInputView(Context context) {
        super(context);
        setFocusable(true);
        setFocusableInTouchMode(true);
        setAlpha(0.0f); // Make it invisible but still capable of taking focus.
        setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    public void setImeAdapter(ImeAdapter imeAdapter) {
        mImeAdapter = imeAdapter;
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return true;
    }

    @Override
    public @Nullable InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (DEBUG_LOGS) Log.i(TAG, "onCreateInputConnection, mImeAdapter=" + mImeAdapter);
        if (mImeAdapter != null) {
            InputConnection ic = mImeAdapter.onCreateInputConnection(outAttrs);
            if (DEBUG_LOGS) Log.i(TAG, "onCreateInputConnection returned: " + ic);
            return ic;
        }
        return null;
    }

    /**
     * Adds this view to the provided FrameLayout layout. We use a 1x1 size because the Android IME
     * framework requires the target view to have a non-zero size to be focusable/active. Along with
     * alpha=0 (set in the constructor), this ensures it is functionally active but invisible.
     */
    public void addToLayout(FrameLayout layout) {
        layout.addView(this, new FrameLayout.LayoutParams(1, 1));
    }
}
