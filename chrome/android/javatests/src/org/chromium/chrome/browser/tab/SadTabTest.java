// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.widget.Button;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManagerTestUtils;
import org.chromium.chrome.browser.tab.TabUtils.LoadIfNeededCaller;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.ExecutionException;

/** Tests related to the sad tab logic. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SadTabTest {
    private static final String LONG_HTML_TEST_PAGE =
            UrlUtils.encodeHtmlDataUri("<html><body style='height:100000px;'></body></html>");

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static boolean isShowingSadTab(Tab tab) {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> SadTab.isShowing(tab));
        } catch (ExecutionException e) {
            return false;
        }
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = sActivityTestRule.getActivity().getActivityTab();
                    tab.show(TabSelectionType.FROM_USER, LoadIfNeededCaller.OTHER);
                    SadTab sadTab = SadTab.from(tab);
                    sadTab.removeIfPresent();
                });
    }

    /** Verify that the sad tab is shown when the renderer crashes. */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabShownWhenRendererProcessKilled() {
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(isShowingSadTab(tab));
        simulateRendererKilled(tab, true);
        Assert.assertTrue(isShowingSadTab(tab));
    }

    /**
     * Verify that the sad tab is not shown when the renderer crashes in the background or the
     * renderer was killed by the OS out-of-memory killer.
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabNotShownWhenRendererProcessKilledInBackround() {
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(isShowingSadTab(tab));
        simulateRendererKilled(tab, false);
        Assert.assertFalse(isShowingSadTab(tab));
    }

    /** Verify that a tab navigating to a page that is killed in the background is reloaded. */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabReloadAfterKill() throws Throwable {
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();

        TestWebServer webServer = TestWebServer.start();
        try {
            final String url1 = webServer.setEmptyResponse("/page1.html");
            sActivityTestRule.loadUrl(url1);
            Assert.assertFalse(tab.needsReload());
            simulateRendererKilled(tab, false);
            Assert.assertTrue(tab.needsReload());
        } finally {
            webServer.shutdown();
        }
    }

    /** Verify that a tab killed in the background is not reloaded if another load has started. */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabNoReloadAfterLoad() throws Throwable {
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();

        TestWebServer webServer = TestWebServer.start();
        try {
            final String url1 = webServer.setEmptyResponse("/page1.html");
            final String url2 = webServer.setEmptyResponse("/page2.html");
            sActivityTestRule.loadUrl(url1);
            Assert.assertFalse(tab.needsReload());
            simulateRendererKilled(tab, false);
            sActivityTestRule.loadUrl(url2);
            Assert.assertFalse(tab.needsReload());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Confirm that after a successive refresh of a failed tab that failed to load, change the
     * button from "Reload" to "Send Feedback". If reloaded a third time and it is successful it
     * reverts from "Send Feedback" to "Reload".
     *
     * @throws IllegalArgumentException
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabPageButtonText() throws IllegalArgumentException {
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(isShowingSadTab(tab));
        simulateRendererKilled(tab, true);
        Assert.assertTrue(isShowingSadTab(tab));
        String actualText = getSadTabButton(tab).getText().toString();
        Assert.assertEquals(
                "Expected the sad tab button to have the reload label",
                sActivityTestRule.getActivity().getString(R.string.sad_tab_reload_label),
                actualText);

        reloadSadTab(tab);
        Assert.assertTrue(isShowingSadTab(tab));
        actualText = getSadTabButton(tab).getText().toString();
        Assert.assertTrue(showSendFeedbackView(tab));
        Assert.assertEquals(
                "Expected the sad tab button to have the feedback label after the tab button "
                        + "crashes twice in a row.",
                sActivityTestRule.getActivity().getString(R.string.sad_tab_send_feedback_label),
                actualText);
        sActivityTestRule.loadUrl("about:blank");
        Assert.assertFalse(
                "Expected about:blank to destroy the sad tab however the sad tab is still in "
                        + "view",
                isShowingSadTab(tab));
        simulateRendererKilled(tab, true);
        actualText = getSadTabButton(tab).getText().toString();
        Assert.assertEquals(
                "Expected the sad tab button to have the reload label after a successful load",
                sActivityTestRule.getActivity().getString(R.string.sad_tab_reload_label),
                actualText);
    }

    @Test
    @MediumTest
    @Feature({"SadTab"})
    @DisabledTest(message = "https://crbug.com/1447840")
    public void testSadTabBrowserControlsVisibility() {
        TestThreadUtils.runOnUiThreadBlocking(
                TabStateBrowserControlsVisibilityDelegate::disablePageLoadDelayForTests);
        FullscreenManagerTestUtils.disableBrowserOverrides();
        sActivityTestRule.loadUrl(LONG_HTML_TEST_PAGE);
        FullscreenManagerTestUtils.waitForBrowserControlsToBeMoveable(
                sActivityTestRule, sActivityTestRule.getActivity().getActivityTab());
        FullscreenManagerTestUtils.scrollBrowserControls(sActivityTestRule, false);
        simulateRendererKilled(sActivityTestRule.getActivity().getActivityTab(), true);
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(sActivityTestRule, 0);
    }

    /** Helper method that kills the renderer on a UI thread. */
    private static void simulateRendererKilled(final Tab tab, final boolean visible) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (!visible) tab.hide(TabHidingType.CHANGED_TABS);
                    ChromeTabUtils.simulateRendererKilledForTesting(tab);
                });
    }

    /** Helper method that reloads a tab with a SadTabView currently displayed. */
    private static void reloadSadTab(final Tab tab) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SadTab sadTab = SadTab.from(tab);
                    sadTab.removeIfPresent();
                    sadTab.show(tab.getContext(), () -> {}, () -> {});
                });
    }

    private static boolean showSendFeedbackView(final Tab tab) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> SadTab.from(tab).showSendFeedbackView());
        } catch (ExecutionException e) {
            return false; // Make tests fail when an exception is thrown.
        }
    }

    /**
     * If there is a SadTabView, this method will get the button for the sad tab.
     *
     * @param tab The tab that needs to contain a SadTabView.
     * @return Returns the button that is on the SadTabView, null if SadTabView. doesn't exist.
     */
    private static Button getSadTabButton(Tab tab) {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> tab.getView().findViewById(R.id.sad_tab_button));
        } catch (ExecutionException e) {
            return null;
        }
    }
}
