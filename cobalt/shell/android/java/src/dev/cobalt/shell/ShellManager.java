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
import android.graphics.Color;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.Callable;

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

    private String mStartupUrl = DEFAULT_SHELL_URL;

    // The target for all content rendering.
    private ContentViewRenderView mContentViewRenderView;

    private Context mContext;

    /**
     * Constructor for inflating via XML.
     */
    public ShellManager(final Context context) {
        mContext = context;
        ShellManagerJni.get().init(this);
    }

    public Context getContext() {
        return mContext;
    }

    /**
     * @param window The window used to generate all shells.
     */
    public void setWindow(final WindowAndroid window) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            assert window != null;
            mWindow = window;
            mContentViewRenderView = new ContentViewRenderView(getContext());
            mContentViewRenderView.onNativeLibraryLoaded(window);
        });
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
        ThreadUtils.runOnUiThread(() -> {
            Shell previousShell = mActiveShell;
            ShellManagerJni.get().launchShell(url);
            if (previousShell != null) previousShell.close();
        });
    }

    @CalledByNative
    private Object createShell(final long nativeShellPtr) {
        return ThreadUtils.runOnUiThreadBlockingNoException((Callable<Object>) () -> {
            if (mContentViewRenderView == null) {
                mContentViewRenderView = new ContentViewRenderView(getContext());
                mContentViewRenderView.setSurfaceViewBackgroundColor(Color.TRANSPARENT);
                mContentViewRenderView.onNativeLibraryLoaded(mWindow);
            }

            Shell shellView = new Shell(getContext());
            shellView.initialize(nativeShellPtr, mWindow);

            // TODO(tedchoc): Allow switching back to these inactive shells.
            if (mActiveShell != null) removeShell(mActiveShell);

            showShell(shellView);
            return shellView;
        });
    }

    private void showShell(Shell shellView) {
        shellView.setContentViewRenderView(mContentViewRenderView);
        mActiveShell = shellView;
        WebContents webContents = mActiveShell.getWebContents();
        if (webContents != null) {
            mContentViewRenderView.setCurrentWebContents(webContents);
            webContents.onShow();
        }
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
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Remove active shell (Currently single shell support only available).
            if (mActiveShell != null) {
                removeShell(mActiveShell);
            }
            if (mContentViewRenderView != null) {
                mContentViewRenderView.destroy();
                mContentViewRenderView = null;
            }
        });
    }

    @NativeMethods
    interface Natives {
        void init(Object shellManagerInstance);
        void launchShell(String url);
    }
}
