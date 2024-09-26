// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.fakes;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Mimics the Chrome TestViewAndroidDelegate in chrome/browser/tab for use in tests,
 * driven by the {@code RenderWidgetHostViewAndroidTest}.
 */
@JNINamespace("content")
class TestViewAndroidDelegate extends ViewAndroidDelegate {
    /** Stores the Visual Viewport bottom inset when under test, just like the real one. */
    private int mApplicationViewportInsetBottomPx;

    /**
     * Private constructor called by the create method from native.
     */
    private TestViewAndroidDelegate() {
        super(null);
    }

    /**
     * Creates an instance that's similar in behavior to the other various ViewAndroidDelegates.
     * @return A fake {@link TestViewAndroidDelegate} to be used in a native test.
     */
    @CalledByNative
    private static TestViewAndroidDelegate create() {
        return new TestViewAndroidDelegate();
    }

    /**
     * Insets the Visual Viewport bottom, just like the real {@code TestViewAndroidDelegate} does.
     * @param viewportInsetBottomPx Amount to inset.
     */
    @CalledByNative
    private void insetViewportBottom(int viewportInsetBottomPx) {
        mApplicationViewportInsetBottomPx = viewportInsetBottomPx;
    }

    @Override
    protected int getViewportInsetBottom() {
        return mApplicationViewportInsetBottomPx;
    }

    private int[] mDisplayFeature;

    /**
     * Sets the display feature in order to return it just like the real {@code
     * TabViewAndroidDelegate does.
     * @param display_feature int array representing the top, left, right, bottom of display feature
     *         rect.
     */
    @CalledByNative
    private void setDisplayFeature(int left, int top, int right, int bottom) {
        mDisplayFeature = new int[] {left, top, right, bottom};
    }

    @Override
    protected int[] getDisplayFeature() {
        return mDisplayFeature;
    }
}
