// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;

/** Unit tests for {@link FeedStreamViewResizer}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class FeedStreamViewResizerTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    private Activity mActivity;
    @Mock
    private RecyclerView mRecyclerView;
    @Mock
    private UiConfig mUiConfig;

    private FeedStreamViewResizer mResizer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();

        when(mUiConfig.getContext()).thenReturn(mActivity);
        mResizer = FeedStreamViewResizer.createAndAttach(mActivity, mRecyclerView, mUiConfig);
        mResizer.onDisplayStyleChanged(
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR));
    }

    @Config(qualifiers = "sw600dp-w600dp")
    @EnableFeatures(ChromeFeatureList.FEED_MULTI_COLUMN)
    @Test
    public void computePaddingWidthLessThan840dpMultiColumnEnabled() {
        // expectedPadding = mMinWidePaddingPixels = 48
        int expectedPadding = 48;
        assertPaddingEquals(expectedPadding);
    }

    @Config(qualifiers = "sw840dp-w840dp")
    @EnableFeatures(ChromeFeatureList.FEED_MULTI_COLUMN)
    @Test
    public void computePaddingWidth840dpMultiColumnEnabled() {
        // expectedPadding = mMinWidePaddingPixels = 48
        int expectedPadding = 48;
        assertPaddingEquals(expectedPadding);
    }

    @Config(qualifiers = "sw1220dp-w1220dp")
    @EnableFeatures(ChromeFeatureList.FEED_MULTI_COLUMN)
    @Test
    public void computePaddingWidth1220dpMultiColumnEnabled() {
        // expectedPadding = max((1220-ntp_wide_card_width_breakpoint)/2, mMinWidePaddingPixels) =
        // max(190, 48)
        int expectedPadding = 190;
        assertPaddingEquals(expectedPadding);
    }

    @Config(qualifiers = "sw1820dp-w1820dp")
    @EnableFeatures(ChromeFeatureList.FEED_MULTI_COLUMN)
    @Test
    public void computePaddingWidth1820dpMultiColumnEnabled() {
        // expectedPadding = max(ntp_wide_card_lateral_margins_max,
        // (1820-ntp_wide_card_width_max)/2) = max(200, (1820-1200)/2) = 310
        int expectedPadding = 310;
        assertPaddingEquals(expectedPadding);
    }

    @Config(qualifiers = "w390dp-h820dp-port")
    @Test
    public void computePaddingPhonePortrait() {
        mResizer.onDisplayStyleChanged(
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR));
        int expectedPadding = 0;
        assertPaddingEquals(expectedPadding);
    }

    @Config(qualifiers = "w390dp-h820dp-land")
    @Test
    public void computePaddingPhoneLandscape() {
        mResizer.onDisplayStyleChanged(
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR));
        // expectedPadding = ((width - usableHeight * 1.778) / 2) = (820 - (390*1.778))/2 = 63;
        int expectedPadding = 63;
        assertPaddingEquals(expectedPadding);
    }

    @Config(qualifiers = "sw840dp-w840dp")
    @EnableFeatures(ChromeFeatureList.FEED_MULTI_COLUMN)
    @Test
    public void computePaddingWidth840dpNonWideDisplayMultiColumnEnabled() {
        shadowOf(mActivity).setInMultiWindowMode(true);
        mResizer.onDisplayStyleChanged(
                new DisplayStyle(HorizontalDisplayStyle.NARROW, VerticalDisplayStyle.REGULAR));
        // expectedPadding = mDefaultPaddingPixels = 0
        int expectedPadding = 0;
        assertPaddingEquals(expectedPadding);
    }

    private void assertPaddingEquals(int expectedPadding) {
        int padding = mResizer.computePadding();
        assertEquals(expectedPadding, padding);
    }
}
