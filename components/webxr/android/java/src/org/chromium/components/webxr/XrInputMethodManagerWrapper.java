// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.os.IBinder;
import android.os.ResultReceiver;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.ExtractedText;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.InputMethodManagerWrapper;
import org.chromium.ui.base.WindowAndroid;

/**
 * Proxy InputMethodManagerWrapper that redirects IME calls from the browser's view to our
 * ProxyInputView inside XrHostActivity.
 *
 * <p>Most methods in this class discard the passed-in {@link View} parameter and forward the call
 * using {@code mProxyView} instead. This is necessary because the browser's IME system (via {@link
 * org.chromium.content.browser.input.ImeAdapterImpl}) still triggers calls using the browser's
 * container view, but the Android OS IME framework expects all interactions to refer to the active
 * view that actually requested/focused the keyboard (which is {@code mProxyView}).
 */
@NullMarked
public class XrInputMethodManagerWrapper implements InputMethodManagerWrapper {
    private static final String TAG = "XrInput";
    private static final boolean DEBUG_LOGS = false;

    private final InputMethodManagerWrapper mDelegate;
    private final View mProxyView;

    public XrInputMethodManagerWrapper(InputMethodManagerWrapper delegate, View proxyView) {
        mDelegate = delegate;
        mProxyView = proxyView;
    }

    @Override
    public void restartInput(View view) {
        if (DEBUG_LOGS) Log.i(TAG, "restartInput");
        mDelegate.restartInput(mProxyView);
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        if (DEBUG_LOGS) Log.i(TAG, "showSoftInput: requesting focus on proxy view");
        // Android's InputMethodManager requires the target view to be focused in its window
        // for showSoftInput to succeed. Since the user's touch and controller interactions in XR
        // bypass the standard Android view touch propagation, the OS doesn't automatically focus
        // our proxy view. We must request focus programmatically right before summoning the IME.
        mProxyView.requestFocus();
        mDelegate.showSoftInput(mProxyView, flags, resultReceiver);
    }

    @Override
    public boolean isActive(@Nullable View view) {
        boolean active = mDelegate.isActive(mProxyView);
        if (DEBUG_LOGS) Log.i(TAG, "isActive: " + active);
        return active;
    }

    @Override
    public boolean hideSoftInputFromWindow(
            IBinder windowToken, int flags, @Nullable ResultReceiver resultReceiver) {
        if (DEBUG_LOGS) Log.i(TAG, "hideSoftInputFromWindow");
        // We override the passed-in windowToken with the token from mProxyView. The system
        // InputMethodManager requires the window token of the window that currently owns the active
        // IME session (XrHostActivity's window) to successfully hide the keyboard. Using the
        // browser's window token would fail as it is in the background.
        return mDelegate.hideSoftInputFromWindow(
                mProxyView.getWindowToken(), flags, resultReceiver);
    }

    @Override
    public void updateSelection(
            View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
        mDelegate.updateSelection(mProxyView, selStart, selEnd, candidatesStart, candidatesEnd);
    }

    @Override
    public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {
        mDelegate.updateCursorAnchorInfo(mProxyView, cursorAnchorInfo);
    }

    @Override
    public void updateExtractedText(View view, int token, @Nullable ExtractedText text) {
        mDelegate.updateExtractedText(mProxyView, token, text);
    }

    @Override
    public void onWindowAndroidChanged(@Nullable WindowAndroid newWindowAndroid) {
        mDelegate.onWindowAndroidChanged(newWindowAndroid);
    }

    @Override
    public void onInputConnectionCreated() {
        mDelegate.onInputConnectionCreated();
    }
}
