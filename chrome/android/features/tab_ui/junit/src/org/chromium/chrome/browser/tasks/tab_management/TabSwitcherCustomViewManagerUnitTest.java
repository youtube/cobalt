// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link TabSwitcherCustomViewManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSwitcherCustomViewManagerUnitTest {
    @Mock
    private TabSwitcherCustomViewManager.Delegate mDelegate;
    @Mock
    private View mView;
    @Mock
    private Runnable mBackPressRunnableMock;

    private TabSwitcherCustomViewManager mTabSwitcherCustomViewManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabSwitcherCustomViewManager = new TabSwitcherCustomViewManager(mDelegate);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mDelegate, mView);
    }

    @Test
    @SmallTest
    public void testRequestView_InvokesDelegateAddingView() {
        doNothing().when(mDelegate).addCustomView(mView, mBackPressRunnableMock);
        mTabSwitcherCustomViewManager.requestView(mView, mBackPressRunnableMock);
        verify(mDelegate).addCustomView(mView, mBackPressRunnableMock);
    }

    @Test
    @SmallTest
    public void testReleaseView_InvokesDelegateRemoveView() {
        // Add the view.
        doNothing().when(mDelegate).addCustomView(mView, mBackPressRunnableMock);
        mTabSwitcherCustomViewManager.requestView(mView, mBackPressRunnableMock);
        verify(mDelegate).addCustomView(mView, mBackPressRunnableMock);

        // Release the view.
        doNothing().when(mDelegate).removeCustomView(mView);
        mTabSwitcherCustomViewManager.releaseView();
        verify(mDelegate).removeCustomView(mView);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testMultipleRequestView_withoutRelease_throwsError() {
        doNothing().when(mDelegate).addCustomView(mView, mBackPressRunnableMock);
        mTabSwitcherCustomViewManager.requestView(mView, mBackPressRunnableMock);
        verify(mDelegate).addCustomView(mView, mBackPressRunnableMock);

        // This should throw an error because we have not release the view yet.
        mTabSwitcherCustomViewManager.requestView(mView, mBackPressRunnableMock);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReleaseView_BeforeRequesting_throwsError() {
        // This should throw an error because we have not requested the view yet.
        mTabSwitcherCustomViewManager.releaseView();
    }
}
