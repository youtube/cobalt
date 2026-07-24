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

package dev.cobalt.shell;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import dev.cobalt.shell.ContentViewRenderView;
import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 *  Copied from org.chromium.content_shell.Shell.
 *  Cobalt's own Java implementation of Shell. It does not have Shell UI.
 *  Container for the various UI components that make up a shell window.
 */
@JNINamespace("content")
@NullMarked
public class Shell {
    /**
     * Interface for notifying observers of WebContents readiness.
     */
    public interface OnWebContentsReadyListener {
        void onWebContentsReady();
    }

    private static final String TAG = "cobalt";

    private WebContents mWebContents;
    private WebContents mSplashScreenWebContents;
    private NavigationController mNavigationController;

    private long mNativeShell;
    private @Nullable StartupGuardNavigationObserver mStartupGuardNavigationObserver;
    private @Nullable ContentViewRenderView mContentViewRenderView;
    private @Nullable WindowAndroid mWindow;
    private @Nullable ShellViewAndroidDelegate mViewAndroidDelegate;

    private boolean mLoading;
    private boolean mIsFullscreen;
    private boolean mIsActivityVisible;

    private @Nullable OnWebContentsReadyListener mWebContentsReadyListener;
    private @Nullable Callback<Boolean> mOverlayModeChangedCallbackForTesting;
    private ViewGroup mRootView;

    /**
     * Constructor for inflating via XML.
     */
    public Shell(Context context) {
        Activity activity = (Activity) context;
        mRootView = activity.findViewById(android.R.id.content);
    }

    public void setRootViewForTesting(ViewGroup view) {
        mRootView = view;
    }

    public void setWebContentsReadyListener(@Nullable OnWebContentsReadyListener listener) {
        mWebContentsReadyListener = listener;
    }

    /**
     * Set the SurfaceView being rendered to as soon as it is available.
     */
    public void setContentViewRenderView(@Nullable ContentViewRenderView contentViewRenderView) {
        if (contentViewRenderView == null) {
            if (mContentViewRenderView != null) {
                mRootView.removeView(mContentViewRenderView);
            }
        } else {
            mRootView.addView(contentViewRenderView,
                    new FrameLayout.LayoutParams(
                            FrameLayout.LayoutParams.MATCH_PARENT,
                            FrameLayout.LayoutParams.MATCH_PARENT));
        }
        mContentViewRenderView = contentViewRenderView;
    }

    /**
     * Initializes the Shell for use.
     *
     * @param nativeShell The pointer to the native Shell object.
     * @param window The owning window for this shell.
     */
    public void initialize(long nativeShell, WindowAndroid window) {
        mNativeShell = nativeShell;
        mWindow = window;
    }

    /**
     * Closes the shell and cleans up the native instance, which will handle destroying all
     * dependencies.
     */
    public void close() {
        if (mNativeShell == 0) return;
        ShellJni.get().closeShell(mNativeShell);
    }

    /**
     * Load splash screen.
     */
    public void loadSplashScreenWebContents() {
        if (mNativeShell == 0) return;
        ShellJni.get().loadSplashScreenWebContents(mNativeShell);
    }

    @SuppressWarnings("NullAway")
    @CalledByNative
    private void onNativeDestroyed() {
        mWindow = null;
        mNativeShell = 0;
        mWebContents = null;
    }

    /**
     * Loads an URL.  This will perform minimal amounts of sanitizing of the URL to attempt to
     * make it valid.
     *
     * @param url The URL to be loaded by the shell.
     */
    public void loadUrl(String url) {
        if (url == null) return;

        if (TextUtils.equals(url, mWebContents.getLastCommittedUrl().getSpec())) {
            mNavigationController.reload(true);
        } else {
            mNavigationController.loadUrl(new LoadUrlParams(sanitizeUrl(url)));
        }
    }

    /**
     * Given an URL, this performs minimal sanitizing to ensure it will be valid.
     * @param url The url to be sanitized.
     * @return The sanitized URL.
     */
    public static String sanitizeUrl(String url) {
        if (url == null) return null;
        if (url.startsWith("www.") || url.indexOf(":") == -1) url = "http://" + url;
        return url;
    }

    @CalledByNative
    private void onUpdateUrl(String url) {}

