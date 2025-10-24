// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_EXPAND_NEW_TAB_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_TIMEOUT_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_TRANSLATE_DURATION_MS;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.CallSuper;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.common.base.Function;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.scene_layer.SolidColorSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.util.XrUtils;

import java.util.Collections;
import java.util.function.DoubleConsumer;

/**
 * A {@link Layout} for Hub that has an empty or single tab {@link SceneLayer}. Android UI for a
 * toolbar and panes will be rendered atop this layout.
 *
 * <p>This implementation is a heavily modified fork of {@link TabSwitcherLayout} that will delegate
 * animations to the current pane.
 *
 * <p>Normally, this layout will show an empty {@link SceneLayer}. However, to facilitate thumbnail
 * capture and animations it may transiently host a {@link StaticTabSceneLayer}.
 */
public class HubLayout extends Layout implements HubLayoutController, AppHeaderObserver {
    /**
     * Implementation of {@link HubLayoutAnimationListener} that updates an {@link
     * ObservableSupplier<Boolean>} to reflect the animation state.
     */
    @VisibleForTesting
    static class HubLayoutAnimationListenerImpl implements HubLayoutAnimationListener {
        private final @NonNull ObservableSupplierImpl<Boolean> mIsAnimatingSupplier;

        /**
         * Constructs a listener that updates the given supplier.
         *
         * @param isAnimatingSupplier The supplier to update with the animation state.
         */
        public HubLayoutAnimationListenerImpl(
                @NonNull ObservableSupplierImpl<Boolean> isAnimatingSupplier) {
            mIsAnimatingSupplier = isAnimatingSupplier;
        }

        @Override
        @CallSuper
        public void onStart() {
            mIsAnimatingSupplier.set(true);
        }

        @Override
        @CallSuper
        public void onEnd(boolean wasForcedToFinish) {
            mIsAnimatingSupplier.set(false);
        }
    }

    private final @NonNull Callback<Pane> mOnPaneFocused = this::updateEmptyLayerColor;
    private final @NonNull Function<HubLayoutAnimatorProvider, HubLayoutAnimationRunner>
            mHubLayoutAnimationRunnerFactory;
    private final @NonNull LayoutStateProvider mLayoutStateProvider;
    private final @NonNull ViewGroup mRootView;
    private final @NonNull HubManager mHubManager;
    private final @NonNull HubController mHubController;
    private final @NonNull PaneManager mPaneManager;
    private final @NonNull HubLayoutScrimController mScrimController;
    private final @NonNull DoubleConsumer mOnToolbarAlphaChange;
    private final @NonNull HubShowPaneHelper mHubShowPaneHelper;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;

    /**
     * The previous {@link LayoutType}, valid between {@link #show(long, boolean)} and {@link
     * #doneShowing()}.
     */
    private final @NonNull ObservableSupplierImpl<Integer> mPreviousLayoutTypeSupplier =
            new ObservableSupplierImpl<>();

    private final @NonNull ObservableSupplierImpl<Boolean> mIsAnimatingSupplier =
            new ObservableSupplierImpl<>(false);

    private @Nullable SceneLayer mCurrentSceneLayer;

    private boolean mFullyShown;

    /** Scene layer to facilitate thumbnail capture prior to starting a transition animation. */
    private @Nullable StaticTabSceneLayer mTabSceneLayer;

    /** An empty scene layer used to avoid drawing anything. */
    private @Nullable SolidColorSceneLayer mEmptySceneLayer;

    private @Nullable HubLayoutAnimationRunner mCurrentAnimationRunner;

