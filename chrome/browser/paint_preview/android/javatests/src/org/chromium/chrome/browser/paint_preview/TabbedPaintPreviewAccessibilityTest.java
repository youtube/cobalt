// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.os.SystemClock;
import android.view.MotionEvent;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Accessibility tests for the {@link TabbedPaintPreview} class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabbedPaintPreviewAccessibilityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_URL = "/chrome/test/data/android/about.html";

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(
                mActivityTestRule.getTestServer().getURL(TEST_URL));
        PaintPreviewTabService.setAccessibilityEnabledForTesting(true);
    }

    @After
    public void tearDown() {
        PaintPreviewTabService.setAccessibilityEnabledForTesting(false);
    }

    /**
     * Asserts that the player has a non-null {@link WebContentsAccessibility}.
     */
    @Test
    @MediumTest
    public void smokeTest() throws ExecutionException, TimeoutException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));

        // Capture paint preview.
        CallbackHelper captureSuccessCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> tabbedPaintPreview.capture(result -> {
            if (result) {
                captureSuccessCallback.notifyCalled();
            } else {
                Assert.fail("Failed to capture paint preview.");
            }
        }));
        captureSuccessCallback.waitForFirst("Timeout waiting for paint preview capture.");

        // Show the captured paint preview.
        CallbackHelper viewReadyCallback = new CallbackHelper();
        PlayerManager.Listener listener = new TabbedPaintPreviewTest.EmptyPlayerListener() {
            @Override
            public void onCompositorError(int status) {
                Assert.fail("Paint Preview should have been displayed successfully"
                        + "with no errors.");
            }

            @Override
            public void onViewReady() {
                viewReadyCallback.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> tabbedPaintPreview.maybeShow(listener));

        // Wait until it's displayed.
        viewReadyCallback.waitForFirst("Paint preview view ready never happened.");

        // Assert accessibility support is initialized.
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> tabbedPaintPreview.getPlayerManagerForTesting()
                                   .getWebContentsAccessibilityForTesting()
                        != null,
                "PlayerManager doesn't have a valid WebContentsAccessibility.");

        // Try hit testing.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebContentsAccessibility wcax = tabbedPaintPreview.getPlayerManagerForTesting()
                                                    .getWebContentsAccessibilityForTesting();
            wcax.setAccessibilityEnabledForTesting();
            long time = SystemClock.uptimeMillis();
            MotionEvent e =
                    MotionEvent.obtain(time, time, MotionEvent.ACTION_HOVER_ENTER, 20, 20, 0);
            wcax.onHoverEventNoRenderer(e);
        });

        // Remove the preview.
        TestThreadUtils.runOnUiThreadBlocking(() -> tabbedPaintPreview.remove(true, false));
        CriteriaHelper.pollUiThread(
                () -> !tabbedPaintPreview.isAttached(), "Paint Preview not removed.");
    }
}