    @CalledByNative
    private void onLoadProgressChanged(double progress) {}

    @CalledByNative
    private void toggleFullscreenModeForTab(boolean enterFullscreen) {
    }

    @CalledByNative
    private boolean isFullscreenForTabOrPending() {
        return mIsFullscreen;
    }

    @CalledByNative
    private void setIsLoading(boolean loading) {
        mLoading = loading;
    }

    /**
     * Initializes the ContentView based on the native tab contents pointer passed in.
     * @param webContents A {@link WebContents} object.
     */
    @CalledByNative
    @Initializer
    private void initFromNativeTabContents(WebContents webContents) {
        if (webContents == null || mContentViewRenderView == null) return;
        mViewAndroidDelegate = new ShellViewAndroidDelegate(mRootView);
        assert (mWebContents != webContents);
        if (mWebContents != null) mWebContents.clearNativeReference();
        webContents.setDelegates(
                "", mViewAndroidDelegate, null, mWindow, WebContents.createDefaultInternalsHolder());
        mWebContents = webContents;
        mNavigationController = assertNonNull(mWebContents.getNavigationController());
        if (mIsActivityVisible) {
            mWebContents.updateWebContentsVisibility(Visibility.VISIBLE);
        }
        assumeNonNull(mContentViewRenderView).setCurrentWebContents(mWebContents);
        mStartupGuardNavigationObserver = new StartupGuardNavigationObserver(mWebContents);
        if (mWebContentsReadyListener != null) {
            mWebContentsReadyListener.onWebContentsReady();
        }
    }

    /**
     * Load the native splash screen contents.
     * @param webContents A {@link WebContents} object.
     */
    @CalledByNative
    private void loadSplashScreenNativeTabContents(WebContents splashWebContents) {
        if (splashWebContents == null || mContentViewRenderView == null) return;
        mSplashScreenWebContents = splashWebContents;
        splashWebContents.setDelegates(
                "", mViewAndroidDelegate, null, mWindow, WebContents.createDefaultInternalsHolder());
        if (mIsActivityVisible) {
            splashWebContents.updateWebContentsVisibility(Visibility.VISIBLE);
        }
        mContentViewRenderView.setCurrentWebContents(splashWebContents);
    }

    /**
     * Update native contents.
     * @param webContents A {@link WebContents} object.
     */
    @CalledByNative
    private void updateNativeTabContents(WebContents webContents) {
        if (webContents == null || mContentViewRenderView == null) return;
        mWebContents = webContents;
        mSplashScreenWebContents = null;
        mNavigationController = mWebContents.getNavigationController();
        if (mIsActivityVisible) {
            mWebContents.updateWebContentsVisibility(Visibility.VISIBLE);
        }
        mContentViewRenderView.setCurrentWebContents(mWebContents);
    }

    public void onActivityVisible(boolean visible) {
        mIsActivityVisible = visible;
        if (mWebContents != null) {
            if (visible) {
                mWebContents.updateWebContentsVisibility(Visibility.VISIBLE);
            } else {
                mWebContents.updateWebContentsVisibility(Visibility.HIDDEN);
            }
        }
    }

    @CalledByNative
    public void setOverlayMode(boolean useOverlayMode) {
        assumeNonNull(mContentViewRenderView).setOverlayVideoMode(useOverlayMode);
        if (mOverlayModeChangedCallbackForTesting != null) {
            mOverlayModeChangedCallbackForTesting.onResult(useOverlayMode);
        }
    }

    public void setOverayModeChangedCallbackForTesting(Callback<Boolean> callback) {
        mOverlayModeChangedCallbackForTesting = callback;
        ResettersForTesting.register(() -> mOverlayModeChangedCallbackForTesting = null);
    }

    /**
     * Enable/Disable navigation(Prev/Next) button if navigation is allowed/disallowed
     * in respective direction.
     * @param controlId Id of button to update
     * @param enabled enable/disable value
     */
    @CalledByNative
    private void enableUiControl(int controlId, boolean enabled) {}

     /**
     * @return The {@link WebContents} currently managing the content shown by this Shell.
     */
    public WebContents getWebContents() {
        return mWebContents;
    }

    @NativeMethods
    interface Natives {
        void loadSplashScreenWebContents(long shellPtr);
        void closeShell(long shellPtr);
    }
}
