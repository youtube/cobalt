// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.app.Activity;
import android.content.Context;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.OverlayPanelEventFilter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Class responsible for testing the OverlayPanelEventFilter. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class OverlayPanelEventFilterTest {
    private static final float PANEL_ALMOST_MAXIMIZED_OFFSET_Y_DP = 50.f;
    private static final float BAR_HEIGHT_DP = 100.f;

    private static final float LAYOUT_WIDTH_DP = 600.f;
    private static final float LAYOUT_HEIGHT_DP = 800.f;

    // A small value used to check whether two floats are almost equal.
    private static final float EPSILON = 1e-04f;

    private static final int MOCK_TOOLBAR_HEIGHT = 100;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutManagerImpl mLayoutManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ViewGroup mCompositorViewHolder;
    @Mock private Tab mTab;
    @Mock private OverlayContentDelegate mOverlayContentDelegate;
    @Mock private OverlayContentProgressObserver mOverlayContentProgressObserver;

    private float mTouchSlopDp;
    private float mDpToPx;

    private float mAlmostMaximizedContentOffsetYDp;
    private float mMaximizedContentOffsetYDp;

    private float mContentVerticalScroll;

    private boolean mWasTapDetectedOnContent;
    private boolean mWasScrollDetectedOnContent;

    private MockOverlayPanel mPanel;
    private OverlayPanelEventFilterWrapper mEventFilter;

    private boolean mShouldLockHorizontalMotionInContent;
    private MotionEvent mEventPropagatedToContent;
    private boolean mEventWasScroll;
    private boolean mEventWasTap;

    private WindowAndroid mWindowAndroid;
    private Activity mActivity;

    // --------------------------------------------------------------------------------------------
    // OverlayPanelEventFilterWrapper
    // --------------------------------------------------------------------------------------------

    /** Wrapper around OverlayPanelEventFilter used by tests. */
    public final class OverlayPanelEventFilterWrapper extends OverlayPanelEventFilter {
        public OverlayPanelEventFilterWrapper(Context context, OverlayPanel panel) {
            super(context, panel);
        }

        @Override
        protected float getContentViewVerticalScroll() {
            return mContentVerticalScroll;
        }

        @Override
        protected void propagateEventToContent(MotionEvent e) {
            mEventPropagatedToContent = MotionEvent.obtain(e);
            super.propagateEventToContent(e);
            mEventPropagatedToContent.recycle();
        }

        @Override
        protected boolean handleSingleTapUp(MotionEvent e) {
            boolean handled = super.handleSingleTapUp(e);
            mEventWasTap = true;
            return handled;
        }

        @Override
        protected boolean handleScroll(MotionEvent e1, MotionEvent e2, float distanceY) {
            boolean handled = super.handleScroll(e1, e2, distanceY);
            mEventWasScroll = true;
            return handled;
        }
    }

    // --------------------------------------------------------------------------------------------
    // MockOverlayPanel
    // --------------------------------------------------------------------------------------------

    /** Mocks an OverlayPanel, so it doesn't create WebContents or animations. */
    private final class MockOverlayPanel extends OverlayPanel {
        private boolean mWasTapDetectedOnPanel;
        private boolean mWasScrollDetectedOnPanel;

        public MockOverlayPanel(
                Context context,
                LayoutManagerImpl layoutManager,
                OverlayPanelManager manager,
                BrowserControlsStateProvider browserControlsStateProvider,
                WindowAndroid windowAndroid,
                ViewGroup compositorViewHolder,
                Tab tab) {
            super(
                    context,
                    layoutManager,
                    manager,
                    browserControlsStateProvider,
                    windowAndroid,
                    compositorViewHolder,
                    MOCK_TOOLBAR_HEIGHT,
                    () -> tab);
        }

        @Override
        public OverlayPanelContent createNewOverlayPanelContent() {
            return new MockOverlayPanelContent();
        }

        /** Override creation and destruction of the WebContents as they rely on native methods. */
        private class MockOverlayPanelContent extends OverlayPanelContent {
            public MockOverlayPanelContent() {
                super(
                        mOverlayContentDelegate,
                        mOverlayContentProgressObserver,
                        mActivity,
                        /* isIncognito= */ false,
                        MOCK_TOOLBAR_HEIGHT,
                        mCompositorViewHolder,
                        mWindowAndroid,
                        () -> mTab);
            }

            @Override
            public void removeLastHistoryEntry(String url, long timeInMs) {}
        }

        @Override
        public ViewGroup getContainerView() {
            return new ViewGroup(ApplicationProvider.getApplicationContext()) {
                @Override
                public boolean dispatchTouchEvent(MotionEvent e) {
                    if (e.getActionMasked() != MotionEvent.ACTION_CANCEL) {
                        mWasScrollDetectedOnContent = mEventWasScroll;
                        mWasTapDetectedOnContent = mEventWasTap;

                        // Check that the event offset is correct.
                        if (!mShouldLockHorizontalMotionInContent) {
                            float propagatedEventY = mEventPropagatedToContent.getY();
                            float offsetY = mPanel.getContentY() * mDpToPx;
                            Assert.assertEquals(propagatedEventY - offsetY, e.getY(), EPSILON);
                        }
                    } else {
                        mWasScrollDetectedOnContent = false;
                        mWasTapDetectedOnContent = false;
                    }
                    return super.dispatchTouchEvent(e);
                }

                @Override
                public void onLayout(boolean changed, int l, int t, int r, int b) {}
            };
        }

        @Override
        protected void resizePanelContentView() {}

        @Override
        protected void animatePanelTo(float height, long duration) {
            // Do not create animations for tests.
        }

        public boolean getWasTapDetected() {
            return mWasTapDetectedOnPanel;
        }

        public boolean getWasScrollDetected() {
            return mWasScrollDetectedOnPanel;
        }

        // GestureHandler overrides.

        @Override
        public void onDown(float x, float y, boolean fromMouse, int buttons) {}

        @Override
        public void onUpOrCancel() {}

        @Override
        public void drag(float x, float y, float dx, float dy, float tx, float ty) {
            mWasScrollDetectedOnPanel = true;
        }

        @Override
        public void click(float x, float y, boolean fromMouse, int buttons) {
            mWasTapDetectedOnPanel = true;
        }

        @Override
        public void fling(float x, float y, float velocityX, float velocityY) {}

        @Override
        public void onLongPress(float x, float y) {}

        @Override
        public void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent) {}
    }

    // --------------------------------------------------------------------------------------------
    // Test Suite
    // --------------------------------------------------------------------------------------------

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
    }

    @Before
    public void setupTest() {
        Context context = ApplicationProvider.getApplicationContext();

        mDpToPx = context.getResources().getDisplayMetrics().density;
        mTouchSlopDp = ViewConfiguration.get(context).getScaledTouchSlop() / mDpToPx;

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity = activityTestRule.getActivity();
                    mWindowAndroid =
                            new ActivityWindowAndroid(
                                    mActivity,
                                    /* listenToActivityState= */ true,
                                    IntentRequestTracker.createFromActivity(mActivity));

                    mPanel =
                            new MockOverlayPanel(
                                    mActivity,
                                    mLayoutManager,
                                    new OverlayPanelManager(),
                                    mBrowserControlsStateProvider,
                                    mWindowAndroid,
                                    mCompositorViewHolder,
                                    mTab);
                    mEventFilter = new OverlayPanelEventFilterWrapper(mActivity, mPanel);

                    mPanel.setSearchBarHeightForTesting(BAR_HEIGHT_DP);
                    mPanel.setHeightForTesting(LAYOUT_HEIGHT_DP);
                    mPanel.setIsFullWidthSizePanelForTesting(true);

                    // NOTE(pedrosimonetti): This should be called after calling the method
                    // setIsFullWidthSizePanelForTesting(), otherwise it will crash the test.
                    mPanel.onSizeChanged(LAYOUT_WIDTH_DP, LAYOUT_HEIGHT_DP, 0, 0);
                });

        setContentViewVerticalScroll(0);

        mAlmostMaximizedContentOffsetYDp = PANEL_ALMOST_MAXIMIZED_OFFSET_Y_DP + BAR_HEIGHT_DP;
        mMaximizedContentOffsetYDp = BAR_HEIGHT_DP;

        mWasTapDetectedOnContent = false;
        mWasScrollDetectedOnContent = false;

        mShouldLockHorizontalMotionInContent = false;
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid.destroy();
                });
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testTapContentView() {
        positionPanelInAlmostMaximizedState();

        // Simulate tap.
        simulateActionDownEvent(0.f, mAlmostMaximizedContentOffsetYDp + 1.f);
        simulateActionUpEvent(0.f, mAlmostMaximizedContentOffsetYDp + 1.f);

        Assert.assertFalse(mPanel.getWasScrollDetected());
        Assert.assertFalse(mPanel.getWasTapDetected());

        Assert.assertTrue(mWasTapDetectedOnContent);
        Assert.assertFalse(mWasScrollDetectedOnContent);
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testScrollingContentViewDragsPanel() {
        positionPanelInAlmostMaximizedState();

        // Simulate swipe up sequence.
        simulateActionDownEvent(0.f, mAlmostMaximizedContentOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mMaximizedContentOffsetYDp);
        simulateActionUpEvent(0.f, mMaximizedContentOffsetYDp);

        Assert.assertTrue(mPanel.getWasScrollDetected());
        Assert.assertFalse(mPanel.getWasTapDetected());

        Assert.assertFalse(mWasScrollDetectedOnContent);
        Assert.assertFalse(mWasTapDetectedOnContent);
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testScrollUpContentView() {
        positionPanelInMaximizedState();

        // Simulate swipe up sequence.
        simulateActionDownEvent(0.f, mAlmostMaximizedContentOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mMaximizedContentOffsetYDp);
        simulateActionUpEvent(0.f, mMaximizedContentOffsetYDp);

        Assert.assertFalse(mPanel.getWasScrollDetected());
        Assert.assertFalse(mPanel.getWasTapDetected());

        Assert.assertTrue(mWasScrollDetectedOnContent);
        Assert.assertFalse(mWasTapDetectedOnContent);
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testScrollDownContentView() {
        positionPanelInMaximizedState();

        // When the Panel is maximized and the scroll position is greater than zero, a swipe down
        // on the ContentView should trigger a scroll on it.
        setContentViewVerticalScroll(100.f);

        // Simulate swipe down sequence.
        simulateActionDownEvent(0.f, mMaximizedContentOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mAlmostMaximizedContentOffsetYDp);
        simulateActionUpEvent(0.f, mAlmostMaximizedContentOffsetYDp);

        Assert.assertFalse(mPanel.getWasScrollDetected());
        Assert.assertFalse(mPanel.getWasTapDetected());

        Assert.assertTrue(mWasScrollDetectedOnContent);
        Assert.assertFalse(mWasTapDetectedOnContent);
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testDragByOverscrollingContentView() {
        positionPanelInMaximizedState();

        // When the Panel is maximized and the scroll position is zero, a swipe down on the
        // ContentView should trigger a swipe on the Panel.
        setContentViewVerticalScroll(0.f);

        // Simulate swipe down sequence.
        simulateActionDownEvent(0.f, mMaximizedContentOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mAlmostMaximizedContentOffsetYDp);
        simulateActionUpEvent(0.f, mAlmostMaximizedContentOffsetYDp);

        Assert.assertTrue(mPanel.getWasScrollDetected());
        Assert.assertFalse(mPanel.getWasTapDetected());

        Assert.assertFalse(mWasScrollDetectedOnContent);
        Assert.assertFalse(mWasTapDetectedOnContent);
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testUnwantedScrollDoesNotHappenInContentView() {
        positionPanelInAlmostMaximizedState();

        float contentViewOffsetYStart = mAlmostMaximizedContentOffsetYDp + 1.f;
        float contentViewOffsetYEnd = mMaximizedContentOffsetYDp - 1.f;

        // Simulate swipe up to maximized position.
        simulateActionDownEvent(0.f, contentViewOffsetYStart);
        simulateActionMoveEvent(0.f, mMaximizedContentOffsetYDp);
        positionPanelInMaximizedState();

        // Confirm that the Panel got a scroll event.
        Assert.assertTrue(mPanel.getWasScrollDetected());

        // Continue the swipe up for one more dp. From now on, the events might be forwarded
        // to the ContentView.
        simulateActionMoveEvent(0.f, contentViewOffsetYEnd);
        simulateActionUpEvent(0.f, contentViewOffsetYEnd);

        // But 1 dp is not enough to trigger a scroll in the ContentView, and in this
        // particular case, it should also not trigger a tap because the total displacement
        // of the touch gesture is greater than the touch slop.
        float contentViewOffsetDelta = contentViewOffsetYStart - contentViewOffsetYEnd;
        Assert.assertTrue(Math.abs(contentViewOffsetDelta) > mTouchSlopDp);

        Assert.assertFalse(mPanel.getWasTapDetected());

        Assert.assertFalse(mWasScrollDetectedOnContent);
        Assert.assertFalse(mWasTapDetectedOnContent);
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testDragPanelThenContinuouslyScrollContentView() {
        positionPanelInAlmostMaximizedState();

        // Simulate swipe up to maximized position.
        simulateActionDownEvent(0.f, mAlmostMaximizedContentOffsetYDp + 1.f);
        simulateActionMoveEvent(0.f, mMaximizedContentOffsetYDp);
        positionPanelInMaximizedState();

        // Confirm that the Panel got a scroll event.
        Assert.assertTrue(mPanel.getWasScrollDetected());

        // Continue the swipe up for one more dp. From now on, the events might be forwarded
        // to the ContentView.
        simulateActionMoveEvent(0.f, mMaximizedContentOffsetYDp - 1.f);

        // Now keep swiping up an amount greater than the touch slop. In this case a scroll
        // should be triggered in the ContentView.
        simulateActionMoveEvent(0.f, mMaximizedContentOffsetYDp - 2 * mTouchSlopDp);
        simulateActionUpEvent(0.f, mMaximizedContentOffsetYDp - 2 * mTouchSlopDp);

        Assert.assertFalse(mPanel.getWasTapDetected());

        Assert.assertTrue(mWasScrollDetectedOnContent);
        Assert.assertFalse(mWasTapDetectedOnContent);
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testTapPanel() {
        positionPanelInAlmostMaximizedState();

        // Simulate tap.
        simulateActionDownEvent(0.f, mAlmostMaximizedContentOffsetYDp - 1.f);
        simulateActionUpEvent(0.f, mAlmostMaximizedContentOffsetYDp - 1.f);

        Assert.assertFalse(mPanel.getWasScrollDetected());
        Assert.assertTrue(mPanel.getWasTapDetected());

        Assert.assertFalse(mWasScrollDetectedOnContent);
        Assert.assertFalse(mWasTapDetectedOnContent);
    }

    @Test
    @MediumTest
    @Feature({"OverlayPanel"})
    @UiThreadTest
    public void testScrollPanel() {
        positionPanelInAlmostMaximizedState();

        // Simulate swipe up sequence.
        simulateActionDownEvent(0.f, mAlmostMaximizedContentOffsetYDp - 1.f);
        simulateActionMoveEvent(0.f, mMaximizedContentOffsetYDp);
        simulateActionUpEvent(0.f, mMaximizedContentOffsetYDp);

        Assert.assertTrue(mPanel.getWasScrollDetected());
        Assert.assertFalse(mPanel.getWasTapDetected());

        Assert.assertFalse(mWasScrollDetectedOnContent);
        Assert.assertFalse(mWasTapDetectedOnContent);
    }

    // --------------------------------------------------------------------------------------------
    // Helpers
    // --------------------------------------------------------------------------------------------

    /** Positions the Panel in the almost maximized state. */
    private void positionPanelInAlmostMaximizedState() {
        mPanel.setSearchBarHeightForTesting(BAR_HEIGHT_DP);
        mPanel.setMaximizedForTesting(false);
        mPanel.setOffsetYForTesting(PANEL_ALMOST_MAXIMIZED_OFFSET_Y_DP);
    }

    /** Positions the Panel in the maximized state. */
    private void positionPanelInMaximizedState() {
        mPanel.setSearchBarHeightForTesting(BAR_HEIGHT_DP);
        mPanel.setMaximizedForTesting(true);
        mPanel.setOffsetYForTesting(0);
    }

    /**
     * Sets the vertical scroll position of the ContentView.
     *
     * @param contentViewVerticalScroll The vertical scroll position.
     */
    private void setContentViewVerticalScroll(float contentViewVerticalScroll) {
        mContentVerticalScroll = contentViewVerticalScroll;
    }

    /**
     * Simulates a MotionEvent in the OverlayPanelEventFilter.
     *
     * @param action The event's action.
     * @param x The event's x coordinate in dps.
     * @param y The event's y coordinate in dps.
     */
    private void simulateEvent(int action, float x, float y) {
        MotionEvent motionEvent = MotionEvent.obtain(0, 0, action, x * mDpToPx, y * mDpToPx, 0);
        mEventFilter.onTouchEventInternal(motionEvent);
    }

    /**
     * Simulates a MotionEvent.ACTION_DOWN in the OverlayPanelEventFilter.
     *
     * @param x The event's x coordinate in dps.
     * @param y The event's y coordinate in dps.
     */
    private void simulateActionDownEvent(float x, float y) {
        simulateEvent(MotionEvent.ACTION_DOWN, x, y);
    }

    /**
     * Simulates a MotionEvent.ACTION_MOVE in the OverlayPanelEventFilter.
     *
     * @param x The event's x coordinate in dps.
     * @param y The event's y coordinate in dps.
     */
    private void simulateActionMoveEvent(float x, float y) {
        simulateEvent(MotionEvent.ACTION_MOVE, x, y);
    }

    /**
     * Simulates a MotionEvent.ACTION_UP in the OverlayPanelEventFilter.
     *
     * @param x The event's x coordinate in dps.
     * @param y The event's y coordinate in dps.
     */
    private void simulateActionUpEvent(float x, float y) {
        simulateEvent(MotionEvent.ACTION_UP, x, y);
    }
}
