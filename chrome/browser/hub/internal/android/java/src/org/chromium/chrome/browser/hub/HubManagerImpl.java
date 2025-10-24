// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout.LayoutParams;

import androidx.annotation.ColorInt;

import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.ui.util.TokenHolder;

/**
 * Implementation of {@link HubManager} and {@link HubController}.
 *
 * <p>This class holds all the dependencies of {@link HubCoordinator} so that the Hub UI can be
 * created and torn down as needed when {@link HubLayout} visibility changes.
 */
@NullMarked
public class HubManagerImpl implements HubManager, HubController {
    private final ValueChangedCallback<Pane> mOnFocusedPaneChanged =
            new ValueChangedCallback<>(this::onFocusedPaneChanged);
    private final ObservableSupplierImpl<Boolean> mHubVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final Activity mActivity;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final PaneManagerImpl mPaneManager;
    private final HubContainerView mHubContainerView;
    private final BackPressManager mBackPressManager;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final SnackbarManager mSnackbarManager;
    private final ObservableSupplier<Tab> mTabSupplier;
    private final MenuButtonCoordinator mMenuButtonCoordinator;
    private final HubShowPaneHelper mHubShowPaneHelper;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final SearchActivityClient mSearchActivityClient;
    private final HubColorMixer mHubColorMixer;

    // This is effectively NonNull and final once the HubLayout is initialized.
    private @MonotonicNonNull HubLayoutController mHubLayoutController;
    private @Nullable HubCoordinator mHubCoordinator;
    private int mSnackbarOverrideToken;
    private int mStatusIndicatorHeight;
    private int mAppHeaderHeight;

    /** See {@link HubManagerFactory#createHubManager}. */
    public HubManagerImpl(
            Activity activity,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            PaneListBuilder paneListBuilder,
            BackPressManager backPressManager,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            SnackbarManager snackbarManager,
            ObservableSupplier<Tab> tabSupplier,
            MenuButtonCoordinator menuButtonCoordinator,
            HubShowPaneHelper hubShowPaneHelper,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            SearchActivityClient searchActivityClient) {
        mActivity = activity;
        mProfileProviderSupplier = profileProviderSupplier;
        mPaneManager = new PaneManagerImpl(paneListBuilder, mHubVisibilitySupplier);
        mBackPressManager = backPressManager;
        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mSnackbarManager = snackbarManager;
        mTabSupplier = tabSupplier;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mHubShowPaneHelper = hubShowPaneHelper;
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mSearchActivityClient = searchActivityClient;

        // TODO(crbug.com/40283238): Consider making this a xml file so the entire core UI is
        // inflated.
        mHubContainerView = new HubContainerView(mActivity);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mHubContainerView.setLayoutParams(params);

        mPaneManager.getFocusedPaneSupplier().addObserver(mOnFocusedPaneChanged);
        mHubColorMixer =
                new HubColorMixerImpl(
                        mActivity, mHubVisibilitySupplier, mPaneManager.getFocusedPaneSupplier());
    }

    @Override
    public void destroy() {
        mHubVisibilitySupplier.set(false);
        mPaneManager.getFocusedPaneSupplier().removeObserver(mOnFocusedPaneChanged);
        mPaneManager.destroy();
        mHubColorMixer.destroy();
        destroyHubCoordinator();
    }

    @Override
    public PaneManager getPaneManager() {
        return mPaneManager;
    }

    @Override
    public HubController getHubController() {
        return this;
    }

    @Override
    public ObservableSupplier<Boolean> getHubVisibilitySupplier() {
        return mHubVisibilitySupplier;
    }

    @Override
    public HubShowPaneHelper getHubShowPaneHelper() {
        return mHubShowPaneHelper;
    }

    @Override
    public void setStatusIndicatorHeight(int height) {
        LayoutParams params = (LayoutParams) mHubContainerView.getLayoutParams();
        assert params != null : "HubContainerView should always have layout params.";
        mStatusIndicatorHeight = height;
        params.topMargin = mStatusIndicatorHeight + mAppHeaderHeight;
        mHubContainerView.setLayoutParams(params);
    }

    @Override
    public void setAppHeaderHeight(int height) {
        if (mAppHeaderHeight == height) return;
        LayoutParams params = (LayoutParams) mHubContainerView.getLayoutParams();
        assert params != null : "HubContainerView should always have layout params.";
        mAppHeaderHeight = height;
        params.topMargin = mStatusIndicatorHeight + mAppHeaderHeight;
        mHubContainerView.setLayoutParams(params);
    }

