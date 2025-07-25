// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerCoordinatorUnitTest {
    private static final String TITLE = "Title";
    private static final String PUBLISHER = "Publisher";

    @Mock private Activity mActivity;
    @Mock private Context mContextForInflation;
    @Mock private LayoutInflater mLayoutInflater;
    @Mock private ViewStub mViewStub;
    @Mock private MiniPlayerLayout mLayout;
    @Mock private MiniPlayerMediator mMediator;
    private PropertyModel mModel;

    private MiniPlayerCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mLayout).when(mViewStub).inflate();
        doReturn(mViewStub).when(mActivity).findViewById(eq(R.id.readaloud_mini_player_stub));
        doReturn(mLayoutInflater)
                .when(mContextForInflation)
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mCoordinator = new MiniPlayerCoordinator(mModel, mMediator, mLayout);
    }

    @Test
    public void testViewInflated() {
        // Test the real constructor
        reset(mViewStub);
        doReturn(mLayout).when(mViewStub).inflate();
        mCoordinator = new MiniPlayerCoordinator(mActivity, mContextForInflation, mModel);
        verify(mViewStub).inflate();
    }

    @Test
    public void testShow() {
        mCoordinator.show(/* animate= */ false);
        verify(mMediator).show(eq(false));

        // Second show() shouldn't inflate the stub again.
        reset(mViewStub);
        mCoordinator.show(/* animate= */ false);
        verify(mMediator, times(2)).show(eq(false));
    }

    @Test
    public void testDismissWhenNeverShown() {
        // Ensure there's no crash.
        assertEquals(VisibilityState.GONE, mCoordinator.getVisibility());
        mCoordinator.dismiss(false);
    }

    @Test
    public void testDismiss() {
        mCoordinator.dismiss(/* animate= */ false);
        verify(mMediator).dismiss(eq(false));
    }

    @Test
    public void testBindPlaybackState() {
        mCoordinator.show(/* animate= */ true);
        mModel.set(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.PLAYING);
        verify(mLayout).onPlaybackStateChanged(eq(PlaybackListener.State.PLAYING));
    }

    @Test
    public void testBindTitle() {
        mCoordinator.show(/* animate= */ true);
        mModel.set(PlayerProperties.TITLE, TITLE);
        verify(mLayout).setTitle(eq(TITLE));
    }

    @Test
    public void testBindPublisher() {
        mCoordinator.show(/* animate= */ true);
        mModel.set(PlayerProperties.PUBLISHER, PUBLISHER);
        verify(mLayout).setPublisher(eq(PUBLISHER));
    }

    @Test
    public void testBindProgress() {
        mCoordinator.show(/* animate= */ true);
        mModel.set(PlayerProperties.PROGRESS, 0.5f);
        verify(mLayout).setProgress(eq(0.5f));
    }

    @Test
    public void testBindVisibility() {
        mModel.set(PlayerProperties.MINI_PLAYER_VISIBILITY, VisibilityState.SHOWING);
        verify(mLayout).updateVisibility(eq(VisibilityState.SHOWING));

        mModel.set(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES, true);
        verify(mLayout).enableAnimations(eq(true));
    }
}
