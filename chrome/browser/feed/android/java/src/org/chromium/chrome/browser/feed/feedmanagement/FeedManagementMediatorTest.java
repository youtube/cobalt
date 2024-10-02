// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * Tests {@link FeedManagementMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FeedManagementMediatorTest {
    private static final @StreamKind int TEST_STREAM_KIND = StreamKind.FOR_YOU;
    private Activity mActivity;
    private ShadowActivity mShadowActivity;
    private ModelList mModelList;
    private FeedManagementMediator mFeedManagementMediator;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;

    @Mock
    private FeedManagementMediator.FollowManagementLauncher mFollowManagementLauncher;

    @Mock
    private FeedManagementMediator.AutoplayManagementLauncher mAutoplayManagementLauncher;

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mShadowActivity = Shadows.shadowOf(mActivity);
        mModelList = new ModelList();
        MockitoAnnotations.initMocks(this);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);

        mFeedManagementMediator = new FeedManagementMediator(mActivity, mModelList,
                mFollowManagementLauncher, mAutoplayManagementLauncher, TEST_STREAM_KIND);

        verify(mFeedServiceBridgeJniMock).isAutoplayEnabled();
    }

    @Test
    public void testHandleActivityClick_FlagDisabled() {
        ChromeFeatureList.sCctResizableSideSheetDiscoverFeedSettings.setForTesting(false);
        // Act
        mFeedManagementMediator.handleActivityClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(
                intent.getData(), Uri.parse("https://myactivity.google.com/myactivity?product=50"));
        assertEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_WIDTH_PX, 0));
        assertEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_HEIGHT_PX, 0));
        verify(mFeedServiceBridgeJniMock)
                .reportOtherUserAction(TEST_STREAM_KIND, FeedUserActionType.TAPPED_MANAGE_ACTIVITY);
    }

    @Test
    public void testHandleActivityClick_FlagEnabled() {
        ChromeFeatureList.sCctResizableSideSheetDiscoverFeedSettings.setForTesting(true);
        // Act
        mFeedManagementMediator.handleActivityClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(
                intent.getData(), Uri.parse("https://myactivity.google.com/myactivity?product=50"));
        assertNotEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_WIDTH_PX, 0));
        assertNotEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_HEIGHT_PX, 0));
        verify(mFeedServiceBridgeJniMock)
                .reportOtherUserAction(TEST_STREAM_KIND, FeedUserActionType.TAPPED_MANAGE_ACTIVITY);
    }

    @Test
    public void testHandleInterestsClick_FlagDisabled() {
        ChromeFeatureList.sCctResizableSideSheetDiscoverFeedSettings.setForTesting(false);
        // Act
        mFeedManagementMediator.handleInterestsClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(intent.getData(),
                Uri.parse("https://www.google.com/preferences/interests/yourinterests?sh=n"));
        assertEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_WIDTH_PX, 0));
        assertEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_HEIGHT_PX, 0));
        verify(mFeedServiceBridgeJniMock)
                .reportOtherUserAction(
                        TEST_STREAM_KIND, FeedUserActionType.TAPPED_MANAGE_INTERESTS);
    }

    @Test
    public void testHandleInterestsClick_FlagEnabled() {
        ChromeFeatureList.sCctResizableSideSheetDiscoverFeedSettings.setForTesting(true);
        // Act
        mFeedManagementMediator.handleInterestsClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(intent.getData(),
                Uri.parse("https://www.google.com/preferences/interests/yourinterests?sh=n"));
        assertNotEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_WIDTH_PX, 0));
        assertNotEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_HEIGHT_PX, 0));
        verify(mFeedServiceBridgeJniMock)
                .reportOtherUserAction(
                        TEST_STREAM_KIND, FeedUserActionType.TAPPED_MANAGE_INTERESTS);
    }

    @Test
    public void testHandleHiddenClick_FlagDisabled() {
        ChromeFeatureList.sCctResizableSideSheetDiscoverFeedSettings.setForTesting(false);
        // Act
        mFeedManagementMediator.handleHiddenClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(intent.getData(),
                Uri.parse("https://www.google.com/preferences/interests/hidden?sh=n"));
        assertEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_WIDTH_PX, 0));
        assertEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_HEIGHT_PX, 0));
        verify(mFeedServiceBridgeJniMock)
                .reportOtherUserAction(
                        TEST_STREAM_KIND, FeedUserActionType.TAPPED_MANAGE_INTERESTS);
    }

    @Test
    public void testHandleHiddenClick_FlagEnabled() {
        ChromeFeatureList.sCctResizableSideSheetDiscoverFeedSettings.setForTesting(true);
        // Act
        mFeedManagementMediator.handleHiddenClick(null);

        // Assert
        Intent intent = mShadowActivity.peekNextStartedActivityForResult().intent;
        assertEquals(intent.getData(),
                Uri.parse("https://www.google.com/preferences/interests/hidden?sh=n"));
        assertNotEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_WIDTH_PX, 0));
        assertNotEquals(
                0, intent.getIntExtra(FeedManagementMediator.EXTRA_ACTIVITY_INITIAL_HEIGHT_PX, 0));
        verify(mFeedServiceBridgeJniMock)
                .reportOtherUserAction(
                        TEST_STREAM_KIND, FeedUserActionType.TAPPED_MANAGE_INTERESTS);
    }

    @Test
    public void testHandleFollowingClick() {
        // Act
        mFeedManagementMediator.handleFollowingClick(null);

        // Assert
        verify(mFollowManagementLauncher).launchFollowManagement(mActivity);
        verify(mFeedServiceBridgeJniMock)
                .reportOtherUserAction(
                        TEST_STREAM_KIND, FeedUserActionType.TAPPED_MANAGE_FOLLOWING);
    }

    @Test
    public void testHandleAutoplayClick() {
        // Act
        mFeedManagementMediator.handleAutoplayClick(null);

        // Assert
        verify(mAutoplayManagementLauncher).launchAutoplayManagement(mActivity);
        verify(mFeedServiceBridgeJniMock)
                .reportOtherUserAction(
                        TEST_STREAM_KIND, FeedUserActionType.OPENED_AUTOPLAY_SETTINGS);
    }
}
