// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.content.Context;
import android.graphics.Rect;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

// Cobalt's own Java implementation of Shell. It does not have Shell UI.

/**
 * Container for the various UI components that make up a shell window.
 */
@JNINamespace("content")
public class Shell extends LinearLayout {

    private static final long COMPLETED_PROGRESS_TIMEOUT_MS = 200;

    // Stylus handwriting: Setting this ime option instructs stylus writing service to restrict
    // capturing writing events slightly outside the Url bar area. This is needed to prevent stylus
    // handwriting in inputs in web content area that are very close to url bar area, from being
    // committed to Url bar's Edit text. Ex: google.com search field.
    private static final String IME_OPTION_RESTRICT_STYLUS_WRITING_AREA =
            "restrictDirectWritingArea=true";

    private WebContents mWebContents;
    private NavigationController mNavigationController;

    private long mNativeShell;
    private ContentViewRenderView mContentViewRenderView;
    private WindowAndroid mWindow;
    private ShellViewAndroidDelegate mViewAndroidDelegate;

    private boolean mLoading;
    private boolean mIsFullscreen;

    private Callback<Boolean> mOverlayModeChangedCallbackForTesting;

    /**
     * Constructor for inflating via XML.
     */
    public Shell(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set the SurfaceView being rendered to as soon as it is available.
     */
    public void setContentViewRenderView(ContentViewRenderView contentViewRenderView) {
        FrameLayout contentViewHolder = (FrameLayout) findViewById(R.id.contentview_holder);
        if (contentViewRenderView == null) {
            if (mContentViewRenderView != null) {
                contentViewHolder.removeView(mContentViewRenderView);
            }
        } else {
            contentViewHolder.addView(contentViewRenderView,
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

    @CalledByNative
    private void onNativeDestroyed() {
        mWindow = null;
        mNativeShell = 0;
        mWebContents = null;
    }

    /**
     * @return Whether the Shell has been destroyed.
     * @see #onNativeDestroyed()
     */
    public boolean isDestroyed() {
        return mNativeShell == 0;
    }

    /**
     * @return Whether or not the Shell is loading content.
     */
    public boolean isLoading() {
        return mLoading;
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

    @SuppressWarnings("unused")
    @CalledByNative
    private void onUpdateUrl(String url) {}

    @SuppressWarnings("unused")
    @CalledByNative
    private void onLoadProgressChanged(double progress) {}

    @CalledByNative
    private void toggleFullscreenModeForTab(boolean enterFullscreen) {
    }

    @CalledByNative
    private boolean isFullscreenForTabOrPending() {
        return mIsFullscreen;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void setIsLoading(boolean loading) {
        mLoading = loading;
    }

    public ShellViewAndroidDelegate getViewAndroidDelegate() {
        return mViewAndroidDelegate;
    }

    /**
     * Initializes the ContentView based on the native tab contents pointer passed in.
     * @param webContents A {@link WebContents} object.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void initFromNativeTabContents(WebContents webContents) {
        Context context = getContext();
        ContentView cv =
                ContentView.createContentView(context, null /* eventOffsetHandler */, webContents);
        mViewAndroidDelegate = new ShellViewAndroidDelegate(cv);
        assert (mWebContents != webContents);
        if (mWebContents != null) mWebContents.clearNativeReference();
        webContents.initialize(
                "", mViewAndroidDelegate, cv, mWindow, WebContents.createDefaultInternalsHolder());
        mWebContents = webContents;
        SelectionPopupController.fromWebContents(webContents)
                .setActionModeCallback(defaultActionCallback());
        mNavigationController = mWebContents.getNavigationController();
        if (getParent() != null) mWebContents.onShow();
        ((FrameLayout) findViewById(R.id.contentview_holder)).addView(cv,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        cv.requestFocus();
        mContentViewRenderView.setCurrentWebContents(mWebContents);
    }

    /**
     * {link @ActionMode.Callback} that uses the default implementation in
     * {@link SelectionPopupController}.
     */
    private ActionMode.Callback2 defaultActionCallback() {
        final ActionModeCallbackHelper helper =
                SelectionPopupController.fromWebContents(mWebContents)
                        .getActionModeCallbackHelper();

        return new ActionMode.Callback2() {
            @Override
            public boolean onCreateActionMode(ActionMode mode, Menu menu) {
                helper.onCreateActionMode(mode, menu);
                return true;
            }

            @Override
            public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
                return helper.onPrepareActionMode(mode, menu);
            }

            @Override
            public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
                return helper.onActionItemClicked(mode, item);
            }

            @Override
            public void onDestroyActionMode(ActionMode mode) {
                helper.onDestroyActionMode();
            }

            @Override
            public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
                helper.onGetContentRect(mode, view, outRect);
            }
        };
    }

    @CalledByNative
    public void setOverlayMode(boolean useOverlayMode) {
        mContentViewRenderView.setOverlayVideoMode(useOverlayMode);
        if (mOverlayModeChangedCallbackForTesting != null) {
            mOverlayModeChangedCallbackForTesting.onResult(useOverlayMode);
        }
    }

    public void setOverayModeChangedCallbackForTesting(Callback<Boolean> callback) {
        mOverlayModeChangedCallbackForTesting = callback;
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
     * @return The {@link View} currently shown by this Shell.
     */
    public View getContentView() {
        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
        return viewDelegate != null ? viewDelegate.getContainerView() : null;
    }

     /**
     * @return The {@link WebContents} currently managing the content shown by this Shell.
     */
    public WebContents getWebContents() {
        return mWebContents;
    }

    @NativeMethods
    interface Natives {
        void closeShell(long shellPtr);
    }
}
