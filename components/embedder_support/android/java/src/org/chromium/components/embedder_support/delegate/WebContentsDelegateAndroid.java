// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import android.view.KeyEvent;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;

/**
 * Java peer of the native class of the same name.
 */
@JNINamespace("web_contents_delegate_android")
public class WebContentsDelegateAndroid {
    // Equivalent of WebCore::WebConsoleMessage::LevelTip.
    public static final int LOG_LEVEL_TIP = 0;
    // Equivalent of WebCore::WebConsoleMessage::LevelLog.
    public static final int LOG_LEVEL_LOG = 1;
    // Equivalent of WebCore::WebConsoleMessage::LevelWarning.
    public static final int LOG_LEVEL_WARNING = 2;
    // Equivalent of WebCore::WebConsoleMessage::LevelError.
    public static final int LOG_LEVEL_ERROR = 3;

    /**
     * @param url
     * @param disposition         The new tab disposition, defined in
     *                            //ui/base/mojo/window_open_disposition.mojom.
     * @param isRendererInitiated Whether or not the renderer initiated this action.
     */
    @CalledByNative
    public void openNewTab(GURL url, String extraHeaders, ResourceRequestBody postData,
            int disposition, boolean isRendererInitiated) {}

    @CalledByNative
    public void activateContents() {}

    @CalledByNative
    public void closeContents() {}

    @CalledByNative
    public void loadingStateChanged(boolean shouldShowLoadingUI) {}

    @CalledByNative
    public void navigationStateChanged(int flags) {}

    @CalledByNative
    public void visibleSSLStateChanged() {}

    /**
     * Signaled when the renderer has been deemed to be unresponsive.
     */
    @CalledByNative
    public void rendererUnresponsive() {}

    /**
     * Signaled when the render has been deemed to be responsive.
     */
    @CalledByNative
    public void rendererResponsive() {}

    @CalledByNative
    public void webContentsCreated(WebContents sourceWebContents, long openerRenderProcessId,
            long openerRenderFrameId, String frameName, GURL targetUrl,
            WebContents newWebContents) {}

    @CalledByNative
    public boolean shouldCreateWebContents(GURL targetUrl) {
        return true;
    }

    @CalledByNative
    public void onUpdateUrl(GURL url) {}

    @CalledByNative
    public boolean takeFocus(boolean reverse) {
        return false;
    }

    @CalledByNative
    public void handleKeyboardEvent(KeyEvent event) {
        // TODO(bulach): we probably want to re-inject the KeyEvent back into
        // the system. Investigate if this is at all possible.
    }

    /**
     * Report a JavaScript console message.
     *
     * @param level message level. One of WebContentsDelegateAndroid.LOG_LEVEL*.
     * @param message the error message.
     * @param lineNumber the line number int the source file at which the error is reported.
     * @param sourceId the name of the source file that caused the error.
     * @return true if the client will handle logging the message.
     */
    @CalledByNative
    public boolean addMessageToConsole(int level, String message, int lineNumber, String sourceId) {
        return false;
    }

    /**
     * Report a form resubmission. The overwriter of this function should eventually call
     * either of NavigationController.ContinuePendingReload or
     * NavigationController.CancelPendingReload.
     */
    @CalledByNative
    public void showRepostFormWarningDialog() {}

    @CalledByNative
    public void enterFullscreenModeForTab(boolean prefersNavigationBar, boolean prefersStatusBar) {}

    @CalledByNative
    public void fullscreenStateChangedForTab(
            boolean prefersNavigationBar, boolean prefersStatusBar) {}

    @CalledByNative
    public void exitFullscreenModeForTab() {}

    @CalledByNative
    public boolean isFullscreenForTabOrPending() {
        return false;
    }

    /**
     * Called when BrowserMediaPlayerManager wants to load a media resource.
     * @param url the URL of media resource to load.
     * @return true to prevent the resource from being loaded.
     */
    @CalledByNative
    public boolean shouldBlockMediaRequest(GURL url) {
        return false;
    }

    /**
     * @return The height of the top controls in DIP.
     */
    @CalledByNative
    public int getTopControlsHeight() {
        return 0;
    }

    /**
     * @return The minimum visible height the top controls can have in DIP.
     */
    @CalledByNative
    public int getTopControlsMinHeight() {
        return 0;
    }

    /**
     * @return The height of the bottom controls in DIP.
     */
    @CalledByNative
    public int getBottomControlsHeight() {
        return 0;
    }

    /**
     * @return The minimum visible height the bottom controls can have in DIP.
     */
    @CalledByNative
    public int getBottomControlsMinHeight() {
        return 0;
    }

    /**
     * @return Whether or not the browser controls height changes should be animated.
     */
    @CalledByNative
    public boolean shouldAnimateBrowserControlsHeightChanges() {
        return false;
    }

    /**
     * @return Whether or not the browser controls resize Blink's view size.
     */
    @CalledByNative
    public boolean controlsResizeView() {
        return false;
    }

    /**
     * @return If shown, returns the height of the virtual keyboard in physical pixels. Otherwise,
     *         returns 0.
     */
    @CalledByNative
    public int getVirtualKeyboardHeight() {
        return 0;
    }

    /**
     * Check and return the {@link DisplayMode} value.
     *
     * @return The {@link DisplayMode} value.
     */
    @CalledByNative
    protected final int getDisplayModeChecked() {
        int displayMode = getDisplayMode();
        assert DisplayMode.isKnownValue(displayMode);
        return displayMode;
    }

    /**
     * @return The {@link DisplayMode} value.
     */
    public int getDisplayMode() {
        return DisplayMode.UNDEFINED;
    }
}
