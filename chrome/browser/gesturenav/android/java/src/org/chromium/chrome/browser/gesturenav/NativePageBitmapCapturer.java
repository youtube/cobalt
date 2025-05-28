// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.back_press.BackPressMetrics;
import org.chromium.chrome.browser.back_press.BackPressMetrics.CaptureNativeViewResult;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.resources.dynamics.CaptureObserver;
import org.chromium.ui.resources.dynamics.CaptureUtils;
import org.chromium.url.GURL;

/** Capture native page as a bitmap. */
public class NativePageBitmapCapturer implements UnownedUserData {
    // Share SoftwareDraw in order to share a single java Bitmap across all tabs in a window
    // as the tab size won't change inside one single window.
    private static final UnownedUserDataKey<NativePageBitmapCapturer> CAPTURER_KEY =
            new UnownedUserDataKey<>(NativePageBitmapCapturer.class);

    private HardwareDraw mHardwareDraw;

    private NativePageBitmapCapturer() {}

    /**
     * Capture native page as a bitmap.
     *
     * @param tab The target tab to be captured.
     * @param callback Executed with a non-null bitmap if the tab is presenting a native page. Empty
     *     bitmap if capturing fails, such as out of memory error.
     * @return True if the capture is successfully triggered; otherwise false.
     */
    public static boolean maybeCaptureNativeView(
            @NonNull Tab tab, @NonNull Callback<Bitmap> callback) {
        if (!isCapturable(tab)) {
            return false;
        }

        int result = shouldUseFallbackUx(tab);
        BackPressMetrics.recordCaptureNativeViewResult(result);
        if (result != CaptureNativeViewResult.CAPTURE_SCREENSHOT) {
            PostTask.postTask(TaskTraits.UI_USER_VISIBLE, () -> callback.onResult(null));
            return true;
        }

        UnownedUserDataHost host = tab.getWindowAndroid().getUnownedUserDataHost();
        if (CAPTURER_KEY.retrieveDataFromHost(host) == null) {
            CAPTURER_KEY.attachToHost(host, new NativePageBitmapCapturer());
        }
        final var capturer = CAPTURER_KEY.retrieveDataFromHost(host);
        if (capturer.mHardwareDraw == null && enableHardwareDraw()) {
            capturer.mHardwareDraw = new HardwareDraw();
        }
        if (capturer.mHardwareDraw != null) {
            return capturer.mHardwareDraw.startBitmapCapture(
                    tab.getView(),
                    tab.getWebContents().getViewAndroidDelegate().getContainerView().getHeight(),
                    getScale(),
                    new CaptureObserver() {
                        @Override
                        public void onCaptureStart(Canvas canvas, Rect dirtyRect) {
                            canvas.drawColor(tab.getNativePage().getBackgroundColor());
                            canvas.translate(
                                    0, -tab.getNativePage().getHeightOverlappedWithTopControls());
                        }

                        @Override
                        public void onCaptureEnd() {}
                    },
                    callback);
        } else {
            Bitmap bitmap = capture(tab, false, 0);
            PostTask.postTask(TaskTraits.UI_USER_VISIBLE, () -> callback.onResult(bitmap));
            return true;
        }
    }

    /**
     * Synchronous version of {@link #maybeCaptureNativeView()}.
     *
     * @param tab The target tab to be captured.
     * @param topControlsHeight The height of the top controls.
     * @return Null if fails; otherwise, a Bitmap object.
     */
    @Nullable
    public static Bitmap maybeCaptureNativeViewSync(@NonNull Tab tab, int topControlsHeight) {
        if (!isCapturable(tab)) {
            return null;
        }

        return capture(tab, true, topControlsHeight);
    }

    private static boolean isCapturable(Tab tab) {
        if (!tab.isNativePage()) {
            return false;
        }
        // The native page, like NTP, is displayed before the url is loaded. Return early to
        // prevent capturing the current NTP as the screenshot of the previous page
        GURL lastCommittedUrl = tab.getWebContents().getLastCommittedUrl();
        if (!NativePage.isNativePageUrl(lastCommittedUrl, tab.isIncognitoBranded(), false)) {
            return false;
        }

        return true;
    }

    private static @CaptureNativeViewResult int shouldUseFallbackUx(Tab tab) {
        if (tab.getWindowAndroid() == null) {
            return CaptureNativeViewResult.NULL_WINDOW_ANDROID;
        }

        View view = tab.getView();
        // The view is not laid out yet.
        if (view.getWidth() == 0 || view.getHeight() == 0) {
            return CaptureNativeViewResult.VIEW_NOT_LAID_OUT;
        }
        if (tab.getWebContents() == null
                || tab.getWebContents().getViewAndroidDelegate() == null
                || tab.getWebContents().getViewAndroidDelegate().getContainerView() == null
                || tab.getWebContents().getViewAndroidDelegate().getContainerView().getHeight()
                        == 0) {
            return CaptureNativeViewResult.VIEW_NOT_LAID_OUT;
        }

        GURL lastCommittedUrl = tab.getWebContents().getLastCommittedUrl();
        boolean isLastPageNative =
                NativePage.isNativePageUrl(lastCommittedUrl, tab.isIncognitoBranded(), false);
        // crbug.com/376115165: Show fallback screenshots when navigating between native pages.
        // Native page views show before the navigation commit, which causes the content/ to
        // capture a wrong screenshot.
        // TODO(crbug.com/378565245): Capture screenshots when navigating between native pages.
        if (isLastPageNative
                && NativePage.isNativePageUrl(tab.getUrl(), tab.isIncognitoBranded(), false)) {
            return CaptureNativeViewResult.BETWEEN_NATIVE_PAGES;
        }

        return CaptureNativeViewResult.CAPTURE_SCREENSHOT;
    }

    private static Bitmap capture(Tab tab, boolean fullscreen, int topControlsHeight) {
        try (TraceEvent e = TraceEvent.scoped("NativePageBitmapCapturer::capture")) {
            View view = tab.getView();
            // The size of the webpage might be different from that of native pages.
            // The former may also capture the area underneath the navigation bar while
            // the latter sometimes does not. If their sizes don't match, a fallback screenshot will
            // be used instead.
            Bitmap bitmap =
                    CaptureUtils.createBitmap(
                            view.getWidth(),
                            tab.getWebContents()
                                    .getViewAndroidDelegate()
                                    .getContainerView()
                                    .getHeight());

            bitmap.eraseColor(tab.getNativePage().getBackgroundColor());

            Canvas canvas = new Canvas(bitmap);
            float scale = getScale();

            // Translate to exclude the area of the top controls if present.
            // When requesting a fullscreen bitmap, the content will translate the layer up. Add
            // top controls height to translate the content down to counter that.
            canvas.translate(
                    0,
                    -tab.getNativePage().getHeightOverlappedWithTopControls()
                            + (fullscreen ? topControlsHeight : 0));
            canvas.scale(scale, scale);
            view.draw(canvas);
            return bitmap;
        }
    }

    private static float getScale() {
        return (float)
                ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS, "downscale", 1);
    }

    private static boolean enableHardwareDraw() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.NATIVE_PAGE_TRANSITION_HARDWARE_CAPTURE);
    }
}