    @Override
    public ObservableSupplier<Integer> getHubOverviewColorSupplier() {
        return mHubColorMixer.getOverviewColorSupplier();
    }

    @Override
    public void setHubLayoutController(HubLayoutController hubLayoutController) {
        assert mHubLayoutController == null : "setHubLayoutController should only be called once.";
        mHubLayoutController = hubLayoutController;
    }

    @Override
    public HubContainerView getContainerView() {
        assert mHubCoordinator != null : "Access of a HubContainerView with no descendants.";
        return mHubContainerView;
    }

    @Override
    public @Nullable View getPaneHostView() {
        assert mHubCoordinator != null : "Access of a Hub pane host view that doesn't exist";
        return mHubContainerView.findViewById(R.id.hub_pane_host);
    }

    @Override
    public @ColorInt int getBackgroundColor(@Nullable Pane pane) {
        @HubColorScheme int colorScheme = HubColors.getColorSchemeSafe(pane);
        return HubColors.getBackgroundColor(mActivity, colorScheme);
    }

    @Override
    public void onHubLayoutShow() {
        mHubVisibilitySupplier.set(true);
        ensureHubCoordinatorIsInitialized();
    }

    @Override
    public void onHubLayoutDoneHiding() {
        // TODO(crbug.com/40283238): Consider deferring this destruction till after a timeout.
        mHubContainerView.removeAllViews();
        mHubVisibilitySupplier.set(false);
        destroyHubCoordinator();
    }

    @Override
    public boolean onHubLayoutBackPressed() {
        if (mHubCoordinator == null) return false;

        switch (mHubCoordinator.handleBackPress()) {
            case BackPressResult.SUCCESS:
                return true;
            case BackPressResult.FAILURE:
                return false;
            default:
                assert false : "Not reached.";
                return false;
        }
    }

    @Override
    public HubColorMixer getHubColorMixer() {
        return mHubColorMixer;
    }

    private void ensureHubCoordinatorIsInitialized() {
        if (mHubCoordinator != null) return;

        assert mHubLayoutController != null
                : "HubLayoutController should be set before creating HubCoordinator.";

        mHubCoordinator =
                new HubCoordinator(
                        mActivity,
                        mProfileProviderSupplier,
                        mHubContainerView,
                        mPaneManager,
                        mHubLayoutController,
                        mTabSupplier,
                        mMenuButtonCoordinator,
                        mSearchActivityClient,
                        mEdgeToEdgeSupplier,
                        mHubColorMixer);
        mBackPressManager.addHandler(mHubCoordinator, BackPressHandler.Type.HUB);
        Pane pane = mPaneManager.getFocusedPaneSupplier().get();
        attachPaneDependencies(pane);
    }

    private void destroyHubCoordinator() {
        if (mHubCoordinator != null) {
            Pane pane = mPaneManager.getFocusedPaneSupplier().get();
            detachPaneDependencies(pane);

            mBackPressManager.removeHandler(mHubCoordinator);
            mHubCoordinator.destroy();
            mHubCoordinator = null;
        }
    }

    @Nullable HubCoordinator getHubCoordinatorForTesting() {
        return mHubCoordinator;
    }

    private void onFocusedPaneChanged(@Nullable Pane newPane, @Nullable Pane oldPane) {
        detachPaneDependencies(oldPane);
        if (mHubCoordinator != null) {
            attachPaneDependencies(newPane);
        }
    }

    private void detachPaneDependencies(@Nullable Pane pane) {
        if (pane == null) return;

        pane.setPaneHubController(null);
        MenuOrKeyboardActionHandler menuOrKeyboardActionHandler =
                pane.getMenuOrKeyboardActionHandler();
        if (menuOrKeyboardActionHandler != null) {
            mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                    menuOrKeyboardActionHandler);
        }
        if (mSnackbarOverrideToken != TokenHolder.INVALID_TOKEN) {
            mSnackbarManager.popParentViewFromOverrideStack(mSnackbarOverrideToken);
            mSnackbarOverrideToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void attachPaneDependencies(@Nullable Pane pane) {
        if (pane == null) return;
        assumeNonNull(mHubCoordinator);

        pane.setPaneHubController(mHubCoordinator);
        MenuOrKeyboardActionHandler menuOrKeyboardActionHandler =
                pane.getMenuOrKeyboardActionHandler();
        if (menuOrKeyboardActionHandler != null) {
            mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                    menuOrKeyboardActionHandler);
        }
        mSnackbarOverrideToken =
                mSnackbarManager.pushParentViewToOverrideStack(
                        mHubCoordinator.getSnackbarContainer());
    }
}
