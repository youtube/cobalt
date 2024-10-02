// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.graphics.Point;
import android.os.SystemClock;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.util.Objects;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for verifying the behavior of various fullscreen actions.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Batch(Batch.PER_CLASS)
public class FullscreenManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String LONG_HTML_WITH_AUTO_FOCUS_INPUT_TEST_PAGE =
            UrlUtils.encodeHtmlDataUri("<html>"
                    + "<body style='height:10000px;'>"
                    + "<p>The text input is focused automatically on load."
                    + " The browser controls should not hide when page is scrolled.</p><br/>"
                    + "<input id=\"input_text\" type=\"text\" autofocus/>"
                    + "</body>"
                    + "</html>");

    private static final String LONG_HTML_TEST_PAGE =
            UrlUtils.encodeHtmlDataUri("<html><body style='height:100000px;'></body></html>");
    private static final String LONG_FULLSCREEN_API_HTML_TEST_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "<head>"
            + "  <meta name=\"viewport\" "
            + "    content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" />"
            + "  <script>"
            + "    function toggleFullScreen() {"
            + "      if (document.webkitIsFullScreen) {"
            + "        document.webkitCancelFullScreen();"
            + "      } else {"
            + "        document.body.webkitRequestFullScreen();"
            + "      }"
            + "    };"
            + "  </script>"
            + "  <style>"
            + "    body:-webkit-full-screen { background: red; width: 100%; }"
            + "  </style>"
            + "</head>"
            + "<body style='height:10000px;' onclick='toggleFullScreen();'>"
            + "</body>"
            + "</html>");
    private static final String LONG_FULLSCREEN_API_HTML_WITH_OPTIONS_TEST_PAGE =
            UrlUtils.encodeHtmlDataUri("<html>"
                    + "<head>"
                    + "  <meta name=\"viewport\" "
                    + "    content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" />"
                    + "  <script>"
                    + "    var mode = 0;"
                    + "    function toggleFullScreen() {"
                    + "      if (mode == 0) {"
                    + "        document.body.requestFullscreen({navigationUI: \"show\"});"
                    + "        mode++;"
                    + "      } else if (mode == 2) {"
                    + "        document.body.requestFullscreen({navigationUI: \"hide\"});"
                    + "        mode++;"
                    + "      } else if (mode == 1 || mode == 3) {"
                    + "        document.exitFullscreen();"
                    + "        mode++;"
                    + "      }"
                    + "    };"
                    + "  </script>"
                    + "  <style>"
                    + "    body:-webkit-full-screen { background: red; width: 100%; }"
                    + "  </style>"
                    + "</head>"
                    + "<body style='height:10000px;' onclick='toggleFullScreen();'>"
                    + "</body>"
                    + "</html>");
    private static final String SCROLL_OFFSET_TEST_PAGE = UrlUtils.encodeHtmlDataUri("<html>"
            + "<head>"
            + "  <meta name=viewport content='width=device-width, initial-scale=1.0'>"
            + "</head>"
            + "<body style='margin: 0; height: 200vh'>"
            + "  <div style='width: 150vw'>wide</div>"
            + "  <script>"
            + "    load_promise = new Promise(r => {onload = r});"
            + "    resize_promise = null;"
            + "    reached_bottom = () => {"
            + "      return Math.abs("
            + "        (se => se.scrollHeight - (se.scrollTop + visualViewport.offsetTop +"
            + "          visualViewport.height))(document.scrollingElement)"
            + "      ) < 1;"
            + "    };"
            + "    start_listening_for_on_resize = () => {"
            + "      resize_promise = new Promise(r => {onresize = r});"
            + "      return true;"
            + "    };"
            + "  </script>"
            + "</body>"
            + "</html>");

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> TabStateBrowserControlsVisibilityDelegate.disablePageLoadDelayForTests());
    }

    @Test
    @MediumTest
    @Feature({"Fullscreen"})
    public void testTogglePersistentFullscreen() {
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);

        FullscreenTestUtils.waitForFullscreenFlag(tab, false, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mActivityTestRule.getActivity());

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, false, mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @Feature({"Fullscreen"})
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testBackPressExitPersistentFullscreen() {
        testBackPressExitPersistentFullscreenInternal();
    }

    @Test
    @MediumTest
    @Feature({"Fullscreen"})
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testBackPressExitPersistentFullscreen_backGestureRefactor() {
        testBackPressExitPersistentFullscreenInternal();
    }

    private void testBackPressExitPersistentFullscreenInternal() {
        launchOnFullscreenMode(LONG_HTML_TEST_PAGE);
        Assert.assertTrue(getPersistentFullscreenMode());

        Espresso.pressBack();

        Assert.assertFalse(getPersistentFullscreenMode());
    }

    @Test
    @MediumTest
    @Feature({"Fullscreen"})
    public void testDelayedPersistentFullscreen() {
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);
        FullscreenTestUtils.waitForFullscreenFlag(tab, false, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);

        // Open a new tab, which puts the tab to test background.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        // Having the background tab enter fullscreen should be delayed until it comes foreground.
        FullscreenTestUtils.togglePersistentFullscreen(delegate, true);
        Assert.assertFalse(getPersistentFullscreenMode());

        // Put the tab foreground and assert the fullscreen was entered.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), tab.getId());
        Assert.assertEquals(tab, mActivityTestRule.getActivity().getActivityTab());
        Assert.assertTrue(getPersistentFullscreenMode());
    }

    private boolean getPersistentFullscreenMode() {
        boolean b1 = TestThreadUtils.runOnUiThreadBlockingNoException(
                mActivityTestRule.getActivity()
                        .getFullscreenManager()::getPersistentFullscreenMode);
        Boolean b2 = TestThreadUtils.runOnUiThreadBlockingNoException(
                mActivityTestRule.getActivity()
                        .getFullscreenManager()
                        .getPersistentFullscreenModeSupplier()::get);
        Assert.assertTrue("Fullscreen mode supplier is holding a different value.",
                (b2 == null && !b1) || Objects.equals(b1, b2));
        return b1;
    }

    private void launchOnFullscreenMode(String url) {
        mActivityTestRule.startMainActivityWithURL(url);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);

        FullscreenTestUtils.waitForFullscreenFlag(tab, false, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mActivityTestRule.getActivity());
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    public void testPersistentFullscreenChangingUiFlags() throws InterruptedException {
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);

        FullscreenTestUtils.waitForFullscreenFlag(tab, false, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mActivityTestRule.getActivity());

        // There is a race condition in android when setting various system UI flags.
        // Adding this wait to allow the animation transitions to complete before continuing
        // the test (See https://b/10387660)
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            View view = tab.getContentView();
            view.setSystemUiVisibility(
                    view.getSystemUiVisibility() & ~View.SYSTEM_UI_FLAG_FULLSCREEN);
        });
        FullscreenTestUtils.waitForFullscreenFlag(tab, true, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, true);
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    public void testFullscreenOptionsUpdatedCorrectly() throws InterruptedException {
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);

        FullscreenTestUtils.waitForFullscreenFlag(tab, false, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);

        // Enter fullscreen w/ all system UI hidden:
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mActivityTestRule.getActivity(), false, false);

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        // We should be in fullscreen, navigation should be hidden:
        FullscreenTestUtils.waitForFullscreenFlag(tab, true, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForHideNavigationFlag(tab, true, mActivityTestRule.getActivity());

        // Adjust the fullscreen options to show navigation bar mid-fullscreen:
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mActivityTestRule.getActivity(), true, false);

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        // We should be in fullscreen, navigation should be visible:
        FullscreenTestUtils.waitForFullscreenFlag(tab, true, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForHideNavigationFlag(tab, false, mActivityTestRule.getActivity());

        // Adjust the fullscreen options to show status bar mid-fullscreen:
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mActivityTestRule.getActivity(), false, true);

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        // We should not be in fullscreen, navigation should be hidden:
        FullscreenTestUtils.waitForFullscreenFlag(tab, false, mActivityTestRule.getActivity());
        FullscreenTestUtils.waitForHideNavigationFlag(tab, true, mActivityTestRule.getActivity());
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    @DisabledTest(message = "crbug.com/1046749")
    public void testExitPersistentFullscreenAllowsManualFullscreen() {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(LONG_FULLSCREEN_API_HTML_TEST_PAGE);

        BrowserControlsManager browserControlManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        int browserControlsHeight = browserControlManager.getTopControlsHeight();

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        View view = tab.getView();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);

        TouchCommon.singleClickView(view);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, true);

        FullscreenManagerTestUtils.waitForBrowserControlsPosition(
                mActivityTestRule, -browserControlsHeight);

        TestTouchUtils.sleepForDoubleTapTimeout(InstrumentationRegistry.getInstrumentation());
        TouchCommon.singleClickView(view);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(mActivityTestRule, 0);

        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, false);
        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, true);
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    public void testManualHidingShowingBrowserControls() {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        final BrowserControlsManager browserControlsManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();

        CriteriaHelper.pollUiThread(
                () -> { return browserControlsManager.getTopControlOffset() == 0f; });

        FullscreenManagerTestUtils.waitForBrowserControlsToBeMoveable(
                mActivityTestRule, mActivityTestRule.getActivity().getActivityTab());

        // Check that the URL bar has not grabbed focus (http://crbug/236365)
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertFalse("Url bar grabbed focus", urlBar.hasFocus());
    }

    @Test
    //@LargeTest
    @DisabledTest(message = "crbug.com/901280")
    public void testHideBrowserControlsAfterFlingBoosting() {
        // Test that fling boosting doesn't break the scroll state management
        // that's used by the BrowserControlsManager to dispatch URL bar based
        // resizes to the renderer.
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        FullscreenManagerTestUtils.waitForBrowserControlsToBeMoveable(
                mActivityTestRule, mActivityTestRule.getActivity().getActivityTab());

        final CallbackHelper flingEndCallback = new CallbackHelper();
        final CallbackHelper scrollStartCallback = new CallbackHelper();
        GestureStateListener scrollListener = new GestureStateListener() {
            @Override
            public void onScrollStarted(
                    int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                scrollStartCallback.notifyCalled();
            }

            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                flingEndCallback.notifyCalled();
            }
        };

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        GestureListenerManager gestureListenerManager =
                WebContentsUtils.getGestureListenerManager(tab.getWebContents());
        gestureListenerManager.addListener(scrollListener);

        final CallbackHelper viewportCallback = new CallbackHelper();

        Assert.assertEquals(0, scrollStartCallback.getCallCount());
        Assert.assertEquals(0, viewportCallback.getCallCount());

        // Start the first fling.
        FullscreenManagerTestUtils.fling(mActivityTestRule, 0, -2000);

        // Wait until we hear the gesture scroll begin before we try to fling
        // again since we'll hit DCHECKs in the fling controller state
        // management.
        try {
            scrollStartCallback.waitForCallback(0, 1, 1000, TimeUnit.MILLISECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Timeout waiting for scroll to start");
        }

        // Fling again while the first fling is still active. This will boost
        // the first fling.
        FullscreenManagerTestUtils.fling(mActivityTestRule, 0, -2000);

        Assert.assertEquals(0, flingEndCallback.getCallCount());
        Assert.assertTrue(gestureListenerManager.isScrollInProgress());

        try {
            flingEndCallback.waitForCallback(0, 1, 5000, TimeUnit.MILLISECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Timeout waiting for scroll to end");
        }

        // Make sure we call the viewport changed callback since the URL bar was hidden.
        // Can be called once for the FlingEnd and once for the ScrollEnd.
        try {
            viewportCallback.waitForCallback(0, 1, 500, TimeUnit.MILLISECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Failed to update viewport");
        }

        // Ensure we don't still think we're scrolling.
        Assert.assertFalse(
                "Failed to reset scrolling state", gestureListenerManager.isScrollInProgress());
    }

    @Test
    @LargeTest
    public void testHidingBrowserControlsPreservesScrollOffset() throws TimeoutException {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(SCROLL_OFFSET_TEST_PAGE);

        ChromeActivity activity = mActivityTestRule.getActivity();
        WebContents webContents = activity.getActivityTab().getWebContents();

        // Browser startup generates resize events as part of compositor initialization. Depending
        // on the relative timing of that initialization and the initial navigation, the test page
        // may receive these onResize events. To ensure that the test page's onResize handler
        // triggers only for the fling we initiate below, we tell the test page to start listening
        // for onResize only now that browser startup has fully completed (note that
        // startMainActivityWithURL() waits for full browser initialization before returning, and
        // hence the renderer's processing of the earlier resize events will be ordered before its
        // reception of the message sent below).
        JavaScriptUtils.runJavascriptWithAsyncResult(
                webContents, "domAutomationController.send(start_listening_for_on_resize());");

        BrowserControlsManager browserControlsManager = activity.getBrowserControlsManager();
        CriteriaHelper.pollUiThread(
                () -> { return browserControlsManager.getTopControlOffset() == 0f; });

        Point displaySize = new Point();
        activity.getWindowManager().getDefaultDisplay().getSize(displaySize);

        FullscreenManagerTestUtils.waitForPageToBeScrollable(activity.getActivityTab());

        JavaScriptUtils.runJavascriptWithAsyncResult(
                webContents, "load_promise.then(() => { domAutomationController.send(true); });");

        FullscreenManagerTestUtils.fling(mActivityTestRule, 0, -displaySize.y * 20);
        Assert.assertEquals("true",
                JavaScriptUtils.runJavascriptWithAsyncResult(webContents,
                        "resize_promise.then(() => {"
                                + "  domAutomationController.send(reached_bottom());"
                                + "});"));
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    @DisabledTest(message = "Flaky. crbug.com/936252")
    public void testManualFullscreenDisabledForChromePages() {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        // The credits page was chosen as it is a chrome:// page that is long and would support
        // manual fullscreen if it were supported.
        mActivityTestRule.startMainActivityWithURL("chrome://credits");

        final BrowserControlsManager browserControlsManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        int browserControlsHeight = browserControlsManager.getTopControlsHeight();

        Assert.assertEquals(browserControlsManager.getTopControlOffset(), 0f, 0);

        float dragX = 50f;
        float dragStartY = browserControlsHeight * 2;
        float dragFullY = dragStartY - browserControlsHeight;

        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(mActivityTestRule.getActivity(), dragX, dragStartY, downTime);
        TouchCommon.dragTo(mActivityTestRule.getActivity(), dragX, dragX, dragStartY, dragFullY,
                100, downTime);
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(mActivityTestRule, 0);
        TouchCommon.dragEnd(mActivityTestRule.getActivity(), dragX, dragFullY, downTime);
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(mActivityTestRule, 0);
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testControlsShownOnUnresponsiveRenderer() {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        BrowserControlsManager browserControlsManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        CriteriaHelper.pollUiThread(
                () -> { return browserControlsManager.getTopControlOffset() == 0f; });

        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, false);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, delegate::rendererUnresponsive);
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(mActivityTestRule, 0);

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, delegate::rendererResponsive);

        // TODO(tedchoc): This is running into timing issues with the renderer offset logic.
        // waitForBrowserControlsToBeMoveable(getActivity().getActivityTab());
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    @DisabledTest(message = "https://crbug.com/1099447")
    public void testControlsShownOnUnresponsiveRendererUponExitingTabSwitcherMode()
            throws Exception {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_TEST_PAGE);

        BrowserControlsManager browserControlsManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        CriteriaHelper.pollUiThread(
                () -> { return browserControlsManager.getTopControlOffset() == 0f; });

        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, false);

        setTabSwitcherModeAndWait(true);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, delegate::rendererUnresponsive);
        setTabSwitcherModeAndWait(false);

        FullscreenManagerTestUtils.waitForBrowserControlsPosition(mActivityTestRule, 0);

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, delegate::rendererResponsive);
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    public void testEnterPendingPersistentFullscreen() {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(LONG_FULLSCREEN_API_HTML_TEST_PAGE);

        // Tests entering fullscreen when browser controls are visible. The request goes through
        // after the controls are hidden.
        BrowserControlsManager browserControlsManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        CriteriaHelper.pollUiThread(
                () -> { return browserControlsManager.getTopControlOffset() == 0f; });

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TouchCommon.singleClickView(tab.getView());
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, true);
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    @DisabledTest(message = "crbug.com/698413")
    public void testBrowserControlsShownWhenInputIsFocused() throws TimeoutException {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(LONG_HTML_WITH_AUTO_FOCUS_INPUT_TEST_PAGE);

        BrowserControlsManager browserControlsManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        CriteriaHelper.pollUiThread(
                () -> { return browserControlsManager.getTopControlOffset() == 0f; });

        int browserControlsHeight = browserControlsManager.getTopControlsHeight();
        float dragX = 50f;
        float dragStartY = browserControlsHeight * 3;
        float dragEndY = dragStartY - browserControlsHeight * 2;
        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(mActivityTestRule.getActivity(), dragX, dragStartY, downTime);
        TouchCommon.dragTo(
                mActivityTestRule.getActivity(), dragX, dragX, dragStartY, dragEndY, 100, downTime);
        TouchCommon.dragEnd(mActivityTestRule.getActivity(), dragX, dragEndY, downTime);
        Assert.assertEquals(browserControlsManager.getTopControlOffset(), 0f, 0);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TouchCommon.singleClickView(tab.getView());
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(),
                "document.getElementById('input_text').blur();");
        waitForEditableNodeToLoseFocus(tab);

        FullscreenManagerTestUtils.waitForBrowserControlsToBeMoveable(
                mActivityTestRule, mActivityTestRule.getActivity().getActivityTab());
    }

    @Test
    @LargeTest
    @Feature({"Fullscreen"})
    @DisabledTest(message = "crbug.com/979189")
    public void testPersistentFullscreenWithOptions() {
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.startMainActivityWithURL(LONG_FULLSCREEN_API_HTML_WITH_OPTIONS_TEST_PAGE);

        BrowserControlsManager browserControlsManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        int browserControlsHeight = browserControlsManager.getTopControlsHeight();

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        View view = tab.getView();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);

        TouchCommon.singleClickView(view);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, true);

        FullscreenManagerTestUtils.waitForBrowserControlsPosition(
                mActivityTestRule, -browserControlsHeight);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Navigation bar not hidden.",
                    (view.getSystemUiVisibility() & View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION)
                            == 0);
        });

        TestTouchUtils.sleepForDoubleTapTimeout(InstrumentationRegistry.getInstrumentation());
        TouchCommon.singleClickView(view);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(mActivityTestRule, 0);

        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, false);
        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, true);

        TestTouchUtils.sleepForDoubleTapTimeout(InstrumentationRegistry.getInstrumentation());
        TouchCommon.singleClickView(view);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, true);
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(
                mActivityTestRule, -browserControlsHeight);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Navigation bar hidden.",
                    (view.getSystemUiVisibility() & View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION)
                            != 0);
        });
        TestTouchUtils.sleepForDoubleTapTimeout(InstrumentationRegistry.getInstrumentation());
        TouchCommon.singleClickView(view);
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(mActivityTestRule, 0);
    }

    private void waitForEditableNodeToLoseFocus(final Tab tab) {
        CriteriaHelper.pollUiThread(() -> {
            SelectionPopupController controller =
                    SelectionPopupController.fromWebContents(tab.getWebContents());
            return !controller.isFocusedNodeEditable();
        });
    }

    /**
     * Enter or exit the tab switcher with animations and wait for the scene to change.
     * @param inSwitcher Whether to enter or exit the tab switcher.
     */
    private void setTabSwitcherModeAndWait(boolean inSwitcher) {
        LayoutManager layoutManager = mActivityTestRule.getActivity().getLayoutManager();
        @LayoutType
        int layout = inSwitcher ? LayoutType.TAB_SWITCHER : LayoutType.BROWSING;
        LayoutTestUtils.startShowingAndWaitForLayout(layoutManager, layout, false);
    }
}
