// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Test for the WebFeedFollowIntroView class. */
@RunWith(BaseRobolectricTestRunner.class)
public final class WebFeedFollowIntroViewTest {
    private WebFeedFollowIntroView mWebFeedFollowIntroView;
    private Activity mActivity;
    private View mMenuButtonAnchorView;

    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private Tracker mTracker;
    @Mock private UserEducationHelper mHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        Mockito.when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        mActivity = Robolectric.setupActivity(Activity.class);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        mMenuButtonAnchorView = new View(mActivity);
        TrackerFactory.setTrackerForTests(mTracker);

        // Build the class under test.
        Runnable noOp = CallbackUtils.emptyRunnable();
        mWebFeedFollowIntroView =
                new WebFeedFollowIntroView(
                        mActivity,
                        null,
                        mMenuButtonAnchorView,
                        mTracker,
                        /* introDismissedCallback= */ noOp);
    }

    @Test
    @SmallTest
    public void showIphTest() {
        mWebFeedFollowIntroView.showIph(
                mHelper, CallbackUtils.emptyRunnable(), CallbackUtils.emptyRunnable());
        verify(mHelper, times(1)).requestShowIph(any());
    }
}
