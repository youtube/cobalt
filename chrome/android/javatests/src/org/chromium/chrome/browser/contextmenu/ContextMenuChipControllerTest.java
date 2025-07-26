// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.embedder_support.contextmenu.ChipRenderParams;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for ContextMenuHeader view and {@link ContextMenuHeaderViewBinder} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ContextMenuChipControllerTest {
    // This is the combination of the expected vertical margins and the chip height.
    private static final int EXPECTED_VERTICAL_DP = 80;
    // Computed by taking the 338dp max width and subtracting:
    // 16 (chip start padding)
    // 24 (main icon width)
    // 8 (text start padding)
    // 16 (close button start padding)
    // 24 (close button icon width)
    // 16 (close button end padding)
    private static final int EXPECTED_CHIP_WIDTH_DP = 234;
    // Computed by taking the 338dp max width and subtracting:
    // 16 (chip start padding)
    // 24 (main icon width)
    // 8 (text start padding)
    private static final int EXPECTED_CHIP_NO_END_BUTTON_WIDTH_DP = 290;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Mock private Runnable mMockChipClickRunnable;

    @Mock private Runnable mMockDismissRunnable;

    private float mMeasuredDeviceDensity;
    private View mAnchorView;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mMeasuredDeviceDensity = sActivity.getResources().getDisplayMetrics().density;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.setContentView(R.layout.context_menu_fullscreen_container);
                    mAnchorView = sActivity.findViewById(R.id.context_menu_chip_anchor_point);
                });
    }

    @Test
    @SmallTest
    public void testDismissChipWhenNotShownBeforeClassificationReturned() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(sActivity, mAnchorView, mMockDismissRunnable);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    chipController.dismissChipIfShowing();
                });

        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNull(
                "Popup window was initialized unexpectedly.",
                chipController.getCurrentPopupWindowForTesting());
    }

    @Test
    @SmallTest
    public void testDismissChipWhenShown() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(sActivity, mAnchorView, mMockDismissRunnable);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChipRenderParams chipRenderParams = new ChipRenderParams();
                    chipRenderParams.titleResourceId =
                            R.string.contextmenu_shop_image_with_google_lens;
                    chipRenderParams.iconResourceId = R.drawable.lens_icon;
                    chipRenderParams.onClickCallback = mMockChipClickRunnable;
                    chipController.showChip(chipRenderParams);
                    chipController.dismissChipIfShowing();
                });

        verify(mMockDismissRunnable, never()).run();
        verify(mMockChipClickRunnable, never()).run();
        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNotNull(
                "Popup window was not initialized.",
                chipController.getCurrentPopupWindowForTesting());
        assertFalse(
                "Popup window showing unexpectedly.",
                chipController.getCurrentPopupWindowForTesting().isShowing());
    }

    @Test
    @SmallTest
    public void testClickChipWhenShown() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(sActivity, mAnchorView, mMockDismissRunnable);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChipRenderParams chipRenderParams = new ChipRenderParams();
                    chipRenderParams.titleResourceId =
                            R.string.contextmenu_shop_image_with_google_lens;
                    chipRenderParams.iconResourceId = R.drawable.lens_icon;
                    chipRenderParams.onClickCallback = mMockChipClickRunnable;
                    chipController.showChip(chipRenderParams);
                    chipController.clickChipForTesting();
                });

        verify(mMockDismissRunnable, times(1)).run();
        verify(mMockChipClickRunnable, times(1)).run();
        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNotNull(
                "Popup window was not initialized.",
                chipController.getCurrentPopupWindowForTesting());
        assertTrue(
                "Dismiss was mocked so the popup window should still be showing.",
                chipController.getCurrentPopupWindowForTesting().isShowing());
    }

    @Test
    @SmallTest
    public void testExpectedVerticalPxNeededForChip() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(sActivity, mAnchorView, mMockDismissRunnable);
        assertEquals(
                "Vertical px is not matching the expectation",
                (int) Math.round(EXPECTED_VERTICAL_DP * mMeasuredDeviceDensity),
                chipController.getVerticalPxNeededForChip());
    }

    @Test
    @SmallTest
    public void testExpectedChipTextMaxWidthPx() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(sActivity, mAnchorView, mMockDismissRunnable);
        assertEquals(
                "Chip width px is not matching the expectation",
                (int) Math.round(EXPECTED_CHIP_WIDTH_DP * mMeasuredDeviceDensity),
                chipController.getChipTextMaxWidthPx(false));
    }

    @Test
    @SmallTest
    public void testExpectedChipTextMaxWidthPx_EndButtonHidden() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(sActivity, mAnchorView, mMockDismissRunnable);
        assertEquals(
                "Chip width px is not matching the expectation",
                (int) Math.round(EXPECTED_CHIP_NO_END_BUTTON_WIDTH_DP * mMeasuredDeviceDensity),
                chipController.getChipTextMaxWidthPx(true));
    }
}