    /**
     * Create the {@link Layout} to show the Hub on.
     *
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} for the {@link LayoutManager}.
     * @param renderHost The {@link LayoutRenderHost} for the {@link LayoutManager}.
     * @param layoutStateProvider The {@link LayoutStateProvider} for the {@link LayoutManager}.
     * @param dependencyHolder The {@link HubLayoutDependencyHolder} that holds dependencies for
     *     HubLayout.
     * @param tabModelSelectorSupplier Supplier for an interface to talk to the Tab Model Selector.
     */
    public HubLayout(
            @NonNull Context context,
            @NonNull LayoutUpdateHost updateHost,
            @NonNull LayoutRenderHost renderHost,
            @NonNull LayoutStateProvider layoutStateProvider,
            @NonNull HubLayoutDependencyHolder dependencyHolder,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
        this(
                context,
                updateHost,
                renderHost,
                layoutStateProvider,
                dependencyHolder,
                tabModelSelectorSupplier,
                desktopWindowStateManager,
                HubLayoutAnimationRunnerFactory::createHubLayoutAnimationRunner);
    }

    @VisibleForTesting
    HubLayout(
            @NonNull Context context,
            @NonNull LayoutUpdateHost updateHost,
            @NonNull LayoutRenderHost renderHost,
            @NonNull LayoutStateProvider layoutStateProvider,
            @NonNull HubLayoutDependencyHolder dependencyHolder,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            @NonNull
                    Function<HubLayoutAnimatorProvider, HubLayoutAnimationRunner>
                            hubLayoutAnimationRunnerFactory) {
        super(context, updateHost, renderHost);
        mPreviousLayoutTypeSupplier.set(layoutStateProvider.getActiveLayoutType());
        mHubLayoutAnimationRunnerFactory = hubLayoutAnimationRunnerFactory;

        mLayoutStateProvider = layoutStateProvider;
        // This is the R.id.tab_switcher_view_holder. It is used by tablets for the tab switcher,
        // but in that case it is the animated object and is opaque. Here we will animate the
        // HubContainerView instead and just animate and just use this view as a transparent
        // container for z-indexing and geometry.
        // TODO(crbug.com/40288013): Consider removing the tab_switcher_view_holder post launch.
        mRootView = dependencyHolder.getHubRootView();
        mRootView.setBackgroundColor(Color.TRANSPARENT);

        mHubManager = dependencyHolder.getHubManager();
        mHubController = mHubManager.getHubController();
        mHubController.setHubLayoutController(this);
        mPaneManager = mHubManager.getPaneManager();
        mPaneManager.getFocusedPaneSupplier().addObserver(mOnPaneFocused);
        mHubShowPaneHelper = mHubManager.getHubShowPaneHelper();
        mScrimController = dependencyHolder.getScrimController();
        mOnToolbarAlphaChange = dependencyHolder.getOnOverviewAlphaChange();
        mTabModelSelector = tabModelSelectorSupplier.get();
        mDesktopWindowStateManager = desktopWindowStateManager;
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(this);
            maybeUpdateLayout();
        }
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        maybeUpdateLayout();
    }

    /** Returns the current {@link HubLayoutAnimationType}. */
    @HubLayoutAnimationType
    int getCurrentAnimationType() {
        return mCurrentAnimationRunner != null
                ? mCurrentAnimationRunner.getAnimationType()
                : HubLayoutAnimationType.NONE;
    }

    @Override
    public void selectTabAndHideHubLayout(@TabId int tabId) {
        TabModelUtils.selectTabById(mTabModelSelector, tabId, TabSelectionType.FROM_USER);
        startHiding();
    }

    @Override
    public ObservableSupplier<Integer> getPreviousLayoutTypeSupplier() {
        return mPreviousLayoutTypeSupplier;
    }

    @Override
    public void onFinishNativeInitialization() {
        ensureSceneLayersExist();
    }

    @Override
    public void setTabContentManager(TabContentManager tabContentManager) {
        super.setTabContentManager(tabContentManager);
        if (mTabSceneLayer != null && mTabContentManager != null) {
            mTabSceneLayer.setTabContentManager(mTabContentManager);
        }
    }

    @Override
    public void destroy() {
        if (mTabSceneLayer != null) {
            mTabSceneLayer.destroy();
            mTabSceneLayer = null;
        }
        if (mEmptySceneLayer != null) {
            mEmptySceneLayer.destroy();
            mEmptySceneLayer = null;
        }
        mCurrentSceneLayer = null;
        mPaneManager.getFocusedPaneSupplier().removeObserver(mOnPaneFocused);
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(this);
        }
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayersExist();
        if (!hasLayoutTab()) return;

        boolean needUpdate = updateSnap(dt, getLayoutTab());
        if (needUpdate) requestUpdate();
    }

    @Override
    public void contextChanged(Context context) {
        super.contextChanged(context);
        // This is called before show() and before getActiveLayoutType() changes so we can know what
        // layout the Hub is showing from.
        mPreviousLayoutTypeSupplier.set(mLayoutStateProvider.getActiveLayoutType());
    }

    @Override
    public @ViewportMode int getViewportMode() {
        // Hub has its own toolbar.
        // TODO(crbug.com/40283200): Confirm this doesn't cause the toolbar to disappear too early
        // or
        // without animation.
        return ViewportMode.ALWAYS_FULLSCREEN;
    }

    @Override
    public void show(long time, boolean animate) {
        if (isStartingToShow()) return;

        try (TraceEvent e = TraceEvent.scoped("HubLayout.show")) {
            super.show(time, animate);

            forceAnimationToFinish();

            Promise<Bitmap> bitmapPromise = new Promise<>();
            @LayoutType int previousLayoutType = mPreviousLayoutTypeSupplier.get();
            if (previousLayoutType == LayoutType.BROWSING) {
                final Tab currentTab = mTabModelSelector.getCurrentTab();
                createLayoutTabForTabId(getIdForTab(currentTab));
                mCurrentSceneLayer = mTabSceneLayer;
                captureTabThumbnail(currentTab, bitmapPromise);
            } else {
                mCurrentSceneLayer = mEmptySceneLayer;
                bitmapPromise.fulfill(null);
            }

            mPaneManager.focusPane(
                    mHubShowPaneHelper.consumeNextPaneId(mTabModelSelector.isIncognitoSelected()));

            mHubController.onHubLayoutShow();

            HubContainerView containerView = mHubController.getContainerView();
            HubLayoutAnimatorProvider animatorProvider = createShowAnimatorProvider(containerView);

            Callback<Bitmap> thumbnailCallback = animatorProvider.getThumbnailCallback();
            if (thumbnailCallback != null) {
                bitmapPromise.then(thumbnailCallback);
            }
            updateEmptyLayerColor(mPaneManager.getFocusedPaneSupplier().get());

            assert mCurrentAnimationRunner == null;
            mCurrentAnimationRunner = mHubLayoutAnimationRunnerFactory.apply(animatorProvider);
            mCurrentAnimationRunner.addListener(
                    new HubLayoutAnimationListenerImpl(mIsAnimatingSupplier) {
                        @Override
                        public void onEnd(boolean wasForcedToFinish) {
                            super.onEnd(wasForcedToFinish);
                            doneShowing();
                            if (!wasForcedToFinish) {
                                // We don't want to hide the tab if the animation was forced to
                                // finish since that means another layout is going to show and
                                // hiding the tab could leave the tab in a bad state.
                                hideCurrentTab();
                            }
                        }
                    });
            maybeAddPaneAnimationListener(mCurrentAnimationRunner);

            mRootView.setVisibility(View.VISIBLE);
            containerView.setVisibility(View.INVISIBLE);
            LayoutParams params = (LayoutParams) containerView.getLayoutParams();
            // TODO(crbug.com/41495991): Change this to an assert and fix any broken tests.
            if (params == null) {
                params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            }
            mRootView.addView(containerView, /* index= */ 0, params);

            if (!animate) {
                if (containerView.getHeight() != 0) {
                    // Don't post or wait for a layout as HubLayout is not in control of when the
                    // previous layout was hidden and this avoids a possibly empty frame.
                    forceShowLayout();
                } else {
                    containerView.runOnNextLayout(this::forceShowLayout);
                }
            } else {
                containerView.runOnNextLayout(this::queueAnimation);
            }
        }
    }

    @Override
    public void doneShowing() {
        try (TraceEvent e = TraceEvent.scoped("HubLayout.doneShowing")) {
            super.doneShowing();
            mCurrentSceneLayer = mEmptySceneLayer;
            mCurrentAnimationRunner = null;
            resetLayoutTabs(/* clearVisibleIds= */ true);
            // We are now fully shown as all animations are finished.
            mFullyShown = true;

            // This is a legacy value from the stack tab switcher, we are using at a proxy for Hub
            // shown. Prior to Hub this was recorded in when the TabSwitcherMediator finished
            // showing. However, this location is a better analog than TabSwitcherPaneMediator and
            // it keeps the call with doneHiding() symmetric.
            RecordUserAction.record("MobileToolbarShowStackView");
        }
    }

    @Override
    public void startHiding() {
        if (isStartingToHide()) return;

        try (TraceEvent e = TraceEvent.scoped("HubLayout.startHiding")) {
            // End spatialization, if active as the tab switcher is hiding on an XR device.
            // It hides the contents and root view temporarily before any transition to the browsing
            // layout takes place and before the notifications are sent to the observer(s).
            if (XrUtils.getInstance().isFsmOnXrDevice()) {
                HubContainerView containerView = mHubController.getContainerView();
                containerView.setVisibility(View.INVISIBLE);
                mRootView.setVisibility(View.INVISIBLE);
            }

            super.startHiding();

            // Since we are hiding this is no-longer fully shown.
            mFullyShown = false;

            // Use the EXPAND_NEW_TAB animation if it is already prepared.
            if (getCurrentAnimationType() == HubLayoutAnimationType.EXPAND_NEW_TAB) {
                PostTask.postTask(TaskTraits.UI_DEFAULT, this::queueAnimation);
                return;
            }

            forceAnimationToFinish();

            Tab currentTab = mTabModelSelector.getCurrentTab();
            if (currentTab != null && currentTab.isHidden()) {
                currentTab.show(TabSelectionType.FROM_USER, TabLoadIfNeededCaller.SET_TAB);
            }

            int tabId = mTabModelSelector.getCurrentTabId();
            @LayoutType int nextLayoutType = mLayoutStateProvider.getNextLayoutType();
            if (nextLayoutType == LayoutType.BROWSING) {
                // During fade and translate animations the composited scene layer is visible. At
                // the end of the animation a composited tab will be fully visible. To ensure
                // continuity during the animation create and show the mTabSceneLayer with the
                // LayoutTab for the tabId that will be shown once the animation finishes.
                createLayoutTabForTabId(tabId);
                mCurrentSceneLayer = mTabSceneLayer;
            } else {
                mCurrentSceneLayer = mEmptySceneLayer;
            }
            updateEmptyLayerColor(mPaneManager.getFocusedPaneSupplier().get());

            HubContainerView containerView = mHubController.getContainerView();
            HubLayoutAnimatorProvider animatorProvider = createHideAnimatorProvider(containerView);

            Callback<Bitmap> thumbnailCallback = animatorProvider.getThumbnailCallback();
            if (thumbnailCallback != null) {
                if (nextLayoutType == LayoutType.BROWSING
                        && mTabContentManager != null
                        && tabId != Tab.INVALID_TAB_ID) {
                    mTabContentManager.getEtc1TabThumbnailWithCallback(tabId, thumbnailCallback);
                } else {
                    thumbnailCallback.onResult(null);
                }
            }

            assert mCurrentAnimationRunner == null;
            mCurrentAnimationRunner = mHubLayoutAnimationRunnerFactory.apply(animatorProvider);
            mCurrentAnimationRunner.addListener(
                    new HubLayoutAnimationListenerImpl(mIsAnimatingSupplier) {
                        @Override
                        public void onEnd(boolean wasForcedToFinish) {
                            super.onEnd(wasForcedToFinish);
                            doneHiding();
                        }
                    });
            maybeAddPaneAnimationListener(mCurrentAnimationRunner);

            PostTask.postTask(TaskTraits.UI_DEFAULT, this::queueAnimation);
        }
    }

    @Override
    public void doneHiding() {
        try (TraceEvent e = TraceEvent.scoped("HubLayout.doneHiding")) {
            HubContainerView containerView = mHubController.getContainerView();
            containerView.setVisibility(View.INVISIBLE);
            mRootView.removeView(containerView);
            mRootView.setVisibility(View.GONE);
            mCurrentAnimationRunner = null;
            mHubController.onHubLayoutDoneHiding();

            mCurrentSceneLayer = mEmptySceneLayer;
            // Don't clear the visible ids because the next layout might have already updated them.
            resetLayoutTabs(/* clearVisibleIds= */ false);

            // This is defensive and probably is not necessary as it is set in startHiding; however,
            // to ensure the toolbar does not become broken also set this here.
            mFullyShown = false;

            // This is a legacy value from the stack tab switcher, we are using at a proxy for Hub
            // hidden.
            RecordUserAction.record("MobileExitStackView");

            // Do this last so the Hub is ready to show again.
            super.doneHiding();
        }
    }

    @Override
    protected void forceAnimationToFinish() {
        if (mCurrentAnimationRunner == null) return;

        // Immediately start any pending animations.
        mHubController.getContainerView().runOnNextLayoutRunnables();

        // #runOnNextLayoutRunnables() may have set this to null.
        if (mCurrentAnimationRunner != null) {
            // Force the animation to run to completion.
            mCurrentAnimationRunner.forceAnimationToFinish();
        }
        mCurrentAnimationRunner = null;

        if (mScrimController != null) {
            mScrimController.forceAnimationToFinish();
        }
    }

    @Override
    public boolean onBackPressed() {
        // This is for the legacy backpress handler which will soon be obsolete.
        return mHubController.onHubLayoutBackPressed();
    }

    @Override
    public void onTabCreated(
            long time,
            @TabId int tabId,
            int tabIndex,
            int sourceTabId,
            boolean newIsIncognito,
            boolean background,
            float originX,
            float originY) {
        // Background tab creation or creation while hiding does not trigger a Hub layout
        // transition.
        if (background || isStartingToHide()) return;

        HubContainerView containerView = mHubController.getContainerView();

        // Skip animation:
        // * If ContainerView is not laid out there will be no geometry for an animation.
        // * For LFF devices which don't have new tab animations in the tab switcher.
        if (!containerView.isLaidOut() || DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            selectTabAndHideHubLayout(tabId);
            return;
        }

        forceAnimationToFinish();

        mCurrentSceneLayer = mEmptySceneLayer;
        updateEmptyLayerColor(mPaneManager.getFocusedPaneSupplier().get());

        @ColorInt
        int backgroundColor = NewTabAnimationUtils.getBackgroundColor(getContext(), newIsIncognito);
        SyncOneshotSupplierImpl<ShrinkExpandAnimationData> animationDataSupplier =
                new SyncOneshotSupplierImpl<>();
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createNewTabAnimatorProvider(
                        mHubController.getContainerView(),
                        animationDataSupplier,
                        backgroundColor,
                        HUB_LAYOUT_EXPAND_NEW_TAB_DURATION_MS,
                        mOnToolbarAlphaChange);

        // TODO(crbug.com/40285429): Supply this from HubController so it can look like the
        // animation originated from wherever on the Hub was clicked. This defaults to the top
        // left/right of the pane host view.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        Rect finalRect = new Rect();
        getFinalRectForNewTabAnimation(containerView, newIsIncognito, finalRect);
        Rect initialRect;
        int cornerRadius;
        if (NewTabAnimationUtils.isNewTabAnimationEnabled()) {
            // Without this code, the upper corner shows a bit of blinking when running the
            // animation. This ensures the {@link ShrinkExpandImageView} fully covers the origin
            // corner.
            if (isRtl) {
                finalRect.right += 1;
            } else {
                finalRect.left -= 1;
            }
            finalRect.top -= 1;

            initialRect = new Rect();
            NewTabAnimationUtils.updateRects(
                    NewTabAnimationUtils.RectStart.TOP, isRtl, initialRect, finalRect);
            cornerRadius =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.new_tab_animation_rect_corner_radius);
        } else {
            cornerRadius = 0;
            int y = finalRect.top;
            int x;
            if (isRtl) {
                x = finalRect.right;
                initialRect = new Rect(x - 1, y, x, y + 1);
            } else {
                x = finalRect.left;
                initialRect = new Rect(x, y, x + 1, y + 1);
            }
        }
        animationDataSupplier.set(
                ShrinkExpandAnimationData.createHubNewTabAnimationData(
                        initialRect, finalRect, cornerRadius, /* useFallbackAnimation= */ false));

        assert mCurrentAnimationRunner == null;
        mCurrentAnimationRunner = mHubLayoutAnimationRunnerFactory.apply(animatorProvider);
        mCurrentAnimationRunner.addListener(
                new HubLayoutAnimationListenerImpl(mIsAnimatingSupplier) {
                    @Override
                    public void onEnd(boolean wasForcedToFinish) {
                        // TODO(crbug.com/40933120): Add fade animator.
                        super.onEnd(wasForcedToFinish);
                        doneHiding();
                    }
                });
        maybeAddPaneAnimationListener(mCurrentAnimationRunner);

        selectTabAndHideHubLayout(tabId);
    }

    @Override
    protected boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        // Return whether an animation is running. Ignore the inputs like TabSwitcherLayout.
        return mCurrentAnimationRunner != null;
    }

    @Override
    public boolean handlesTabClosing() {
        // Tabs can be closed from the Tab Switcher panes without changing layout.
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        // For the new tab animation.
        return true;
    }

    @Override
    public boolean canHostBeFocusable() {
        // TODO(crbug.com/40283200): Consider returning false here so that the omnibox doesn't steal
        // focus.
        return super.canHostBeFocusable();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mCurrentSceneLayer;
    }

    @Override
    protected void updateSceneLayer(
            RectF viewport,
            RectF contentViewport,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        ensureSceneLayersExist();

        if (mCurrentSceneLayer != mTabSceneLayer) return;

        LayoutTab layoutTab = getLayoutTab();
        layoutTab.set(LayoutTab.IS_ACTIVE_LAYOUT, isActive());
        layoutTab.set(LayoutTab.CONTENT_OFFSET, browserControls.getContentOffset());
        mTabSceneLayer.update(layoutTab);
    }

    @Override
    public boolean forceHideBrowserControlsAndroidView() {
        // Fixes being able to click the toolbar through the Hub on LFF devices see b/337616153.
        // This is not always `true` because it results in a visible flicker when exiting the Hub
        // into an NTP when using the expand animation.
        return mFullyShown;
    }

    @Override
    public @LayoutType int getLayoutType() {
        // Pretend to be the TAB_SWITCHER for initial development to minimize churn outside of
        // LayoutManager.
        return LayoutType.TAB_SWITCHER;
    }

    @Override
    public boolean isRunningAnimations() {
        return mCurrentAnimationRunner != null;
    }

    @Override
    public ObservableSupplier<Boolean> getIsAnimatingSupplier() {
        return mIsAnimatingSupplier;
    }

    // Visible for testing or spying

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    HubLayoutAnimatorProvider createShowAnimatorProvider(@NonNull HubContainerView containerView) {
        @Nullable Pane pane = mPaneManager.getFocusedPaneSupplier().get();

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            return TranslateHubLayoutAnimationFactory.createTranslateUpAnimatorProvider(
                    mHubController.getHubColorMixer(),
                    containerView,
                    mScrimController,
                    HUB_LAYOUT_TRANSLATE_DURATION_MS,
                    getContainerYOffset());
        } else if (pane == null) {
            return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                    containerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
        }
        return pane.createShowHubLayoutAnimatorProvider(containerView);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    HubLayoutAnimatorProvider createHideAnimatorProvider(@NonNull HubContainerView containerView) {
        @Nullable Pane pane = mPaneManager.getFocusedPaneSupplier().get();

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            return TranslateHubLayoutAnimationFactory.createTranslateDownAnimatorProvider(
                    mHubController.getHubColorMixer(),
                    containerView,
                    mScrimController,
                    HUB_LAYOUT_TRANSLATE_DURATION_MS,
                    getContainerYOffset());
        } else if (pane == null) {
            return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                    containerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
        }
        return pane.createHideHubLayoutAnimatorProvider(containerView);
    }

    /**
     * Updates the final {@link Rect} that will be used for the new foreground tab animation.
     *
     * @param containerView The {@link HubContainerView} that contains all the Hub UI.
     * @param newIsIncognito true if the new tab is an incognito tab.
     * @param finalRect The {@link Rect} that will get updated for the animation.
     */
    @VisibleForTesting
    void getFinalRectForNewTabAnimation(
            HubContainerView containerView, boolean newIsIncognito, Rect finalRect) {
        View paneHost = mHubController.getPaneHostView();
        assert paneHost.isLaidOut();
        paneHost.getGlobalVisibleRect(finalRect);
        Rect containerViewRect = new Rect();
        containerView.getGlobalVisibleRect(containerViewRect);

        if (mTabModelSelector.isIncognitoBrandedModelSelected() != newIsIncognito) {
            // Start from above the toolbar when switching models.
            finalRect.top = containerViewRect.top;
        } else if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
            // Account for the hub's search box container height.
            int searchBoxHeight =
                    HubUtils.getSearchBoxHeight(
                            containerView, R.id.hub_toolbar, R.id.toolbar_action_container);
            finalRect.offset(0, -searchBoxHeight);
            finalRect.bottom += searchBoxHeight;
        }
        // Ignore edge offset and just ensure the width is correct. See crbug.com/1502437.
        finalRect.offset(-finalRect.left, -containerViewRect.top);
    }

    private void forceShowLayout() {
        queueAnimation();
        forceAnimationToFinish();
        hideCurrentTab();
        maybeUpdateLayout();
    }

    private void maybeAddPaneAnimationListener(HubLayoutAnimationRunner animationRunner) {
        @Nullable Pane pane = mPaneManager.getFocusedPaneSupplier().get();
        if (pane == null) return;

        HubLayoutAnimationListener listener = pane.getHubLayoutAnimationListener();
        if (listener == null) return;

        animationRunner.addListener(listener);
    }

    private void queueAnimation() {
        if (mCurrentAnimationRunner == null) return;

        mCurrentAnimationRunner.runWithWaitForAnimatorTimeout(HUB_LAYOUT_TIMEOUT_MS);
    }

    private void ensureSceneLayersExist() {
        if (mTabSceneLayer == null) {
            mTabSceneLayer = new StaticTabSceneLayer();
            if (mTabContentManager != null) {
                mTabSceneLayer.setTabContentManager(mTabContentManager);
            }
        }
        if (mEmptySceneLayer == null) {
            mEmptySceneLayer = new SolidColorSceneLayer();
        }
        if (mCurrentSceneLayer == null && mEmptySceneLayer != null) {
            mCurrentSceneLayer = mEmptySceneLayer;
        }
    }

    private boolean hasLayoutTab() {
        return mLayoutTabs != null && mLayoutTabs.length > 0;
    }

    private LayoutTab getLayoutTab() {
        assert hasLayoutTab();
        return mLayoutTabs[0];
    }

    private void createLayoutTabForTabId(@TabId int tabId) {
        LayoutTab layoutTab = createLayoutTab(tabId, mTabModelSelector.isIncognitoSelected());
        mLayoutTabs = new LayoutTab[] {layoutTab};
        updateCacheVisibleIds(Collections.singletonList(tabId));
    }

    private void resetLayoutTabs(boolean clearVisibleIds) {
        if (clearVisibleIds) {
            // Clear the visible IDs as once mLayoutTabs is empty tabs thumbnails cannot be
            // captured. This prevents thumbnail requests from waiting indefinitely.
            updateCacheVisibleIds(Collections.emptyList());
        }

        // mLayoutTabs is used in conjunction with mTabSceneLayer to capture a tab thumbnail for
        // the last visible Tab prior to transitioning to the Hub. This should be nulled once
        // the capture is completed.
        mLayoutTabs = null;
    }

    private void hideCurrentTab() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab != null) {
            currentTab.hide(TabHidingType.TAB_SWITCHER_SHOWN);
        }
    }

    private void updateEmptyLayerColor(@Nullable Pane pane) {
        if (mEmptySceneLayer == null) return;

        mEmptySceneLayer.setBackgroundColor(mHubController.getBackgroundColor(pane));
    }

    private void captureTabThumbnail(
            @Nullable Tab currentTab, @NonNull Promise<Bitmap> bitmapPromise) {
        if (currentTab == null) {
            bitmapPromise.fulfill(null);
            return;
        }

        mTabContentManager.cacheTabThumbnailWithCallback(
                currentTab,
                /* returnBitmap= */ true,
                (bitmap) -> {
                    if (bitmap != null || !currentTab.isNativePage()) {
                        bitmapPromise.fulfill(bitmap);
                        return;
                    }

                    // NativePage may not produce a new bitmap if no state has changed. Refetch from
                    // disk. For a normal tab we can't do this fallback as the thumbnail may be
                    // stale.
                    mTabContentManager.getEtc1TabThumbnailWithCallback(
                            currentTab.getId(), bitmapPromise::fulfill);
                });
    }

    /**
     * Returns the tab id for a {@link Tab}.
     *
     * @param tab The {@link Tab} to get an ID for or null.
     * @return the {@code tab}'s ID or {@link Tab#INVALID_TAB_ID} if null.
     */
    private @TabId int getIdForTab(@Nullable Tab tab) {
        return tab == null ? Tab.INVALID_TAB_ID : tab.getId();
    }

    /**
     * @return The y-offset for the container view that may be impacted by the status indicator and
     *     app header heights.
     */
    private float getContainerYOffset() {
        var params = (FrameLayout.LayoutParams) mHubController.getContainerView().getLayoutParams();
        return params.topMargin;
    }

    /**
     * Update the top margin and y-offset of the container view in response to an app header state
     * change.
     */
    private void maybeUpdateLayout() {
        int appHeaderHeight =
                (mDesktopWindowStateManager != null
                                && mDesktopWindowStateManager.getAppHeaderState() != null)
                        ? mDesktopWindowStateManager.getAppHeaderState().getAppHeaderHeight()
                        : 0;
        mHubManager.setAppHeaderHeight(appHeaderHeight);
        // If the app header height or desktop windowing mode changes while the HubLayout is active,
        // adjust the container view's y-offset. This update will be posted because it has been
        // observed that an immediate update causes unexpected visual positioning in this scenario,
        // potentially because the top margin for the view is also updated at nearly the same time,
        // but the root cause is unknown.
        // TODO (crbug/335651375): Investigate and remove the use of a PostTask for this update.
        // TODO (crbug/334156232): Also update container height.
        if (isActive()) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> mHubController.getContainerView().setY(getContainerYOffset()));
        }
    }

    public HubController getHubControllerForTesting() {
        return mHubController;
    }
}
