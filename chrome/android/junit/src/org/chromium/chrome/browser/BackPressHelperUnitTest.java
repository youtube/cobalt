// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.activity.OnBackPressedDispatcher;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.LifecycleRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link org.chromium.chrome.browser.BackPressHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackPressHelperUnitTest {
    @Mock
    private BackPressHelper.ObsoleteBackPressedHandler mBackPressedHandler;
    @Mock
    private BackPressHelper.ObsoleteBackPressedHandler mBackPressedHandler2;
    @Mock
    private LifecycleOwner mLifecycleOwner;
    private LifecycleRegistry mLifecycle;
    @Mock
    private Runnable mFallbackRunnable;

    private OnBackPressedDispatcher mOnBackPressedDispatcher;
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLifecycle = new LifecycleRegistry(mLifecycleOwner);
        mOnBackPressedDispatcher = new OnBackPressedDispatcher(mFallbackRunnable);
        doReturn(mLifecycle).when(mLifecycleOwner).getLifecycle();
        mLifecycle.setCurrentState(Lifecycle.State.CREATED);
    }

    @Test
    public void testNextCallbackNotInvokedIfAlreadyConsumed() {
        doReturn(true).when(mBackPressedHandler).onBackPressed();

        // The last-added one is invoked first: mBackPressedHandler -> mBackPressedHandler2
        BackPressHelper.create(mLifecycleOwner, mOnBackPressedDispatcher, mBackPressedHandler2);
        BackPressHelper.create(mLifecycleOwner, mOnBackPressedDispatcher, mBackPressedHandler);
        mLifecycle.setCurrentState(Lifecycle.State.STARTED);
        mOnBackPressedDispatcher.onBackPressed();

        verify(mBackPressedHandler).onBackPressed();
        verify(mBackPressedHandler2, never()).onBackPressed();
        verify(mFallbackRunnable, never()).run();
    }

    @Test
    public void testInvokeNextCallbackIfNotConsumed() {
        doReturn(false).when(mBackPressedHandler).onBackPressed();
        doReturn(true).when(mBackPressedHandler2).onBackPressed();
        BackPressHelper.create(mLifecycleOwner, mOnBackPressedDispatcher, mBackPressedHandler2);
        BackPressHelper.create(mLifecycleOwner, mOnBackPressedDispatcher, mBackPressedHandler);
        mLifecycle.setCurrentState(Lifecycle.State.STARTED);
        mOnBackPressedDispatcher.onBackPressed();

        verify(mBackPressedHandler).onBackPressed();
        verify(mBackPressedHandler2).onBackPressed();
        verify(mFallbackRunnable, never()).run();
    }

    @Test
    public void testInvokeFallbackRunnableIfNotHandled() {
        doReturn(false).when(mBackPressedHandler).onBackPressed();
        BackPressHelper.create(mLifecycleOwner, mOnBackPressedDispatcher, mBackPressedHandler);
        mLifecycle.setCurrentState(Lifecycle.State.STARTED);
        mOnBackPressedDispatcher.onBackPressed();

        verify(mFallbackRunnable).run();
    }
}
