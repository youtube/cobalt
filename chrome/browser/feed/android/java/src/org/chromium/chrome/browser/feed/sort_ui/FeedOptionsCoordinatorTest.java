// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sort_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.FEED_HEADER_STICK_TO_TOP;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.v2.ContentOrder;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link FeedOptionsCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.FEED_HEADER_STICK_TO_TOP})
public class FeedOptionsCoordinatorTest {
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock
    private FeedOptionsView mView;
    @Mock
    private FeedOptionsView mStickyHeaderOptionsView;
    @Mock
    private ChipView mChipView;
    @Mock
    private ChipView mStickyHeaderChipView;
    @Mock
    private TextView mTextView;
    @Mock
    private TextView mStickyHeaderTextView;

    @Rule
    public JniMocker mMocker = new JniMocker();

    private FeedOptionsCoordinator mCoordinator;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mMocker.mock(FeedServiceBridge.getTestHooksForTesting(), mFeedServiceBridgeJniMock);
        when(mFeedServiceBridgeJniMock.getContentOrderForWebFeed())
                .thenReturn(ContentOrder.REVERSE_CHRON);
        when(mView.createNewChip()).thenReturn(mChipView);
        when(mStickyHeaderOptionsView.createNewChip()).thenReturn(mStickyHeaderChipView);
        when(mChipView.getPrimaryTextView()).thenReturn(mTextView);
        when(mStickyHeaderChipView.getPrimaryTextView()).thenReturn(mStickyHeaderTextView);

        mCoordinator = new FeedOptionsCoordinator(mContext, mView, mStickyHeaderOptionsView);
    }

    @Test
    public void testToggleVisibility_turnoff() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, true);

        mCoordinator.toggleVisibility();

        assertFalse(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
    }

    @Test
    public void testToggleVisibility_turnon() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, false);

        mCoordinator.toggleVisibility();

        assertTrue(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
        assertFalse(mCoordinator.getChipModelsForTest().get(0).get(ChipProperties.SELECTED));
        assertTrue(mCoordinator.getChipModelsForTest().get(1).get(ChipProperties.SELECTED));
    }

    @Test
    public void testToggleVisibility_turnon_updateOrder() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, false);
        when(mFeedServiceBridgeJniMock.getContentOrderForWebFeed())
                .thenReturn(ContentOrder.GROUPED);

        mCoordinator.toggleVisibility();

        assertTrue(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
        assertTrue(mCoordinator.getChipModelsForTest().get(0).get(ChipProperties.SELECTED));
        assertFalse(mCoordinator.getChipModelsForTest().get(1).get(ChipProperties.SELECTED));
    }

    @Test
    public void testEnsureGone_startOn() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, true);

        mCoordinator.ensureGone();

        assertFalse(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
    }

    @Test
    public void testEnsureGone_startOff() {
        PropertyModel model = mCoordinator.getModelForTest();
        model.set(FeedOptionsProperties.VISIBILITY_KEY, false);

        mCoordinator.ensureGone();

        assertFalse(mCoordinator.getModelForTest().get(FeedOptionsProperties.VISIBILITY_KEY));
    }

    @Test
    public void testOptionsSelected() {
        AtomicBoolean listenerCalled = new AtomicBoolean(false);
        mCoordinator.setOptionsListener(() -> { listenerCalled.set(true); });
        List<PropertyModel> chipModels = mCoordinator.getChipModelsForTest();
        chipModels.get(0).set(ChipProperties.SELECTED, false);
        chipModels.get(1).set(ChipProperties.SELECTED, true);

        mCoordinator.onOptionSelected(chipModels.get(0));

        assertFalse(chipModels.get(1).get(ChipProperties.SELECTED));
        assertTrue(chipModels.get(0).get(ChipProperties.SELECTED));
        assertTrue(listenerCalled.get());
    }

    @Features.DisableFeatures({FEED_HEADER_STICK_TO_TOP})
    @Test
    public void testStickyHeaderReturnsNullWhenFlagIsOff() {
        mCoordinator = new FeedOptionsCoordinator(mContext, mView, null);
        View stickyHeaderOptionsView = null;
        try {
            stickyHeaderOptionsView = mCoordinator.getStickyHeaderOptionsView();
        } catch (AssertionError e) {
            // Success when the assertions are enabled.
        }
        assertEquals(null, stickyHeaderOptionsView);
    }
}
