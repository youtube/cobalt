// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObscuringHandler.Target;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripHeightObserver;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.DesktopWindowModeState;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

import java.util.concurrent.TimeUnit;

/** Unit test for {@link TabStripTransitionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w600dp-h800dp", shadows = ShadowLooper.class)
@EnableFeatures(ChromeFeatureList.TAB_STRIP_TRANSITION_IN_DESKTOP_WINDOW)
@DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
public class TabStripTransitionCoordinatorUnitTest {
    private static final int TEST_TAB_STRIP_HEIGHT = 40;
    private static final int TEST_TOOLBAR_HEIGHT = 56;
    private static final int NOTHING_OBSERVED = -1;
    private static final int LARGE_NORMAL_WINDOW_WIDTH = 413;
    private static final int LARGE_DESKTOP_WINDOW_WIDTH = 285;
    private static final int NARROW_NORMAL_WINDOW_WIDTH = 411;
    private static final int NARROW_DESKTOP_WINDOW_WIDTH = 283;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private BrowserStateBrowserControlsVisibilityDelegate mVisibilityDelegate;
    @Mock private ControlContainer mControlContainer;
    @Mock private ViewResourceAdapter mViewResourceAdapter;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Captor private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserver;
    @Captor private ArgumentCaptor<Callback<Resource>> mOnCaptureReadyCallback;

    private TestControlContainerView mSpyControlContainer;
    private TabStripTransitionCoordinator mCoordinator;
    private TestActivity mActivity;
    private TabObscuringHandler mTabObscuringHandler = new TabObscuringHandler();
    private TestObserver mObserver;
    private TestDelegate mDelegate;
    private OneshotSupplierImpl<TabStripTransitionDelegate> mDelegateSupplier;
    private int mReservedTopPadding;

    // Test variables
    private int mTopControlsContentOffset;
    private AppHeaderState mAppHeaderState;

    @Before
    public void setup() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
        mSpyControlContainer = TestControlContainerView.createSpy(mActivity);
        mActivity.setContentView(mSpyControlContainer);
        mReservedTopPadding =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_reserved_top_padding);

        // Set the mocks for control container and its view resource adapter.
        doReturn(mSpyControlContainer).when(mControlContainer).getView();
        doReturn(mViewResourceAdapter).when(mControlContainer).getToolbarResourceAdapter();
        doNothing()
                .when(mViewResourceAdapter)
                .addOnResourceReadyCallback(mOnCaptureReadyCallback.capture());
        doAnswer(inv -> triggerCapture()).when(mViewResourceAdapter).triggerBitmapCapture();

        // Set up test browser controls manger.
        mTopControlsContentOffset = TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT;
        doNothing()
                .when(mBrowserControlsVisibilityManager)
                .addObserver(mBrowserControlsObserver.capture());
        doReturn(View.VISIBLE)
                .when(mBrowserControlsVisibilityManager)
                .getAndroidControlsVisibility();
        doAnswer(invocationOnMock -> mTopControlsContentOffset)
                .when(mBrowserControlsVisibilityManager)
                .getContentOffset();
        doReturn(mVisibilityDelegate)
                .when(mBrowserControlsVisibilityManager)
                .getBrowserVisibilityDelegate();
        doReturn(BrowserControlsState.BOTH).when(mVisibilityDelegate).get();

        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ false, LARGE_NORMAL_WINDOW_WIDTH);
    }

    @Test
    public void initWithWideWindow() {
        Assert.assertEquals(
                "Tab strip height is wrong.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());

        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        Assert.assertEquals("Tab strip height is wrong.", 0, mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void initWithNarrowWindow() {
        Assert.assertEquals(
                "Init will not change the tab strip height.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());
        Assert.assertEquals(
                "Tab strip height requested changing to 0.", 0, mObserver.heightRequested);

        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Changing the window to wide will request for full-size tab strip.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    public void hideTabStrip() {
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);

        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();
        runOffsetTransitionForBrowserControlManager(
                /* beginOffset= */ TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT,
                /* endOffset= */ TEST_TOOLBAR_HEIGHT);
        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    public void hideTabStripWithOffsetOverride() {
        // Simulate top controls size change from browser.
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    public void hideTabStripWithForceBrowserControlShown() {
        doReturn(BrowserControlsState.SHOWN).when(mVisibilityDelegate).get();
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    public void hideTabStripWithForceBrowserControlHidden() {
        doReturn(BrowserControlsState.HIDDEN).when(mVisibilityDelegate).get();
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    public void hideTabStripWhileTopControlsHidden() {
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);

        // Assume the top control is hidden and content is at the top.
        doReturn(0).when(mBrowserControlsVisibilityManager).getContentOffset();
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false, false);

        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
        assertObservedTransitionFinished(true);
    }

    @Test
    public void hideTabStripWhileUrlBarFocused_Fullscreen() {
        mCoordinator.onUrlFocusChange(true);
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        Assert.assertEquals(
                "Height request should be blocked by the url bar focus.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        // Url focus animation finished to unblock the transition.
        mCoordinator.onUrlAnimationFinished(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the url bar focus.",
                0,
                mObserver.heightRequested);
    }

    @Test
    public void hideTabStripWhileUrlBarFocused_DesktopWindow() {
        // Assume that the tab strip is initially visible in a desktop window.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);

        mCoordinator.onUrlFocusChange(true);
        setDeviceWidthDp(NARROW_DESKTOP_WINDOW_WIDTH);
        Assert.assertTrue(
                "Height transition should be blocked.",
                mCoordinator.getHeightTransitionHandlerForTesting().isHeightTransitionBlocked());
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 1f);
    }

    @Test
    public void hideTabStripWhileTabObscured_Fullscreen() {
        TabObscuringHandler.Token token = mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        Assert.assertEquals(
                "Height request should be blocked after tab obscured.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        // Tab is unobscured to unblock the transition.
        mTabObscuringHandler.unobscure(token);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after tab unobscured.",
                0,
                mObserver.heightRequested);
    }

    @Test
    public void hideTabStripWhileTabObscured_DesktopWindow() {
        // Assume that the tab strip is initially visible in a desktop window.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);

        mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        setDeviceWidthDp(NARROW_DESKTOP_WINDOW_WIDTH);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 1f);
        Assert.assertTrue(
                "Height transition should be blocked.",
                mCoordinator.getHeightTransitionHandlerForTesting().isHeightTransitionBlocked());
    }

    @Test
    public void hideTabStripWhileTabAndToolbarObscured() {
        mTabObscuringHandler.obscure(Target.ALL_TABS_AND_TOOLBAR);
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        Assert.assertEquals(
                "Height request should go through when tab and toolbar are obscured.",
                0,
                mObserver.heightRequested);
    }

    @Test
    public void hideTabStripDisabledInDesktopWindow() {
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, NARROW_NORMAL_WINDOW_WIDTH);
        Assert.assertEquals(
                "Height transition to hide strip is disabled in a small desktop window.",
                TEST_TAB_STRIP_HEIGHT + mReservedTopPadding,
                mObserver.heightRequested);
    }

    @Test
    public void hideTabStripBeforeLayout() {
        // Simulate the control container hasn't been measured yet.
        doReturn(0).when(mSpyControlContainer).getWidth();
        doReturn(0).when(mSpyControlContainer).getHeight();

        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        Assert.assertEquals(
                "Height request should be ignored if control container hasn't been measured.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip() {
        settleTransitionDuringInitForNarrowWindow();
        setDeviceWidthDp(600);

        doReturn(TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();
        runOffsetTransitionForBrowserControlManager(
                /* beginOffset= */ TEST_TOOLBAR_HEIGHT,
                /* endOffset= */ TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWithOffsetOverride() {
        settleTransitionDuringInitForNarrowWindow();
        // Simulate top controls size change from browser.
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWithBrowserControlForceShown() {
        settleTransitionDuringInitForNarrowWindow();
        doReturn(BrowserControlsState.SHOWN).when(mVisibilityDelegate).get();
        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWithBrowserControlForceHidden() {
        settleTransitionDuringInitForNarrowWindow();
        doReturn(BrowserControlsState.HIDDEN).when(mVisibilityDelegate).get();
        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileTopControlsHidden() {
        settleTransitionDuringInitForNarrowWindow();
        setDeviceWidthDp(600);

        // Assume the top control is hidden and content is at the top.
        doReturn(0).when(mBrowserControlsVisibilityManager).getContentOffset();
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false, false);

        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
        assertObservedTransitionFinished(true);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileUrlBarFocused_Fullscreen() {
        settleTransitionDuringInitForNarrowWindow();
        mCoordinator.onUrlFocusChange(true);
        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should be blocked by the url bar focus.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        // Url focus animation finished to unblock the transition
        mCoordinator.onUrlAnimationFinished(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the url bar focus.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    public void showTabStripWhileUrlBarFocused_DesktopWindow() {
        // Assume that the tab strip is initially hidden by a fade transition.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, NARROW_DESKTOP_WINDOW_WIDTH);

        // Simulate url bar focus.
        mCoordinator.onUrlFocusChange(true);
        // Increase the width of the strip for it to show.
        setDeviceWidthDp(NARROW_DESKTOP_WINDOW_WIDTH + 100);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 0f);
        Assert.assertTrue(
                "Height transition should be blocked.",
                mCoordinator.getHeightTransitionHandlerForTesting().isHeightTransitionBlocked());
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileTabObscured_Fullscreen() {
        settleTransitionDuringInitForNarrowWindow();
        TabObscuringHandler.Token token = mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should be blocked after tab obscured.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        // Tab is unobscured to unblock the transition.
        mTabObscuringHandler.unobscure(token);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the tab unobscured.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w600dp")
    public void showTabStripWhileTabObscured_DesktopWindow() {
        // Assume that the tab strip is initially hidden by a fade transition.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow */ true, NARROW_DESKTOP_WINDOW_WIDTH);

        // Simulate obscuring the tab.
        mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        // Increase the width of the strip for it to show.
        setDeviceWidthDp(NARROW_DESKTOP_WINDOW_WIDTH + 100);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 0f);
        Assert.assertTrue(
                "Height transition should be blocked.",
                mCoordinator.getHeightTransitionHandlerForTesting().isHeightTransitionBlocked());
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileTabAndToolbarObscured() {
        settleTransitionDuringInitForNarrowWindow();
        mTabObscuringHandler.obscure(Target.ALL_TABS_AND_TOOLBAR);
        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should go through if both the tab and toolbar are obscured.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenBeforeLayout() {
        settleTransitionDuringInitForNarrowWindow();
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        mCoordinator.releaseTabStripToken(token);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenDuringLayout() {
        settleTransitionDuringInitForNarrowWindow();
        setConfigurationWithNewWidth(600);

        // Layout pass will trigger the delayed task for layout transition.
        simulateLayoutChange(600);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        Assert.assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        mCoordinator.releaseTabStripToken(token);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenReleaseEarly() {
        settleTransitionDuringInitForNarrowWindow();
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        setConfigurationWithNewWidth(600);
        simulateLayoutChange(600);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        mCoordinator.releaseTabStripToken(token);
        Assert.assertEquals(
                "Height request should be blocked by the delayed layout request.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripBeforeLayout() {
        settleTransitionDuringInitForNarrowWindow();

        // Simulate the control container hasn't been measured yet.
        doReturn(0).when(mSpyControlContainer).getWidth();
        doReturn(0).when(mSpyControlContainer).getHeight();

        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should be ignored if control container hasn't been measured.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);
    }

    @Test
    public void configurationChangedDuringDelayedTask() {
        setConfigurationWithNewWidth(NARROW_NORMAL_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void destroyDuringDelayedTask() {
        setConfigurationWithNewWidth(NARROW_NORMAL_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        // Destroy the coordinator so the transition task is canceled.
        mCoordinator.destroy();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void destroyBeforeCapture() {
        setConfigurationWithNewWidth(NARROW_NORMAL_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        ShadowLooper.idleMainLooper(300, TimeUnit.MILLISECONDS);
        // Tab strip still visible.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        // The capture task is scheduled.
        verify(mViewResourceAdapter).addOnResourceReadyCallback(any());

        // Destroy the coordinator so the capture task won't go through.
        mCoordinator.destroy();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void viewStubInflated() {
        doReturn(mSpyControlContainer.findToolbar)
                .when(mSpyControlContainer)
                .findViewById(R.id.find_toolbar);
        doReturn(mSpyControlContainer.dropTargetView)
                .when(mSpyControlContainer)
                .findViewById(R.id.toolbar_drag_drop_target_view);

        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();
        runOffsetTransitionForBrowserControlManager(
                /* beginOffset= */ TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT,
                /* endOffset= */ TEST_TOOLBAR_HEIGHT);
        assertTabStripHeightForMargins(0);
    }

    @Test
    public void transitionFinishedUMASuccess() {
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.TabStripTransition.Finished", true)) {
            runOffsetTransitionForBrowserControlManager(
                    /* beginOffset= */ TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT,
                    /* endOffset= */ TEST_TOOLBAR_HEIGHT);
        }
    }

    @Test
    public void transitionFinishedUMAInterrupted() {
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();

        int midOffset = TEST_TOOLBAR_HEIGHT + TEST_TAB_STRIP_HEIGHT / 2;
        mTopControlsContentOffset = midOffset;
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false, false);

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.TabStripTransition.Finished", false)) {
            setDeviceWidthDp(600);
        }
    }

    @Test
    public void enterDesktopWindow_IncreaseHeight() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        // Simulate a rect update.
        int newHeight = 10 + TEST_TAB_STRIP_HEIGHT;
        Rect appHeaderRect = new Rect(0, 0, 600, newHeight);
        mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, true);
        mCoordinator.onAppHeaderStateChanged(mAppHeaderState);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertEquals(
                "Height request should include the top padding.",
                newHeight,
                mObserver.heightRequested);

        // Push a browser control height update to kick off the height transition.
        doReturn(TEST_TOOLBAR_HEIGHT).when(mBrowserControlsVisibilityManager).getContentOffset();
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false, false);

        assertTabStripHeightForMargins(newHeight);
        assertObservedHeight(newHeight);
    }

    @Test
    public void enterDesktopWindow_DecreaseHeight() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        // Simulate a rect update that has a smaller height.
        int newHeight = TEST_TAB_STRIP_HEIGHT - 10;
        int expectedHeight = mReservedTopPadding + TEST_TAB_STRIP_HEIGHT;
        Rect appHeaderRect = new Rect(0, 0, 600, newHeight);
        mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, true);
        mCoordinator.onAppHeaderStateChanged(mAppHeaderState);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertEquals(
                "When new height is less than height with reserved padding, use that instead.",
                expectedHeight,
                mObserver.heightRequested);

        // Push a browser control height update to kick off the height transition.
        doReturn(TEST_TOOLBAR_HEIGHT).when(mBrowserControlsVisibilityManager).getContentOffset();
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false, false);

        assertTabStripHeightForMargins(expectedHeight);
        assertObservedHeight(expectedHeight);
    }

    @Test
    public void enterDesktopWindow_DecreaseWidth() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        // Simulate a rect update that has a smaller width.
        int newHeight = TEST_TAB_STRIP_HEIGHT + mReservedTopPadding;
        Rect appHeaderRect = new Rect(0, 0, NARROW_DESKTOP_WINDOW_WIDTH, newHeight);
        mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, true);
        mCoordinator.onAppHeaderStateChanged(mAppHeaderState);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertEquals(
                "Narrow width does not trigger tab strip height transition.",
                newHeight,
                mObserver.heightRequested);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 1f);
    }

    @Test
    public void enterDesktopWindow_NarrowInitialWidth() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        // Create the transition coordinator again for a narrow width desktop window.
        int newHeight = TEST_TAB_STRIP_HEIGHT + mReservedTopPadding;
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, NARROW_DESKTOP_WINDOW_WIDTH);

        Assert.assertEquals(
                "Tab strip height transition was not triggered for window with narrow width.",
                newHeight,
                mObserver.heightRequested);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 1f);
    }

    @Test
    public void enterDesktopWindow_WideInitialWidth() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        // Create the transition coordinator again for a large desktop window.
        int newHeight = TEST_TAB_STRIP_HEIGHT + mReservedTopPadding;
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 0f);
    }

    @Test
    public void enterDesktopWindow_WithoutControlContainerLayout() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        // Set the height as if the first measure pass hasn't happened yet.
        doReturn(0).when(mSpyControlContainer).getHeight();
        doReturn(0).when(mSpyControlContainer).getWidth();

        // Create the transition coordinator for a desktop window.
        int newHeight = TEST_TAB_STRIP_HEIGHT + mReservedTopPadding;
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);

        Assert.assertEquals(
                "Height request should be ignored if control container hasn't been measured.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);
    }

    @Test
    public void recordHistogramWindowResize_LayoutChangeInDesktopWindow() {
        // Simulate desktop windowing mode.
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.WindowResize.DesktopWindowModeState",
                        DesktopWindowModeState.ACTIVE);
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);
        // Histogram should be emitted only when the strip size is changing across multiple layout
        // changes.
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        watcher.assertExpected();
    }

    @Test
    public void recordHistogramWindowResize_LayoutChangeNotInDesktopWindow_SupportedDevice() {
        // Simulate non-desktop windowing mode on a supported device.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ false, LARGE_NORMAL_WINDOW_WIDTH);
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.WindowResize.DesktopWindowModeState",
                        DesktopWindowModeState.INACTIVE);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        watcher.assertExpected();
    }

    @Test
    public void recordHistogramWindowResize_LayoutChangeNotInDesktopWindow_UnsupportedDevice() {
        // Create the transition coordinator with an initial null value of
        // DesktopWindowStateManager that is representative of an unsupported device.
        mDesktopWindowStateManager = null;
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ false, LARGE_NORMAL_WINDOW_WIDTH);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.WindowResize.DesktopWindowModeState",
                        DesktopWindowModeState.UNAVAILABLE);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        watcher.assertExpected();
    }

    // Tests for transitions initiated during desktop windowing mode changes.

    @Test
    public void smallFullscreenWindowToLargeDesktopWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ false);
    }

    @Test
    public void largeFullscreenWindowToLargeDesktopWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ false);
    }

    @Test
    public void largeFullscreenWindowToLargeDesktopWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ true);
    }

    @Test
    public void smallDesktopWindowToLargeFullscreenWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ false);
    }

    @Test
    public void largeDesktopWindowToLargeFullscreenWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ false);
    }

    @Test
    public void smallDesktopWindowToLargeFullscreenWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ true);
    }

    @Test
    public void largeDesktopWindowToLargeFullscreenWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ true);
    }

    private void doTestDesktopWindowModeChanged(
            boolean enterDesktopWindow,
            boolean smallSourceWindow,
            boolean smallDestinationWindow,
            boolean tokenInUse) {
        // Setup widths based on test requirement.
        int sourceWidth;
        int destinationWidth;
        if (enterDesktopWindow) {
            sourceWidth =
                    smallSourceWindow ? NARROW_NORMAL_WINDOW_WIDTH : LARGE_NORMAL_WINDOW_WIDTH;
            destinationWidth =
                    smallDestinationWindow
                            ? NARROW_DESKTOP_WINDOW_WIDTH
                            : LARGE_DESKTOP_WINDOW_WIDTH;
        } else {
            sourceWidth =
                    smallSourceWindow ? NARROW_DESKTOP_WINDOW_WIDTH : LARGE_DESKTOP_WINDOW_WIDTH;
            destinationWidth =
                    smallDestinationWindow ? NARROW_NORMAL_WINDOW_WIDTH : LARGE_NORMAL_WINDOW_WIDTH;
        }

        // Initialize the coordinator with the start state.
        setUpTabStripTransitionCoordinator(!enterDesktopWindow, sourceWidth);

        // If the test requires blocking a height transition by acquiring a token, simulate this
        // scenario.
        if (tokenInUse) {
            mCoordinator.onUrlFocusChange(true);
        }

        // Simulate switching desktop windowing mode.
        simulateAppHeaderStateChanged(destinationWidth, enterDesktopWindow);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify the last height request made to the transition delegate.
        int expectedHeight;
        int expectedHeightAfterTokenRelease;
        if (enterDesktopWindow) {
            expectedHeightAfterTokenRelease = TEST_TAB_STRIP_HEIGHT + mReservedTopPadding;
            expectedHeight = tokenInUse ? NOTHING_OBSERVED : expectedHeightAfterTokenRelease;
        } else {
            // Height will not be updated while exiting desktop windowing mode if the transition is
            // blocked.
            expectedHeightAfterTokenRelease = smallDestinationWindow ? 0 : TEST_TAB_STRIP_HEIGHT;
            expectedHeight =
                    tokenInUse
                            ? TEST_TAB_STRIP_HEIGHT + mReservedTopPadding
                            : expectedHeightAfterTokenRelease;
        }
        Assert.assertEquals(
                "Height is not as expected.", expectedHeight, mObserver.heightRequested);

        // Verify the strip scrim opacity request made to the transition delegate.
        boolean forceFadeInTransition = !enterDesktopWindow && tokenInUse && smallSourceWindow;
        if (enterDesktopWindow || forceFadeInTransition) {
            // While exiting desktop windowing mode, scrim opacity will be updated via a fade
            // transition only when switching from a window with an invisible strip and when the
            // height transition is blocked. In all other cases, the height transition will be
            // responsible for updating the scrim opacity while exiting a desktop window.
            float expectedScrimOpacity =
                    forceFadeInTransition ? 0f : (smallDestinationWindow ? 1f : 0f);
            verifyFadeTransitionState(expectedScrimOpacity);
        }

        // If testing a scenario with tokens in use, unblock the height transition and verify that
        // the desired height request was made.
        if (tokenInUse) {
            mCoordinator.onUrlAnimationFinished(false);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            Assert.assertEquals(
                    "Height request should go through after the token is released.",
                    expectedHeightAfterTokenRelease,
                    mObserver.heightRequested);
        }
    }

    private void setUpTabStripTransitionCoordinator(boolean isInDesktopWindow, int windowWidth) {
        if (mDesktopWindowStateManager != null) {
            int stripHeight = TEST_TAB_STRIP_HEIGHT + (isInDesktopWindow ? mReservedTopPadding : 0);
            var appHeaderRect =
                    isInDesktopWindow ? new Rect(0, 0, windowWidth, stripHeight) : new Rect();
            mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, isInDesktopWindow);
            doAnswer((arg) -> mAppHeaderState).when(mDesktopWindowStateManager).getAppHeaderState();
        }

        mDelegate = new TestDelegate();
        mDelegateSupplier = new OneshotSupplierImpl<>();
        mDelegateSupplier.set(mDelegate);

        mCoordinator =
                new TabStripTransitionCoordinator(
                        mBrowserControlsVisibilityManager,
                        mControlContainer,
                        mSpyControlContainer.toolbarLayout,
                        TEST_TAB_STRIP_HEIGHT,
                        mTabObscuringHandler,
                        mDesktopWindowStateManager,
                        mDelegateSupplier);
        mObserver = new TestObserver();
        mCoordinator.addObserver(mObserver);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    /** Run #onControlsOffsetChanged, changing content offset from |beginOffset| to |endOffset|. */
    private void runOffsetTransitionForBrowserControlManager(int beginOffset, int endOffset) {
        mTopControlsContentOffset = beginOffset;

        final int step = (beginOffset - endOffset) / 10;
        for (int turns = 0; turns <= 10; turns++) {
            // Simulate top controls size change from browser. Input values doesn't matter in this
            // call.
            getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false, false);
            if (mTopControlsContentOffset == endOffset) break;

            assertObservedTransitionFinished(false);
            if (step > 0) {
                mTopControlsContentOffset = Math.max(endOffset, mTopControlsContentOffset - step);
            } else {
                mTopControlsContentOffset = Math.min(endOffset, mTopControlsContentOffset - step);
            }
        }
        assertObservedTransitionFinished(true);
    }

    private void setDeviceWidthDp(int widthDp) {
        Configuration configuration = setConfigurationWithNewWidth(widthDp);
        simulateConfigurationChanged(configuration);
        simulateAppHeaderStateChanged(widthDp, mAppHeaderState.isInDesktopWindow());
        simulateLayoutChange(widthDp);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    private Configuration setConfigurationWithNewWidth(int widthDp) {
        Resources res = mActivity.getResources();
        DisplayMetrics displayMetrics = res.getDisplayMetrics();
        displayMetrics.widthPixels = (int) (displayMetrics.density * widthDp);

        Configuration configuration = res.getConfiguration();
        configuration.screenWidthDp = widthDp;
        mActivity.createConfigurationContext(configuration);
        return configuration;
    }

    private void assertTabStripHeightForMargins(int tabStripHeight) {
        Assert.assertEquals(
                "Top margin is wrong for toolbarLayout.",
                tabStripHeight,
                mSpyControlContainer.toolbarLayout.getTopMargin());
        Assert.assertEquals(
                "Top margin is wrong for findToolbar.",
                tabStripHeight + TEST_TOOLBAR_HEIGHT,
                mSpyControlContainer.findToolbar.getTopMargin());
        Assert.assertEquals(
                "Top margin is wrong for dropTargetView.",
                tabStripHeight,
                mSpyControlContainer.dropTargetView.getTopMargin());
        Assert.assertEquals(
                "Top margin is wrong for toolbarHairline.",
                tabStripHeight + TEST_TOOLBAR_HEIGHT,
                mSpyControlContainer.toolbarHairline.getTopMargin());
    }

    private void assertObservedHeight(int tabStripHeight) {
        Assert.assertEquals(
                "#getHeight has a different value.",
                tabStripHeight,
                mCoordinator.getTabStripHeight());

        Assert.assertEquals(
                "Delegate#onHeightChanged received a different value.",
                tabStripHeight,
                mDelegate.heightChanged);
    }

    private void assertObservedTransitionFinished(boolean finished) {
        Assert.assertEquals(
                "Transition finished signal not dispatched. Current contentOffset: "
                        + mTopControlsContentOffset,
                finished,
                mDelegate.heightTransitionFinished);
    }

    private BrowserControlsStateProvider.Observer getBrowserControlsObserver() {
        var observer = mBrowserControlsObserver.getValue();
        Assert.assertNotNull("Browser controls observer not attached.", observer);
        return observer;
    }

    private Void triggerCapture() {
        var callback = mOnCaptureReadyCallback.getValue();
        Assert.assertNotNull("Capture callback is null.", callback);
        callback.onResult(null);
        return null;
    }

    // For test cases init with narrow width, the initialization will create an transition request.
    private void settleTransitionDuringInitForNarrowWindow() {
        mTopControlsContentOffset = TEST_TOOLBAR_HEIGHT;
        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false, false);
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false, false);
        mObserver = new TestObserver();
        mCoordinator.addObserver(mObserver);
        mDelegate.reset();
    }

    private void simulateLayoutChange(int width) {
        Assert.assertNotNull(mSpyControlContainer.onLayoutChangeListener);
        mSpyControlContainer.onLayoutChangeListener.onLayoutChange(
                mSpyControlContainer,
                /* left= */ 0,
                /* top= */ 0,
                /* right= */ width,
                /* bottom= */ 0,
                0,
                0,
                0,
                0);
    }

    private void simulateConfigurationChanged(Configuration newConfig) {
        mCoordinator.onConfigurationChanged(newConfig != null ? newConfig : new Configuration());
    }

    private void simulateAppHeaderStateChanged(int width, boolean isInDesktopWindow) {
        int stripHeight = TEST_TAB_STRIP_HEIGHT + (isInDesktopWindow ? mReservedTopPadding : 0);
        var appHeaderRect = isInDesktopWindow ? new Rect(0, 0, width, stripHeight) : new Rect();
        mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, isInDesktopWindow);
        mCoordinator.onAppHeaderStateChanged(mAppHeaderState);
    }

    private void verifyFadeTransitionState(float expectedScrimOpacity) {
        Assert.assertEquals(
                "Fade transition end opacity is incorrect.",
                expectedScrimOpacity,
                mDelegate.scrimOpacityRequested,
                0f);
    }

    // Due to the complexity to use the real views for top toolbar in robolectric tests, use view
    // mocks for the sake of unit tests.
    static class TestControlContainerView extends FrameLayout {
        public TestView toolbarLayout;
        public TestView toolbarHairline;
        public TestView findToolbar;
        public TestView dropTargetView;

        @Nullable public View.OnLayoutChangeListener onLayoutChangeListener;

        static TestControlContainerView createSpy(Context context) {
            TestControlContainerView controlContainer =
                    Mockito.spy(new TestControlContainerView(context, null));

            doReturn(controlContainer.toolbarLayout)
                    .when(controlContainer)
                    .findViewById(R.id.toolbar);
            doReturn(controlContainer.toolbarHairline)
                    .when(controlContainer)
                    .findViewById(R.id.toolbar_hairline);
            doReturn(controlContainer.findToolbar)
                    .when(controlContainer)
                    .findViewById(R.id.find_toolbar_tablet_stub);
            doReturn(controlContainer.dropTargetView)
                    .when(controlContainer)
                    .findViewById(R.id.target_view_stub);

            doAnswer(args -> context.getResources().getDisplayMetrics().widthPixels)
                    .when(controlContainer)
                    .getWidth();
            // Set a test height for the control container as if it's already being measured.
            doReturn(TEST_TOOLBAR_HEIGHT + TEST_TAB_STRIP_HEIGHT)
                    .when(controlContainer)
                    .getHeight();
            doAnswer(
                            args -> {
                                controlContainer.onLayoutChangeListener = args.getArgument(0);
                                return null;
                            })
                    .when(controlContainer)
                    .addOnLayoutChangeListener(any());

            return controlContainer;
        }

        public TestControlContainerView(Context context, @Nullable AttributeSet attrs) {
            super(context, attrs);

            toolbarLayout = Mockito.spy(new TestView(context, attrs));
            findToolbar = Mockito.spy(new TestView(context, attrs));
            dropTargetView = Mockito.spy(new TestView(context, attrs));
            when(toolbarLayout.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
            when(findToolbar.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
            when(dropTargetView.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
            toolbarHairline = new TestView(context, attrs);

            MarginLayoutParams sourceParams =
                    new MarginLayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT);
            sourceParams.topMargin = TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT;
            addView(toolbarHairline, new MarginLayoutParams(sourceParams));
            addView(findToolbar, new MarginLayoutParams(sourceParams));

            sourceParams.topMargin = TEST_TAB_STRIP_HEIGHT;
            sourceParams.height = TEST_TOOLBAR_HEIGHT;
            addView(toolbarLayout, new MarginLayoutParams(sourceParams));
            addView(dropTargetView, new MarginLayoutParams(sourceParams));
        }
    }

    static class TestView extends View {
        public TestView(Context context, @Nullable AttributeSet attrs) {
            super(context, attrs);
        }

        public int getTopMargin() {
            return ((MarginLayoutParams) getLayoutParams()).topMargin;
        }
    }

    static class TestObserver implements TabStripHeightObserver {
        public int heightRequested = NOTHING_OBSERVED;

        @Override
        public void onTransitionRequested(int newHeight) {
            heightRequested = newHeight;
        }
    }

    static class TestDelegate implements TabStripTransitionDelegate {
        public int heightChanged = NOTHING_OBSERVED;
        public boolean heightTransitionFinished;
        public float scrimOpacityRequested = NOTHING_OBSERVED;
        private @StripVisibilityState int mStripVisibilityState = StripVisibilityState.UNKNOWN;

        void reset() {
            heightChanged = NOTHING_OBSERVED;
            heightTransitionFinished = false;
            scrimOpacityRequested = NOTHING_OBSERVED;
            mStripVisibilityState = StripVisibilityState.UNKNOWN;
        }

        @Override
        public void onHeightChanged(int newHeight) {
            heightChanged = newHeight;
            mStripVisibilityState =
                    newHeight == 0 ? StripVisibilityState.GONE : StripVisibilityState.VISIBLE;
        }

        @Override
        public void onHeightTransitionFinished() {
            heightTransitionFinished = true;
        }

        @Override
        public void onFadeTransitionRequested(float newOpacity, int durationMs) {
            scrimOpacityRequested = newOpacity;
            mStripVisibilityState =
                    newOpacity == 0f
                            ? StripVisibilityState.VISIBLE
                            : StripVisibilityState.INVISIBLE;
        }

        @Override
        public int getStripVisibilityState() {
            return mStripVisibilityState;
        }
    }
}
