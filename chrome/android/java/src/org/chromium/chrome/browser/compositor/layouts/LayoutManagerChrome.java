// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegateProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceDelegate;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * A {@link Layout} controller for the more complicated Chrome browser.  This is currently a
 * superset of {@link LayoutManagerImpl}.
 */
public class LayoutManagerChrome
        extends LayoutManagerImpl implements ChromeAccessibilityUtil.Observer {
    // Layouts
    /** A {@link Layout} that should be used when the user is swiping sideways on the toolbar. */
    protected ToolbarSwipeLayout mToolbarSwipeLayout;
    /**
     * A {@link Layout} that should be used when the user is in the tab switcher or start surface
     * when the refactor flag isn't enabled.
     */
    protected Layout mOverviewLayout;
    /**
     * A {@link Layout} that should be used when the user is in the start surface when the refactor
     * flag is enabled.
     */
    protected Layout mStartSurfaceHomeLayout;
    /**
     * A {@link Layout} that should be used when the user is in the tab switcher when the refactor
     * flag is enabled.
     */
    protected Layout mTabSwitcherLayout;

    // Event Filter Handlers
    private final SwipeHandler mToolbarSwipeHandler;

    /** Whether or not animations are enabled.  This can disable certain layouts or effects. */
    private boolean mEnableAnimations = true;
    private boolean mCreatingNtp;
    private LayoutStateObserver mTabSwitcherFocusLayoutStateObserver;

    protected ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private boolean mFinishNativeInitialization;

    // Lazy Tab Switcher Init
    private final Supplier<StartSurface> mStartSurfaceSupplier;
    private final Supplier<TabSwitcher> mTabSwitcherSupplier;
    private final ScrimCoordinator mScrimCoordinator;
    private final Callable<ViewGroup> mCreateTabSwitcherOrStartSurfaceCallable;

    // Theme Color
    private TopUiThemeColorProvider mTopUiThemeColorProvider;
    private ThemeColorObserver mThemeColorObserver;

    /**
     * Creates the {@link LayoutManagerChrome} instance.
     * @param host         A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param startSurfaceSupplier Supplier for an interface to talk to the Grid Tab Switcher.
     *         Creates overviewLayout with this surface if this is has value. If not, {@link
     *         #showLayout(int, boolean)} will create overviewLayout.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher when
     *         Start surface refactor is enabled. Used to create overviewLayout if it has value,
     *         otherwise will use the accessibility overview layout.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *         controls.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param tabSwitcherScrimAnchor {@link ViewGroup} used by tab switcher layout to show scrim
     *         when overview is visible.
     * @param scrimCoordinator {@link ScrimCoordinator} to show/hide scrim.
     * @param delayedTabSwitcherOrStartSurfaceCallable Callable to create StartSurface/GTS views.
     */
    public LayoutManagerChrome(LayoutManagerHost host, ViewGroup contentContainer,
            Supplier<StartSurface> startSurfaceSupplier, Supplier<TabSwitcher> tabSwitcherSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            ViewGroup tabSwitcherScrimAnchor, ScrimCoordinator scrimCoordinator,
            Callable<ViewGroup> delayedTabSwitcherOrStartSurfaceCallable) {
        super(host, contentContainer, tabContentManagerSupplier, topUiThemeColorProvider);

        // Build Event Filter Handlers
        mToolbarSwipeHandler = createToolbarSwipeHandler(/* supportSwipeDown = */ true);

        mTabContentManagerSupplier = tabContentManagerSupplier;
        mTabContentManagerSupplier.addObserver(new Callback<TabContentManager>() {
            @Override
            public void onResult(TabContentManager manager) {
                manager.addThumbnailChangeListener((id) -> requestUpdate());
                if (mOverviewLayout != null) {
                    mOverviewLayout.setTabContentManager(manager);
                }
                if (mTabSwitcherLayout != null) {
                    mTabSwitcherLayout.setTabContentManager(manager);
                }
                if (mStartSurfaceHomeLayout != null) {
                    mStartSurfaceHomeLayout.setTabContentManager(manager);
                }
                tabContentManagerSupplier.removeObserver(this);
            }
        });

        mStartSurfaceSupplier = startSurfaceSupplier;
        mTabSwitcherSupplier = tabSwitcherSupplier;
        mScrimCoordinator = scrimCoordinator;
        mCreateTabSwitcherOrStartSurfaceCallable = delayedTabSwitcherOrStartSurfaceCallable;

        Context context = host.getContext();
        if (ReturnToChromeUtil.isStartSurfaceRefactorEnabled(context)) {
            if (mStartSurfaceSupplier.hasValue() || mTabSwitcherSupplier.hasValue()) {
                createLayoutsForStartSurfaceAndTabSwitcher(mStartSurfaceSupplier.get(),
                        mTabSwitcherSupplier.get(), browserControlsStateProvider, scrimCoordinator,
                        tabSwitcherScrimAnchor);
            }
        } else if (mStartSurfaceSupplier.hasValue()) {
            createLayoutsForStartSurfaceAndTabSwitcher(mStartSurfaceSupplier.get(),
                    /*tabSwitcher=*/null, browserControlsStateProvider, scrimCoordinator,
                    tabSwitcherScrimAnchor);
        }
    }

    /**
     * Creates {@link org.chromium.chrome.features.start_surface.TabSwitcherAndStartSurfaceLayout}
     * or ({@link org.chromium.chrome.features.start_surface.StartSurfaceHomeLayout} AND
     * {@link org.chromium.chrome.features.tasks.tab_management.TabSwitcherLayout}).
     * @param startSurface An interface to talk to the Grid Tab Switcher when Start surface refactor
     *         is disabled.
     * @param tabSwitcher An interface to talk to the GridTabSwitcher when Start surface refactor is
     *         enabled and DeferTabSwitcherLayoutCreation is disabled.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *         controls.
     * @param scrimCoordinator scrim coordinator for GTS
     * @param tabSwitcherScrimAnchor scrim anchor view for GTS
     */
    protected void createLayoutsForStartSurfaceAndTabSwitcher(@Nullable StartSurface startSurface,
            @Nullable TabSwitcher tabSwitcher,
            BrowserControlsStateProvider browserControlsStateProvider,
            ScrimCoordinator scrimCoordinator, ViewGroup tabSwitcherScrimAnchor) {
        assert mOverviewLayout == null && mTabSwitcherLayout == null
                && mStartSurfaceHomeLayout == null;
        boolean isRefactorEnabled =
                ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mHost.getContext());

        if (isRefactorEnabled) {
            // TabSwitcherLayout creation is deferred until it is first shown.
            if (!ChromeFeatureList.sDeferTabSwitcherLayoutCreation.isEnabled()) {
                assert tabSwitcher != null;
                createTabSwitcherLayout(tabSwitcher, browserControlsStateProvider, scrimCoordinator,
                        tabSwitcherScrimAnchor);
            }
            if (startSurface != null) {
                createStartSurfaceHomeLayout(startSurface);
            }
        } else {
            assert startSurface != null;
            createTabSwitcherAndStartSurfaceLayout(startSurface, browserControlsStateProvider,
                    scrimCoordinator, tabSwitcherScrimAnchor);
        }
    }

    /**
     * Creates {@link org.chromium.chrome.features.tasks.tab_management.TabSwitcherLayout}
     * @param tabSwitcher An interface to talk to the Grid Tab Switcher.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *         controls.
     * @param scrimCoordinator scrim coordinator for GTS
     * @param tabSwitcherScrimAnchor scrim anchor view for GTS
     */
    protected void createTabSwitcherLayout(@NonNull TabSwitcher tabSwitcher,
            BrowserControlsStateProvider browserControlsStateProvider,
            ScrimCoordinator scrimCoordinator, ViewGroup tabSwitcherScrimAnchor) {
        assert mTabSwitcherLayout == null;
        boolean isRefactorEnabled =
                ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mHost.getContext());
        assert isRefactorEnabled;

        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        TabManagementDelegate tabManagementDelegate = TabManagementDelegateProvider.getDelegate();

        mTabSwitcherLayout = tabManagementDelegate.createTabSwitcherLayout(context, this, this,
                renderHost, browserControlsStateProvider, tabSwitcher, tabSwitcherScrimAnchor,
                scrimCoordinator);

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mHost.getContext())) {
            mTabSwitcherFocusLayoutStateObserver = new LayoutStateObserver() {
                @Override
                public void onFinishedShowing(int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        tabSwitcher.getTabListDelegate().requestFocusOnCurrentTab();
                    }
                }
            };
            addObserver(mTabSwitcherFocusLayoutStateObserver);
        }
        if (mTabContentManagerSupplier.hasValue()) {
            mTabSwitcherLayout.setTabContentManager(mTabContentManagerSupplier.get());
        }
        if (getTabModelSelector() != null) {
            mTabSwitcherLayout.setTabModelSelector(getTabModelSelector());
        }
        if (mFinishNativeInitialization) {
            mTabSwitcherLayout.onFinishNativeInitialization();
        }
    }

    /**
     * Creates {@link org.chromium.chrome.features.start_surface.StartSurfaceHomeLayout}.
     * @param startSurface An interface to talk to the Grid Tab Switcher when Start surface refactor
     *         is disabled.
     */
    protected void createStartSurfaceHomeLayout(@NonNull StartSurface startSurface) {
        assert mStartSurfaceHomeLayout == null;
        boolean isRefactorEnabled =
                ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mHost.getContext());
        assert isRefactorEnabled;

        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        mStartSurfaceHomeLayout = StartSurfaceDelegate.createStartSurfaceHomeLayout(
                context, this, renderHost, startSurface);

        if (mTabContentManagerSupplier.hasValue()) {
            mStartSurfaceHomeLayout.setTabContentManager(mTabContentManagerSupplier.get());
        }
        if (getTabModelSelector() != null) {
            mStartSurfaceHomeLayout.setTabModelSelector(getTabModelSelector());
        }
        if (mFinishNativeInitialization) {
            mStartSurfaceHomeLayout.onFinishNativeInitialization();
        }
    }

    /**
     * Creates {@link org.chromium.chrome.features.start_surface.TabSwitcherAndStartSurfaceLayout}
     * @param startSurface An interface to talk to the Grid Tab Switcher when Start surface refactor
     *         is disabled.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *         controls.
     * @param scrimCoordinator scrim coordinator for GTS
     * @param tabSwitcherScrimAnchor scrim anchor view for GTS
     */
    protected void createTabSwitcherAndStartSurfaceLayout(@NonNull StartSurface startSurface,
            BrowserControlsStateProvider browserControlsStateProvider,
            ScrimCoordinator scrimCoordinator, ViewGroup tabSwitcherScrimAnchor) {
        assert mOverviewLayout == null;
        boolean isRefactorEnabled =
                ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mHost.getContext());
        assert !isRefactorEnabled;

        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        mOverviewLayout = StartSurfaceDelegate.createTabSwitcherAndStartSurfaceLayout(context, this,
                renderHost, browserControlsStateProvider, startSurface, tabSwitcherScrimAnchor,
                scrimCoordinator);

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mHost.getContext())) {
            mTabSwitcherFocusLayoutStateObserver = new LayoutStateObserver() {
                @Override
                public void onFinishedShowing(int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        startSurface.getGridTabListDelegate().requestFocusOnCurrentTab();
                    }
                }
            };
            addObserver(mTabSwitcherFocusLayoutStateObserver);
        }

        if (mTabContentManagerSupplier.hasValue()) {
            mOverviewLayout.setTabContentManager(mTabContentManagerSupplier.get());
        }

        if (getTabModelSelector() != null) {
            mOverviewLayout.setTabModelSelector(getTabModelSelector());
        }
        if (mFinishNativeInitialization) {
            mOverviewLayout.onFinishNativeInitialization();
        }
    }

    /**
     * @return A list of virtual views representing compositor rendered views.
     */
    @Override
    public void getVirtualViews(List<VirtualView> views) {
        // TODO(dtrainor): Investigate order.
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            mSceneOverlays.get(i).getVirtualViews(views);
        }
    }

    /**
     * @return The {@link SwipeHandler} responsible for processing swipe events for the toolbar.
     */
    @Override
    public SwipeHandler getToolbarSwipeHandler() {
        return mToolbarSwipeHandler;
    }

    @Override
    public SwipeHandler createToolbarSwipeHandler(boolean supportSwipeDown) {
        return new ToolbarSwipeHandler(supportSwipeDown);
    }

    @Override
    public void init(TabModelSelector selector, TabCreatorManager creator,
            ControlContainer controlContainer, DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();
        BrowserControlsStateProvider browserControlsStateProvider =
                mHost.getBrowserControlsManager();

        // Build Layouts
        mToolbarSwipeLayout = new ToolbarSwipeLayout(context, this, renderHost,
                browserControlsStateProvider, this, topUiColorProvider);

        super.init(selector, creator, controlContainer, dynamicResourceLoader, topUiColorProvider);

        // Initialize Layouts
        TabContentManager content = mTabContentManagerSupplier.get();
        mToolbarSwipeLayout.setTabModelSelector(selector);
        mToolbarSwipeLayout.setTabContentManager(content);
        if (mOverviewLayout != null) {
            mOverviewLayout.setTabModelSelector(selector);
            mOverviewLayout.setTabContentManager(content);
            mOverviewLayout.onFinishNativeInitialization();
        }
        if (mTabSwitcherLayout != null) {
            mTabSwitcherLayout.setTabModelSelector(selector);
            mTabSwitcherLayout.setTabContentManager(content);
            mTabSwitcherLayout.onFinishNativeInitialization();
        }
        if (mStartSurfaceHomeLayout != null) {
            mStartSurfaceHomeLayout.setTabModelSelector(selector);
            mStartSurfaceHomeLayout.setTabContentManager(content);
            mStartSurfaceHomeLayout.onFinishNativeInitialization();
        }
        mFinishNativeInitialization = true;
    }

    @Override
    public void showLayout(int layoutType, boolean animate) {
        if (layoutType == LayoutType.TAB_SWITCHER && mOverviewLayout == null
                && mTabSwitcherLayout == null) {
            initTabSwitcher();
        }
        super.showLayout(layoutType, animate);
    }

    /**
     * For lazy initialization of {@link mTabSwitcherLayout} or {@link mOverviewLayout}.
     * This always happens the first time the tab switcher is shown on tablets or if Start Surface
     * refactor is enabled for phones.
     */
    private void initTabSwitcher() {
        try {
            boolean isRefactorEnabled =
                    ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mHost.getContext());

            // Implicitly guarded by sDeferTabSwitcherLayoutCreation as mTabSwitcherSupplier will
            // have a value already if the feature is disabled.
            if (mStartSurfaceSupplier.hasValue()
                    && (!isRefactorEnabled || mTabSwitcherSupplier.hasValue())) {
                return;
            }

            final ViewGroup containerView = mCreateTabSwitcherOrStartSurfaceCallable.call();
            if (isRefactorEnabled) {
                final TabSwitcher tabSwitcher = mTabSwitcherSupplier.get();
                assert tabSwitcher != null;
                createTabSwitcherLayout(tabSwitcher, mHost.getBrowserControlsManager(),
                        mScrimCoordinator, containerView);
            } else {
                final StartSurface startSurface = mStartSurfaceSupplier.get();
                assert startSurface != null;
                createTabSwitcherAndStartSurfaceLayout(startSurface,
                        mHost.getBrowserControlsManager(), mScrimCoordinator, containerView);
            }

            if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mHost.getContext())) {
                mThemeColorObserver =
                        (color, shouldAnimate) -> containerView.setBackgroundColor(color);
                mTopUiThemeColorProvider = getTopUiThemeColorProvider().get();
                mTopUiThemeColorProvider.addThemeColorObserver(mThemeColorObserver);
            }
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize start surface.", e);
        }
    }

    @Override
    public void setTabModelSelector(TabModelSelector selector) {
        super.setTabModelSelector(selector);
        if (mOverviewLayout != null) {
            mOverviewLayout.setTabModelSelector(selector);
        }
        if (mTabSwitcherLayout != null) {
            mTabSwitcherLayout.setTabModelSelector(selector);
        }
        if (mStartSurfaceHomeLayout != null) {
            mStartSurfaceHomeLayout.setTabModelSelector(selector);
        }
    }

    @Override
    public void destroy() {
        super.destroy();

        if (mTabContentManagerSupplier != null) {
            mTabContentManagerSupplier = null;
        }

        if (mTabSwitcherFocusLayoutStateObserver != null) {
            removeObserver(mTabSwitcherFocusLayoutStateObserver);
            mTabSwitcherFocusLayoutStateObserver = null;
        }

        if (mOverviewLayout != null) {
            mOverviewLayout.destroy();
            mOverviewLayout = null;
        }
        if (mTabSwitcherLayout != null) {
            mTabSwitcherLayout.destroy();
            mTabSwitcherLayout = null;
        }
        if (mStartSurfaceHomeLayout != null) {
            mStartSurfaceHomeLayout.destroy();
            mStartSurfaceHomeLayout = null;
        }
        if (mToolbarSwipeLayout != null) {
            mToolbarSwipeLayout.destroy();
        }
        if (mTopUiThemeColorProvider != null && mThemeColorObserver != null) {
            mTopUiThemeColorProvider.removeThemeColorObserver(mThemeColorObserver);
            mTopUiThemeColorProvider = null;
            mThemeColorObserver = null;
        }
    }

    @Override
    protected Layout getLayoutForType(@LayoutType int layoutType) {
        Layout layout;
        if (layoutType == LayoutType.TOOLBAR_SWIPE) {
            layout = mToolbarSwipeLayout;
        } else if (layoutType == LayoutType.TAB_SWITCHER) {
            if (mTabSwitcherLayout != null) {
                layout = mTabSwitcherLayout;
            } else {
                layout = mOverviewLayout;
            }
        } else if (layoutType == LayoutType.START_SURFACE) {
            layout = mStartSurfaceHomeLayout;
        } else {
            layout = super.getLayoutForType(layoutType);
        }
        return layout;
    }

    @Override
    protected void startShowing(Layout layout, boolean animate) {
        mCreatingNtp = false;
        super.startShowing(layout, animate);
    }

    @Override
    public void doneHiding() {
        if (getNextLayout() == getDefaultLayout()) {
            Tab tab = getTabModelSelector() != null ? getTabModelSelector().getCurrentTab() : null;
            emptyTabCachesExcept(tab != null ? tab.getId() : Tab.INVALID_TAB_ID);
        }

        super.doneHiding();
    }

    @Override
    protected void tabCreated(int id, int sourceId, @TabLaunchType int launchType,
            boolean incognito, boolean willBeSelected, float originX, float originY) {
        Tab newTab = TabModelUtils.getTabById(getTabModelSelector().getModel(incognito), id);
        mCreatingNtp = newTab != null && newTab.isNativePage();
        super.tabCreated(id, sourceId, launchType, incognito, willBeSelected, originX, originY);
    }

    @Override
    protected void tabClosed(int id, int nextId, boolean incognito, boolean tabRemoved) {
        boolean showOverview = nextId == Tab.INVALID_TAB_ID;
        boolean animate = !tabRemoved && animationsEnabled();
        if (getActiveLayoutType() != LayoutType.TAB_SWITCHER
                && getActiveLayoutType() != LayoutType.START_SURFACE && showOverview
                && getNextLayoutType() != LayoutType.TAB_SWITCHER) {
            showLayout(LayoutType.TAB_SWITCHER, animate);
        }
        super.tabClosed(id, nextId, incognito, tabRemoved);
    }

    @Override
    public void onTabsAllClosing(boolean incognito) {
        if (getActiveLayout() == mStaticLayout) return;

        super.onTabsAllClosing(incognito);
    }

    /**
     * @return The overview layout {@link Layout} managed by this class.
     */
    @VisibleForTesting
    public Layout getOverviewLayout() {
        return mOverviewLayout;
    }

    /** Initializes TabSwitcherLayout without needing to open the Tab Switcher. */
    public void initTabSwitcherLayoutForTesting() {
        initTabSwitcher();
    }

    /**
     * Returns the grid tab switcher layout {@link Layout} managed by this class. This should be
     * non-null when the Start surface refactor is enabled if init has finished.
     */
    public Layout getTabSwitcherLayoutForTesting() {
        return mTabSwitcherLayout;
    }

    /**
     * @return The {@link StripLayoutHelperManager} managed by this class.
     */
    @VisibleForTesting
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return null;
    }

    /**
     * @param enabled Whether or not to allow model-reactive animations (tab creation, closing,
     *                etc.).
     */
    public void setEnableAnimations(boolean enabled) {
        mEnableAnimations = enabled;
    }

    /**
     * @return Whether animations should be done for model changes.
     */
    @VisibleForTesting
    public boolean animationsEnabled() {
        return mEnableAnimations;
    }

    // ChromeAccessibilityUtil.Observer

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        setEnableAnimations(DeviceClassManager.enableAnimations());
    }

    /**
     * A {@link SwipeHandler} meant to respond to edge events for the toolbar.
     */
    protected class ToolbarSwipeHandler implements SwipeHandler {
        /** The scroll direction of the current gesture. */
        private @ScrollDirection int mScrollDirection;

        /**
         * The range in degrees that a swipe can be from a particular direction to be considered
         * that direction.
         */
        private static final float SWIPE_RANGE_DEG = 25;

        private final boolean mSupportSwipeDown;

        public ToolbarSwipeHandler(boolean supportSwipeDown) {
            mSupportSwipeDown = supportSwipeDown;
        }

        @Override
        public void onSwipeStarted(@ScrollDirection int direction, MotionEvent ev) {
            mScrollDirection = ScrollDirection.UNKNOWN;
        }

        @Override
        public void onSwipeUpdated(MotionEvent current, float tx, float ty, float dx, float dy) {
            if (mToolbarSwipeLayout == null) return;

            float x = current.getRawX() * mPxToDp;
            float y = current.getRawY() * mPxToDp;
            dx *= mPxToDp;
            dy *= mPxToDp;
            tx *= mPxToDp;
            ty *= mPxToDp;

            // If scroll direction has been computed, send the event to super.
            if (mScrollDirection != ScrollDirection.UNKNOWN) {
                mToolbarSwipeLayout.swipeUpdated(time(), x, y, dx, dy, tx, ty);
                return;
            }

            mScrollDirection = computeScrollDirection(dx, dy);
            if (mScrollDirection == ScrollDirection.UNKNOWN) return;

            if (mSupportSwipeDown && isTabSwitcherReady()
                    && mScrollDirection == ScrollDirection.DOWN) {
                RecordUserAction.record("MobileToolbarSwipeOpenStackView");
                showLayout(LayoutType.TAB_SWITCHER, true);
            } else if (mScrollDirection == ScrollDirection.LEFT
                    || mScrollDirection == ScrollDirection.RIGHT) {
                startShowing(mToolbarSwipeLayout, true);
            }

            mToolbarSwipeLayout.swipeStarted(time(), mScrollDirection, x, y);
        }

        @Override
        public void onSwipeFinished() {
            if (mToolbarSwipeLayout == null || !mToolbarSwipeLayout.isActive()) return;
            mToolbarSwipeLayout.swipeFinished(time());
        }

        @Override
        public void onFling(@ScrollDirection int direction, MotionEvent current, float tx, float ty,
                float vx, float vy) {
            if (mToolbarSwipeLayout == null || !mToolbarSwipeLayout.isActive()) return;
            float x = current.getRawX() * mPxToDp;
            float y = current.getRawX() * mPxToDp;
            tx *= mPxToDp;
            ty *= mPxToDp;
            vx *= mPxToDp;
            vy *= mPxToDp;
            mToolbarSwipeLayout.swipeFlingOccurred(time(), x, y, tx, ty, vx, vy);
        }

        /**
         * Compute the direction of the scroll.
         * @param dx The distance traveled on the X axis.
         * @param dy The distance traveled on the Y axis.
         * @return The direction of the scroll.
         */
        private @ScrollDirection int computeScrollDirection(float dx, float dy) {
            @ScrollDirection
            int direction = ScrollDirection.UNKNOWN;

            // Figure out the angle of the swipe. Invert 'dy' so 90 degrees is up.
            double swipeAngle = (Math.toDegrees(Math.atan2(-dy, dx)) + 360) % 360;

            if (swipeAngle < 180 + SWIPE_RANGE_DEG && swipeAngle > 180 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.LEFT;
            } else if (swipeAngle < SWIPE_RANGE_DEG || swipeAngle > 360 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.RIGHT;
            } else if (swipeAngle < 270 + SWIPE_RANGE_DEG && swipeAngle > 270 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.DOWN;
            }

            return direction;
        }

        @Override
        public boolean isSwipeEnabled(@ScrollDirection int direction) {
            FullscreenManager manager = mHost.getFullscreenManager();
            if (getActiveLayout() != mStaticLayout || !DeviceClassManager.enableToolbarSwipe()
                    || (manager != null && manager.getPersistentFullscreenMode())) {
                return false;
            }

            if (direction == ScrollDirection.DOWN) {
                return isTabSwitcherReady();
            }

            return direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT;
        }

        /**
         * @return Whether or not we are ready to show the GTS layout.
         */
        private boolean isTabSwitcherReady() {
            // On Tablets, or with Start Surface Refactor, attempting to show the GTS while it's
            // null will trigger its creation.
            return mOverviewLayout != null || mTabSwitcherLayout != null
                    || DeviceFormFactor.isNonMultiDisplayContextOnTablet(mHost.getContext())
                    || (ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mHost.getContext())
                            && ChromeFeatureList.sDeferTabSwitcherLayoutCreation.isEnabled());
        }
    }

    /**
     * @param id The id of the {@link Tab} to search for.
     * @return   A {@link Tab} instance or {@code null} if it could be found.
     */
    protected Tab getTabById(int id) {
        TabModelSelector selector = getTabModelSelector();
        return selector == null ? null : selector.getTabById(id);
    }

    @Override
    protected void switchToTab(Tab tab, int lastTabId) {
        if (tab == null || lastTabId == Tab.INVALID_TAB_ID) {
            super.switchToTab(tab, lastTabId);
            return;
        }

        mToolbarSwipeLayout.setSwitchToTab(tab.getId(), lastTabId);
        showLayout(LayoutType.TOOLBAR_SWIPE, false);
    }
}
