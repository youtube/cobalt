// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.ui.util.XrUtils;

/** Class to observe the layout change on an XR device to initiate and end spatialization. */
@NullMarked
public class XrLayoutStateObserver {
    private static final String TAG = "XrLayoutObserver";
    private static final int XR_SYSUI_FADING_TIME_MS = 30;

    private final Activity mActivity;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final ViewGroup mHubRootView;
    @Nullable private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;
    @Nullable private LayoutStateProvider mLayoutStateProvider;
    @Nullable private ViewGroup mControlContainer;
    @Nullable private ViewGroup mContentContainer;
    @Nullable private View mToolbarContainerView;

    /**
     * Creates the {@link XrLayoutStateObserver} instance.
     *
     * @param activity ChromeTabbedActivity used to determine when the activity layout is changed so
     *     that spatialization can be performed on an XR device.
     * @param layoutStateProviderSupplier Supplier of the {@link LayoutStateProvider}.
     * @param callbackController Handle to {@link CallbackController} to setup layout observer.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param hubLayoutDependencyHolder Handle to {@link HubLayoutDependencyHolder}
     */
    public XrLayoutStateObserver(
            Activity activity,
            OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderSupplier,
            CallbackController callbackController,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            ViewGroup hubRootView) {

        mActivity = activity;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mHubRootView = hubRootView;

        layoutStateProviderSupplier.onAvailable(
                callbackController.makeCancelable(this::setLayoutStateProvider));
    }

    public void destroy() {
        if (mLayoutStateProvider != null && mLayoutStateObserver != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
    }

    public void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert layoutStateProvider != null;
        assert mLayoutStateProvider == null : "the mLayoutStateProvider should set at most once.";

        mLayoutStateProvider = layoutStateProvider;

        // Initiates the spatialization in XR using the transparent tab hub view when the browsing
        // layout starts to hide and is switching to tab hub layout view.
        // Exists the spatilization when the tab-switcher layout view is being hidden and the switch
        // is to default browsing view.
        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        // These containers are populated during the post inflation startup hence
                        // delayed the initialization just before first use.
                        if (mContentContainer == null) {
                            mContentContainer = mActivity.findViewById(android.R.id.content);
                        }
                        if (mControlContainer == null) {
                            mControlContainer = mActivity.findViewById(R.id.control_container);
                        }
                        if (mToolbarContainerView == null) {
                            mToolbarContainerView = mActivity.findViewById(R.id.toolbar_container);
                        }

                        if (layoutType == LayoutType.BROWSING) {
                            // Start spatialization.
                            beginSpatialization();
                        } else if (layoutType == LayoutType.TAB_SWITCHER) {
                            // End spatialization.
                            endSpatialization();
                        }
                    }
                };
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void beginSpatialization() {
        assert mCompositorViewHolderSupplier.hasValue();
        Log.i(TAG, "SPA beginSpatialization");
        XrUtils.getInstance().viewInFullSpaceMode();
        mActivity.getWindow().getDecorView().setVisibility(View.INVISIBLE);
        showToolbar(false);
        ThreadUtils.postOnUiThreadDelayed(
                () -> {
                    mCompositorViewHolderSupplier
                            .get()
                            .getCompositorView()
                            .setXrFullSpaceMode(true);
                    mHubRootView.setVisibility(View.VISIBLE);
                    mActivity.getWindow().getDecorView().setVisibility(View.VISIBLE);
                },
                XR_SYSUI_FADING_TIME_MS);
    }

    private void endSpatialization() {
        assert mCompositorViewHolderSupplier.hasValue();
        Log.i(TAG, "SPA endSpatialization");
        XrUtils.getInstance().viewInHomeSpaceMode();
        ThreadUtils.postOnUiThreadDelayed(
                () -> {
                    mCompositorViewHolderSupplier
                            .get()
                            .getCompositorView()
                            .setXrFullSpaceMode(false);
                    ThreadUtils.postOnUiThreadDelayed(
                            () -> {
                                showContentAndControlContainers(true);
                                showToolbar(true);
                            },
                            XR_SYSUI_FADING_TIME_MS);
                },
                XR_SYSUI_FADING_TIME_MS);
        mActivity
                .getWindow()
                .getDecorView()
                .setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        showContentAndControlContainers(false);
    }

    private void showToolbar(boolean show) {
        if (mToolbarContainerView != null) {
            mToolbarContainerView.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
        }
    }

    private void showContentAndControlContainers(boolean show) {
        if (mControlContainer != null) {
            mControlContainer.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
        }
        if (mContentContainer != null) {
            mContentContainer.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
        }
    }
}
