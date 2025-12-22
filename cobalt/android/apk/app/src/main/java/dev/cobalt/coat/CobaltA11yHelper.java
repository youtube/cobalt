// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package dev.cobalt.coat;

import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.customview.widget.ExploreByTouchHelper;
import java.lang.ref.WeakReference;
import java.util.BitSet;
import java.util.List;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * An ExploreByTouchHelper that create a virtual d-pad grid, so that Cobalt
 * remains functional when the TalkBack screen reader is enabled (which
 * otherwise intercepts d-pad events for most applications).
 */
class CobaltA11yHelper extends ExploreByTouchHelper {

    private static final String TAG = "CobaltA11yHelper";
    // The fake dimensions for the nine virtual views.
    // These values are arbitrary as long as the views stay on the screen.
    private static final int FAKE_VIEW_HEIGHT = 10;
    private static final int FAKE_VIEW_WIDTH = 10;

    private static final int CENTER_VIEW_ID = 5;
    private static final int UP_VIEW_ID = 2;
    private static final int DOWN_VIEW_ID = 8;
    private static final int LEFT_VIEW_ID = 4;
    private static final int RIGHT_VIEW_ID = 6;

    private static final int INPUT_FOCUS_CHANGE_DELAY = 1500; // milliseconds

    // This set tracks whether onPopulateNodeForVirtualView has been
    // called for each virtual view id.
    private final BitSet mNodePopulatedSet = new BitSet(9);
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private boolean mUnhandledInput;
    private boolean mHasInitialFocusBeenSet;

    // Add WeakReference to CobaltActivity to avoid creating dependency cycles.
    private WeakReference<CobaltActivity> mCobaltActivityRef;

    public CobaltA11yHelper(CobaltActivity cobaltActivity, View view) {
        super(view);
        ViewCompat.setAccessibilityDelegate(view, this);
        //this.webContents = webContents;
        this.mCobaltActivityRef = new WeakReference(cobaltActivity);
    }

    @Override
    protected int getVirtualViewAt(float x, float y) {
        // This method is only required for touch or mouse interfaces.
        // Since we don't support either, we simply always return HOST_ID.
        return HOST_ID;
    }

    @Override
    protected void getVisibleVirtualViews(List<Integer> virtualViewIds) {
        if (!virtualViewIds.isEmpty()) {
            throw new RuntimeException("Expected empty list");
        }
        // We always have precisely 9 virtual views.
        for (int i = 1; i <= 9; i++) {
            virtualViewIds.add(i);
        }
    }

