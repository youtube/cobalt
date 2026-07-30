// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContent;

/** Implements {@code SidePanelCoordinatorAndroid}. */
@NullMarked
public final class SidePanelCoordinatorAndroidImpl implements SidePanelCoordinatorAndroid {
    private static final String TAG = "SidePanelCoordinatorAndroidImpl";

    /** Sentinel value for invalid or unset coordinates. */
    private static final int INVALID_COORDINATE = -1;

    private final SidePanelContainerCoordinator mSidePanelContainerCoordinator;

    /** Address of the native {@code SidePanelCoordinatorAndroid}. */
    private long mNativeSidePanelCoordinatorAndroid;

    private boolean mDisableAnimationsForTesting;

    public SidePanelCoordinatorAndroidImpl(
            SidePanelContainerCoordinator sidePanelContainerCoordinator) {
        log(TAG, "constructor", sidePanelContainerCoordinator);
        mSidePanelContainerCoordinator = sidePanelContainerCoordinator;
    }

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        long nativeBrowserWindowPtr = initInfo.nativeBrowserWindowPtr;
        log(TAG, "onAddedToTask", nativeBrowserWindowPtr);
        createNativePtr(nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        log(TAG, "onFeatureRemoved");
        destroyNativePtr();
    }

    @Override
    public boolean hasContentToShow() {
        boolean hasContentToShow =
                mNativeSidePanelCoordinatorAndroid != 0
                        ? SidePanelCoordinatorAndroidImplJni.get()
                                .hasContentToShow(mNativeSidePanelCoordinatorAndroid)
                        : false;

        log(TAG, "hasContentToShow", hasContentToShow);
        return hasContentToShow;
    }

    @Override
    public void init() {
        log(TAG, "init");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get().init(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @Override
    public void onWillAutoClose() {
        log(TAG, "onWillAutoClose");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onWillAutoClose(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @Override
    public void onWillAutoRestore() {
        log(TAG, "onWillAutoRestore");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onWillAutoRestore(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @VisibleForTesting
    void createNativePtr(long nativeBrowserWindowPtr) {
        log(TAG, "createNativePtr", nativeBrowserWindowPtr);
        assert nativeBrowserWindowPtr != 0
                : "Native BrowserWindowInterface pointer shouldn't be null. Is the"
                        + " ChromeAndroidTaskFeatureKey correct?";
        assert mNativeSidePanelCoordinatorAndroid == 0
                : "Native SidePanelCoordinatorAndroid already exists";
        mNativeSidePanelCoordinatorAndroid =
                SidePanelCoordinatorAndroidImplJni.get().create(this, nativeBrowserWindowPtr);
    }

    @VisibleForTesting
    void destroyNativePtr() {
        log(TAG, "destroyNativePtr");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get().destroy(mNativeSidePanelCoordinatorAndroid);
        }
    }

    long getNativePtrForTesting() {
        return mNativeSidePanelCoordinatorAndroid;
    }

    @CalledByNative
    private void clearNativePtr() {
        log(TAG, "clearNativePtr");
        mNativeSidePanelCoordinatorAndroid = 0;
    }

    @CalledByNativeForTesting
    private void disableAnimationsForTesting() {
        log(TAG, "disableAnimationsForTesting");
        mDisableAnimationsForTesting = true;
    }

    /**
     * Starts populating the side panel with content.
     *
     * @param sidePanelNativeView The view to show.
     * @param x The x coordinate of the starting bounds, or -1 if none.
     * @param y The y coordinate of the starting bounds, or -1 if none.
     * @param width The width of the starting bounds, or -1 if none.
     * @param height The height of the starting bounds, or -1 if none.
     * @param suppressAnimations Whether animations should be suppressed when showing the panel.
     */
    @CalledByNative
    private void startPopulatingContent(
            View sidePanelNativeView,
            int x,
            int y,
            int width,
            int height,
            boolean suppressAnimations) {
        log(TAG, "startPopulatingContent", sidePanelNativeView, x, y, width, height);
        mSidePanelContainerCoordinator.startPopulatingContent(
                new SidePanelContent(sidePanelNativeView),
                () -> onContentPopulated(),
                createRectFromCoordinates(x, y, width, height),
                suppressAnimations || mDisableAnimationsForTesting);
    }

    /**
     * Starts removing content and closing the side panel.
     *
     * @param suppressAnimations Whether animations should be suppressed when closing the panel.
     */
    @CalledByNative
    private void startRemovingContent(boolean suppressAnimations) {
        log(TAG, "startRemovingContent", suppressAnimations);
        mSidePanelContainerCoordinator.startRemovingContent(
                () -> onContentRemoved(), suppressAnimations || mDisableAnimationsForTesting);
    }

    @CalledByNativeForTesting
    private int getContainerWidthForTesting() {
        View view = mSidePanelContainerCoordinator.getViewForTesting(); // IN-TEST
        if (view == null || !view.isAttachedToWindow()) {
            return 0;
        }
        return view.getWidth();
    }

    private @Nullable Rect createRectFromCoordinates(int x, int y, int width, int height) {
        if (x == INVALID_COORDINATE
                && y == INVALID_COORDINATE
                && width == INVALID_COORDINATE
                && height == INVALID_COORDINATE) {
            return null;
        }
        return new Rect(x, y, x + width, y + height);
    }

    private void onContentPopulated() {
        log(TAG, "onContentPopulated");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onContentPopulated(mNativeSidePanelCoordinatorAndroid);
        }
    }

    private void onContentRemoved() {
        log(TAG, "onContentRemoved");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onContentRemoved(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code SidePanelCoordinatorAndroid}.
         *
         * @param caller The Java object calling this method.
         * @param nativeBrowserWindowPtr The pointer to the native {@code BrowserWindowInterface}.
         * @return The address of the native {@code SidePanelCoordinatorAndroid}.
         */
        long create(SidePanelCoordinatorAndroidImpl caller, long nativeBrowserWindowPtr);

        /**
         * Destroys the native {@code SidePanelCoordinatorAndroid}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void destroy(long nativeSidePanelCoordinatorAndroid);

        /**
         * Notifies the underlying native object that the content has been removed.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void onContentRemoved(long nativeSidePanelCoordinatorAndroid);

        /**
         * Notifies the underlying native object that the content has been populated.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void onContentPopulated(long nativeSidePanelCoordinatorAndroid);

        /**
         * Initializes the native coordinator and restores the active entry if one exists.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void init(long nativeSidePanelCoordinatorAndroid);

        /**
         * Returns whether the native coordinator has content to show.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#hasContentToShow
         */
        boolean hasContentToShow(long nativeSidePanelCoordinatorAndroid);

        /**
         * See {@link SidePanelCoordinatorAndroid#onWillAutoClose()}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void onWillAutoClose(long nativeSidePanelCoordinatorAndroid);

        /**
         * See {@link SidePanelCoordinatorAndroid#onWillAutoRestore()}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void onWillAutoRestore(long nativeSidePanelCoordinatorAndroid);
    }
}
