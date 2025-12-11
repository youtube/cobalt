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
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import org.chromium.base.ThreadUtils;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Copied from org.chromium.content_shell.ShellManager.
 * Container and generator of ShellViews.
 */
@JNINamespace("content")
public class ShellManager extends FrameLayout {
    private static final String TAG = "cobalt";
    public static final String DEFAULT_SHELL_URL = "http://www.google.com";
    private WindowAndroid mWindow;
    private Shell mActiveShell;

    private String mStartupUrl = DEFAULT_SHELL_URL;

    private Shell.OnWebContentsReadyListener mNextWebContentsReadyListener;

    // The target for all content rendering.
    private ContentViewRenderView mContentViewRenderView;

    /**
     * Constructor for inflating via XML.
     */
    public ShellManager(final Context context, AttributeSet attrs) {
        super(context, attrs);
        if (sNatives == null) {
            sNatives = ShellManagerJni.get();
        }
        sNatives.init(this);
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
        return mActiveShell;
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
        if (previousShell != null) previousShell.close();
    }

    @CalledByNative
    private Object createShell(long nativeShellPtr) {
        if (mContentViewRenderView == null) {
            mContentViewRenderView = new ContentViewRenderView(getContext());
            mContentViewRenderView.onNativeLibraryLoaded(mWindow);
        }

        LayoutInflater inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        Shell shellView = (Shell) inflater.inflate(R.layout.shell_view, null);
        shellView.initialize(nativeShellPtr, mWindow);
        shellView.setWebContentsReadyListener(mNextWebContentsReadyListener);
        mNextWebContentsReadyListener = null;

        // TODO(tedchoc): Allow switching back to these inactive shells.
        if (mActiveShell != null) removeShell(mActiveShell);

        showShell(shellView);
        return shellView;
    }

    private void showShell(Shell shellView) {
        shellView.setContentViewRenderView(mContentViewRenderView);
        addView(shellView,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        mActiveShell = shellView;
        WebContents webContents = mActiveShell.getWebContents();
        if (webContents != null) {
            mContentViewRenderView.setCurrentWebContents(webContents);
            webContents.updateWebContentsVisibility(Visibility.VISIBLE);
        }
    }

    @CalledByNative
    private void removeShell(Shell shellView) {
        if (shellView == mActiveShell) mActiveShell = null;
        if (shellView.getParent() == null) return;
        shellView.setContentViewRenderView(null);
        removeView(shellView);
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