    private void focusOnCenter() {
        // Setting Accessibility focus to CENTER_VIEW_ID will make TalkBack focus
        // on CENTER_VIEW_ID immediately, but the actual mouse focus is either
        // unchanged or return INVALID_ID.
        mHandler.post(
                new Runnable() {
            @Override
            public void run() {
                sendEventForVirtualView(
                        CENTER_VIEW_ID, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
            }
        });

        // There is a known Android bug about setting focus too early
        // taking no effect. The impact for Cobalt is that sometimes after
        // we click on a video, TalkBack sees nothing in focus in the watch
        // page if no user input happens. To avoid this bug we have to
        // delay the focus long enough for all the TalkBack movements to settle
        // down. More details here: https://stackoverflow.com/questions/28472985.
        mHandler.postDelayed(
                new Runnable() {
            @Override
            public void run() {
                sendEventForVirtualView(CENTER_VIEW_ID, AccessibilityEvent.TYPE_VIEW_FOCUSED);
            }
        },
                INPUT_FOCUS_CHANGE_DELAY);
    }

    private void maybeInjectEvent(int currentFocusedViewId) {
        if (!mUnhandledInput) {
            return;
        }
        CobaltActivity cobaltActivity = mCobaltActivityRef.get();
        switch (currentFocusedViewId) {
            case CENTER_VIEW_ID:
                // no move;
                break;
            case UP_VIEW_ID:
                cobaltActivity.onKeyDown(KeyEvent.KEYCODE_DPAD_UP, new KeyEvent(KeyEvent.KEYCODE_DPAD_UP, KeyEvent.ACTION_DOWN));
                cobaltActivity.onKeyUp(KeyEvent.KEYCODE_DPAD_UP, new KeyEvent(KeyEvent.KEYCODE_DPAD_UP, KeyEvent.ACTION_UP));
                break;
            case LEFT_VIEW_ID:
                cobaltActivity.onKeyDown(KeyEvent.KEYCODE_DPAD_LEFT, new KeyEvent(KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.ACTION_DOWN));
                cobaltActivity.onKeyUp(KeyEvent.KEYCODE_DPAD_LEFT, new KeyEvent(KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.ACTION_UP));
                break;
            case RIGHT_VIEW_ID:
                cobaltActivity.onKeyDown(KeyEvent.KEYCODE_DPAD_RIGHT, new KeyEvent(KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.ACTION_DOWN));
                cobaltActivity.onKeyUp(KeyEvent.KEYCODE_DPAD_RIGHT, new KeyEvent(KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.ACTION_UP));
                break;
            case DOWN_VIEW_ID:
                cobaltActivity.onKeyDown(KeyEvent.KEYCODE_DPAD_DOWN, new KeyEvent(KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.ACTION_DOWN));
                cobaltActivity.onKeyUp(KeyEvent.KEYCODE_DPAD_DOWN, new KeyEvent(KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.ACTION_UP));
                break;
            default:
                // TODO: Could support diagonal movements, although it's likely
                // not possible to reach this.
                break;
        }

        mUnhandledInput = false;
        focusOnCenter();
    }

    /**
     *
     *
     * <pre>Fake number grid:
     *   |+-+-+-+
     *   ||1|2|3|
     *   |+-+-+-|
     *   ||4|5|6|
     *   |+-+-+-|
     *   ||7|8|9|
     *   |+-+-+-+
     * </pre>
     *
     * <p>
     * The focus always starts from the middle number 5. When user changes
     * focus, the focus is then moved to either 2, 4, 6 or 8 and we can capture
     * the movement this way. The focus is then quickly switched back to the
     * center 5 to be ready for the next movement.
     */
    @Override
    protected void onPopulateNodeForVirtualView(int virtualViewId, AccessibilityNodeInfoCompat node) {
        focusOnWebContentsView();

        int focusedViewId = getAccessibilityFocusedVirtualViewId();

        if (focusedViewId < 1 || focusedViewId > 9) {
            // If this is not one of our nine-patch views, it's probably HOST_ID
            // In any case, assume there is no focus change.
            focusedViewId = CENTER_VIEW_ID;
        }

        // onPopulateNodeForVirtualView() gets called at least once every
        // time the focused view changes. So see if it's changed since the
        // last time we've been called and inject an event if so.
        if (focusedViewId != CENTER_VIEW_ID) {
            maybeInjectEvent(focusedViewId);
        } else {
            mUnhandledInput = true;
        }

        int x = (virtualViewId - 1) % 3;
        int y = (virtualViewId - 1) / 3;

        // Note that the specific bounds here are arbitrary. The importance
        // is the relative bounds to each other.
        node.setBoundsInParent(
                new Rect(
                        x * FAKE_VIEW_WIDTH,
                        y * FAKE_VIEW_HEIGHT,
                        x * FAKE_VIEW_WIDTH + FAKE_VIEW_WIDTH,
                        y * FAKE_VIEW_HEIGHT + FAKE_VIEW_HEIGHT));
        node.setText("");

        if (virtualViewId >= 1 && virtualViewId <= 9) {
            mNodePopulatedSet.set(virtualViewId - 1);
        }
        if (!mHasInitialFocusBeenSet && mNodePopulatedSet.cardinality() == 9) {
            // Once the ExploreByTouchHelper knows about all of our virtual views,
            // but not before, ask that the accessibility focus be moved from
            // it's initial position on HOST_ID to the one we want to start with.
            mHasInitialFocusBeenSet = true;
            focusOnCenter();
        }
    }

    @Override
    protected boolean onPerformActionForVirtualView(int virtualViewId, int action, Bundle arguments) {
        return false;
    }

    /**
     * When the accessibility service is active, it can prevent the application
     * from gaining focus directly. This code is our best-effort attempt to
     * return focus to the browser's web view.
     */
    private void focusOnWebContentsView() {
        CobaltActivity cobaltActivity = mCobaltActivityRef.get();
        if (cobaltActivity == null) {
            Log.i(TAG, "CobaltActivity is null");
            return;
        }
        WebContents webContents = cobaltActivity.getActiveWebContents();
        if (webContents == null) {
            Log.i(TAG, "WebContents is null");
            return;
        }
        ViewAndroidDelegate delegate = webContents.getViewAndroidDelegate();
        if (delegate == null) {
            Log.i(TAG, "ViewAndroidDelegate is null");
            return;
        }
        View webContentsView = delegate.getContainerView();
        if (webContentsView == null) {
            Log.i(TAG, "WebContentsView is null");
            return;
        }

        webContentsView.requestFocus();
    }
}
