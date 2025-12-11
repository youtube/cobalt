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

import android.content.Context;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Copied from org.chromium.content_shell.ShellManager.
 * Container and generator of ShellViews.
 */
@JNINamespace("content")
public class ShellManager {
    private static final String TAG = "cobalt";
    public static final String DEFAULT_SHELL_URL = "http://www.google.com";
    private WindowAndroid mWindow;
    private Shell mActiveShell;
    private Shell mSplashShell;
    private Shell mAppShell;

    private String mStartupUrl = DEFAULT_SHELL_URL;

    private Shell.OnWebContentsReadyListener mNextWebContentsReadyListener;

    // The target for all content rendering.
    private ContentViewRenderView mContentViewRenderView;

    private Context mContext;

    /**
     * Constructor for inflating via XML.
     */
    public ShellManager(final Context context) {
        mContext = context;
        if (sNatives == null) {
            sNatives = ShellManagerJni.get();
        }
        sNatives.init(this);
    }

    public Context getContext() {
        return mContext;
    }

    /**
     * @param window The window used to generate all shells.
     */
    public void setWindow(WindowAndroid window) {
        assert window != null;
        mWindow = window;
        mContentViewRenderView = new ContentViewRenderView(getContext());
        mContentViewRenderView.onNativeLibraryLoaded(window);
    }

    /**
     * @return The window used to generate all shells.
     */
    public WindowAndroid getWindow() {
        return mWindow;
    }

    /**
     * Get the ContentViewRenderView.
     */
    public ContentViewRenderView getContentViewRenderView() {
        return mContentViewRenderView;
    }

    /**
     * Sets the startup URL for new shell windows.
     */
    public void setStartupUrl(String url) {
        mStartupUrl = url;
    }

    /**
     * @return The currently visible shell view or null if one is not showing.
     */
    public Shell getActiveShell() {
        return mSplashShell == null? mAppShell : mSplashShell;
    }

    /**
     * @return The current Splash shell.
     */
    public Shell getSplashAppShell() {
        return mSplashShell;
    }

    /**
     * @return The current App shell.
     */
    public Shell getAppShell() {
        return mAppShell;
    }

    /**
     * Creates a new shell pointing to the specified URL.
     * @param url The URL the shell should load upon creation.
     */
    public void launchShell(String url) {
        // Calls the overloaded method with a null listener.
        launchShell(url, null);
    }

    /**
     * Creates a new shell pointing to the specified URL.
     * @param url The URL the shell should load upon creation.
     * @param listener The listener to be notified when WebContents is ready.
     */
    public void launchShell(String url, Shell.OnWebContentsReadyListener listener) {
        ThreadUtils.assertOnUiThread();
        mNextWebContentsReadyListener = listener;
        Shell previousShell = mActiveShell;
        sNatives.launchShell(url);
    }

    @CalledByNative
    private Object createShell(long nativeShellPtr) {
        if (mContentViewRenderView == null) {
            mContentViewRenderView = new ContentViewRenderView(getContext());
            mContentViewRenderView.onNativeLibraryLoaded(mWindow);
        }

        Shell shellView = new Shell(getContext());
        shellView.initialize(nativeShellPtr, mWindow);
        shellView.setWebContentsReadyListener(mNextWebContentsReadyListener);
        mNextWebContentsReadyListener = null;

        if (mActiveShell == null) {
            mSplashShell = shellView;
            showSplashShell();
        } else {
            mAppShell = shellView;
        }
        return shellView;
    }

    private void showShell(Shell shellView) {
        if (mActiveShell != null) {
            mActiveShell.setContentViewRenderView(null);
        }
        shellView.setContentViewRenderView(mContentViewRenderView);
        mActiveShell = shellView;
        WebContents webContents = mActiveShell.getWebContents();
        if (webContents != null) {
            mContentViewRenderView.setCurrentWebContents(webContents);
            webContents.onShow();
        }
    }

    private void showSplashShell() {
        // mActiveShell will be mSplashShell.
        showShell(mSplashShell);
    }

    public void showAppShell() {
        // mActiveShell will be mAppShell, and close mSplashShell.
        if (mSplashShell != null) {
            mSplashShell.close();
        }
        showShell(mAppShell);
        mSplashShell = null;
    }

    @CalledByNative
    private void removeShell(Shell shellView) {
        if (shellView == mActiveShell) mActiveShell = null;
        shellView.setContentViewRenderView(null);
    }

    /**
     * Destroys the Shell manager and associated components.
     * Always called at activity exit, and potentially called by native in cases where we need to
     * control the timing of mContentViewRenderView destruction. Must handle being called twice.
     */
    @CalledByNative
    public void destroy() {
        // Remove active shell (Currently single shell support only available).
        if (mActiveShell != null) {
            removeShell(mActiveShell);
        }
        if (mContentViewRenderView != null) {
            mContentViewRenderView.destroy();
            mContentViewRenderView = null;
        }
    }

    private static Natives sNatives;

    public static void setNativesForTesting(Natives natives) {
        sNatives = natives;
    }

    /**
     * Interface for the native implementation of ShellManager.
     */
    @NativeMethods
    public interface Natives {
        /**
         * Creates the native ShellManager object.
         * @param shellManagerInstance The Java instance of the ShellManager.
         */
        void init(Object shellManagerInstance);
        /**
         * Creates a new shell pointing to the specified URL.
         * @param url The URL the shell should load upon creation.
         */
        void launchShell(String url);
    }
}
