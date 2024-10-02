// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefService;

import java.time.Duration;

/**
 * Unit tests for {@link FeedFeatures}.
 */
// @RunWith(BaseRobolectricTestRunner.class)
@RunWith(BlockJUnit4ClassRunner.class)
public class FeedFeaturesTest {
    @Rule
    public MockitoRule rule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Mock
    private PrefService mPrefService;

    private FeatureList.TestValues mParamsTestValues;
    private @StreamTabId int mPrefStoredTab;

    @Before
    public void setUp() {
        // Defaults the stored pref to return the Following feed.
        mPrefStoredTab = StreamTabId.FOLLOWING;
        // mPrefService = mock(PrefService.class);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        when(mPrefService.getInteger(Pref.LAST_SEEN_FEED_TYPE))
                .thenAnswer((InvocationOnMock i) -> mPrefStoredTab);
        doAnswer((InvocationOnMock invocation) -> {
            Object[] args = invocation.getArguments();
            mPrefStoredTab = (Integer) args[1];
            return null;
        })
                .when(mPrefService)
                .setInteger(eq(Pref.LAST_SEEN_FEED_TYPE), anyInt());

        mParamsTestValues = new FeatureList.TestValues();
        FeatureList.setTestValues(mParamsTestValues);
        FeedFeatures.resetInternalStateForTesting();
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
        FeedFeatures.setFakePrefsForTest(null);
    }

    @Test
    public void testAlwaysResetByDefault() {
        assertEquals(StreamTabId.FOR_YOU, FeedFeatures.getFeedTabIdToRestore());
        assertEquals(StreamTabId.FOR_YOU, mPrefStoredTab);
        // Simulates a Following tab selection.
        FeedFeatures.setLastSeenFeedTabId(StreamTabId.FOLLOWING);
        assertEquals(StreamTabId.FOR_YOU, FeedFeatures.getFeedTabIdToRestore());
        assertEquals(StreamTabId.FOR_YOU, mPrefStoredTab);
    }

    @Test
    public void testResetUponRestartFromFinchParam() {
        mParamsTestValues.addFieldTrialParamOverride(ChromeFeatureList.WEB_FEED,
                "feed_tab_stickiness_logic", "reset_upon_chrome_restart");
        FeatureList.setTestValues(mParamsTestValues);

        assertEquals(StreamTabId.FOR_YOU, FeedFeatures.getFeedTabIdToRestore());
        assertEquals(StreamTabId.FOR_YOU, mPrefStoredTab);
        // Simulates a Following tab selection.
        FeedFeatures.setLastSeenFeedTabId(StreamTabId.FOLLOWING);
        assertEquals(StreamTabId.FOLLOWING, FeedFeatures.getFeedTabIdToRestore());
        assertEquals(StreamTabId.FOLLOWING, FeedFeatures.getFeedTabIdToRestore());
    }

    @Test
    public void testIndefinitelyPersistedFromFinchParam() {
        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_FEED, "feed_tab_stickiness_logic", "indefinitely_persisted");
        FeatureList.setTestValues(mParamsTestValues);

        assertEquals(StreamTabId.FOLLOWING, FeedFeatures.getFeedTabIdToRestore());
        assertEquals(StreamTabId.FOLLOWING, mPrefStoredTab);
        // Simulates a For You tab selection.
        FeedFeatures.setLastSeenFeedTabId(StreamTabId.FOR_YOU);
        assertEquals(StreamTabId.FOR_YOU, FeedFeatures.getFeedTabIdToRestore());
        assertEquals(StreamTabId.FOR_YOU, FeedFeatures.getFeedTabIdToRestore());
    }

    @Test
    public void testShouldUseNewIndicator_noLimit() {
        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style", "new_animation_no_limit");
        FeatureList.setTestValues(mParamsTestValues);

        when(mPrefService.getBoolean(Pref.HAS_SEEN_WEB_FEED)).thenReturn(true);
        when(mPrefService.getString(Pref.LAST_BADGE_ANIMATION_TIME))
                .thenReturn("" + System.currentTimeMillis());

        assertTrue(FeedFeatures.shouldUseNewIndicator());
    }

    @Test
    public void testShouldUseNewIndicator_seenFeed() {
        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style", "new_animation");
        FeatureList.setTestValues(mParamsTestValues);

        when(mPrefService.getBoolean(Pref.HAS_SEEN_WEB_FEED)).thenReturn(true);
        when(mPrefService.getString(Pref.LAST_BADGE_ANIMATION_TIME)).thenReturn("0");

        assertFalse(FeedFeatures.shouldUseNewIndicator());
    }

    @Test
    public void testShouldUseNewIndicator_seenAnimation() {
        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style", "new_animation");
        FeatureList.setTestValues(mParamsTestValues);

        when(mPrefService.getBoolean(Pref.HAS_SEEN_WEB_FEED)).thenReturn(false);
        when(mPrefService.getString(Pref.LAST_BADGE_ANIMATION_TIME))
                .thenReturn("" + System.currentTimeMillis());

        assertFalse(FeedFeatures.shouldUseNewIndicator());
    }

    @Test
    public void testShouldUseNewIndicator_notSeenFeedAndAnimation() {
        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style", "new_animation");
        FeatureList.setTestValues(mParamsTestValues);

        when(mPrefService.getBoolean(Pref.HAS_SEEN_WEB_FEED)).thenReturn(false);
        when(mPrefService.getString(Pref.LAST_BADGE_ANIMATION_TIME))
                .thenReturn("" + (System.currentTimeMillis() - Duration.ofDays(1).toMillis()));

        assertTrue(FeedFeatures.shouldUseNewIndicator());
    }

    @Test
    public void testShouldUseNewIndicator_notSeenAnimationInFuture() {
        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style", "new_animation");
        FeatureList.setTestValues(mParamsTestValues);

        when(mPrefService.getBoolean(Pref.HAS_SEEN_WEB_FEED)).thenReturn(false);
        when(mPrefService.getString(Pref.LAST_BADGE_ANIMATION_TIME))
                .thenReturn("" + (System.currentTimeMillis() + Duration.ofDays(1).toMillis()));

        assertTrue(FeedFeatures.shouldUseNewIndicator());
    }
}
