// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sensitive_content;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.ViewGroup;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SensitiveContentClientUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    private ViewAndroidDelegate mViewAndroidDelegate;
    private ViewAndroidDelegate mSecondViewAndroidDelegate;
    @Mock private ViewGroup mFirstContainerView;
    @Mock private ViewGroup mSecondContainerView;
    @Mock private ViewGroup mThirdContainerView;
    @Mock private SensitiveContentClient.ContentSensitivitySetter mContentSensitivitySetter;
    @Mock private SensitiveContentClient.Observer mObserver;

    private SensitiveContentClient mClient;

    @Before
    public void setUp() {
        mViewAndroidDelegate = ViewAndroidDelegate.createBasicDelegate(mFirstContainerView);
        mSecondViewAndroidDelegate = ViewAndroidDelegate.createBasicDelegate(mThirdContainerView);
        when(mWebContents.getViewAndroidDelegate()).thenAnswer(invocation -> mViewAndroidDelegate);
        mClient = new SensitiveContentClient(mWebContents, mContentSensitivitySetter);
        mClient.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        mClient.removeObserver(mObserver);
    }

    @Test
    public void updateContentSensitivity() {
        mClient.setContentSensitivity(/* contentIsSensitive= */ true);
        verify(mContentSensitivitySetter).setContentSensitivity(mFirstContainerView, true);
        mClient.setContentSensitivity(/* contentIsSensitive= */ false);
        verify(mContentSensitivitySetter).setContentSensitivity(mFirstContainerView, false);
    }

    @Test
    public void updateContainerView() {
        mViewAndroidDelegate.setContainerView(mSecondContainerView);
        verify(mContentSensitivitySetter).setContentSensitivity(mSecondContainerView, false);
    }

    @Test
    public void updateViewAndroidDelegate() {
        when(mWebContents.getViewAndroidDelegate())
                .thenAnswer(invocation -> mSecondViewAndroidDelegate);

        mClient.onViewAndroidDelegateSet();
        verify(mContentSensitivitySetter).setContentSensitivity(mThirdContainerView, false);

        mSecondViewAndroidDelegate.setContainerView(mSecondContainerView);
        verify(mContentSensitivitySetter).setContentSensitivity(mSecondContainerView, false);

        verify(mContentSensitivitySetter, never()).setContentSensitivity(any(), eq(true));
        verify(mContentSensitivitySetter, never())
                .setContentSensitivity(eq(mFirstContainerView), anyBoolean());
    }

    @Test
    public void observersNotifiedOnSensitivityChange() {
        mClient.setContentSensitivity(/* contentIsSensitive= */ true);
        verify(mObserver).onContentSensitivityChanged(true);
        mClient.setContentSensitivity(/* contentIsSensitive= */ false);
        verify(mObserver).onContentSensitivityChanged(false);
    }
}
