// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.Pair;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.BottomContainer;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.TestActivity;

/** Unit tests for ChromeActivity. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeActivityUnitTest {
    Activity mActivity;

    @Mock RootUiCoordinator mRootUiCoordinatorMock;
    @Mock TabModel mTabModel;
    @Mock Profile mProfile;
    @Mock Tab mActivityTab;
    @Mock ReadAloudController mReadAloudController;

    ObservableSupplierImpl<ReadAloudController> mReadAloudControllerSupplier =
            new ObservableSupplierImpl<>();

    class TestChromeActivity extends ChromeActivity {
        public TestChromeActivity() {
            mRootUiCoordinator = mRootUiCoordinatorMock;
        }

        @Override
        protected TabModelOrchestrator createTabModelOrchestrator() {
            return null;
        }

        @Override
        protected void createTabModels() {}

        @Override
        protected void destroyTabModels() {}

        @Override
        protected Pair<? extends TabCreator, ? extends TabCreator> createTabCreators() {
            return null;
        }

        @Override
        protected LaunchCauseMetrics createLaunchCauseMetrics() {
            return null;
        }

        @Override
        public @ActivityType int getActivityType() {
            return ActivityType.TABBED;
        }

        @Override
        protected boolean handleBackPressed() {
            return true;
        }
    }

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
    }

    @Test
    public void testCreateWindowErrorSnackbar() {
        String errorString = "Some error.";
        ViewGroup viewGroup = new BottomContainer(mActivity, null);
        SnackbarManager snackbarManager =
                Mockito.spy(new SnackbarManager(mActivity, viewGroup, null));
        ChromeActivity.createWindowErrorSnackbar(errorString, snackbarManager);
        Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
        Mockito.verify(snackbarManager).showSnackbar(ArgumentMatchers.any());
        Assert.assertNull("Snackbar controller should be null.", snackbar.getController());
        Assert.assertEquals(
                "Snackbar text should match.", errorString, snackbar.getTextForTesting());
        Assert.assertEquals(
                "Snackbar identifier should match.",
                Snackbar.UMA_WINDOW_ERROR,
                snackbar.getIdentifierForTesting());
        Assert.assertEquals(
                "Snackbar dismiss duration is incorrect.",
                SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS,
                snackbar.getDuration());
        snackbarManager.dismissSnackbars(null);
    }

    @Test
    public void testReadAloudAppMenuItemClicked() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());

        doReturn(mActivityTab).when(chromeActivity).getActivityTab();
        doReturn(mTabModel).when(chromeActivity).getCurrentTabModel();
        when(mTabModel.getProfile()).thenReturn(mProfile);
        mReadAloudControllerSupplier.set(mReadAloudController);
        when(mRootUiCoordinatorMock.getReadAloudControllerSupplier())
                .thenReturn(mReadAloudControllerSupplier);

        assertTrue(
                chromeActivity.onMenuOrKeyboardAction(
                        R.id.readaloud_menu_id, /* fromMenu= */ true));
        verify(mReadAloudController, times(1)).playTab(eq(mActivityTab));
    }
}
