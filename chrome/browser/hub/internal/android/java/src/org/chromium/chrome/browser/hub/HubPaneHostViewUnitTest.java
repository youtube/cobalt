// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SNACKBAR_CONTAINER_CALLBACK;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link HubPaneHostView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubPaneHostViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock Runnable mOnActionButton;
    @Mock Callback<ViewGroup> mSnackbarContainerCallback;

    private Activity mActivity;
    private HubPaneHostView mPaneHost;
    private ImageView mHairline;
    private ViewGroup mSnackbarContainer;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mPaneHost = (HubPaneHostView) inflater.inflate(R.layout.hub_pane_host_layout, null, false);
        mHairline = mPaneHost.findViewById(R.id.pane_top_hairline);
        mSnackbarContainer = mPaneHost.findViewById(R.id.pane_host_view_snackbar_container);
        mActivity.setContentView(mPaneHost);

        mPropertyModel = new PropertyModel(HubPaneHostProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mPropertyModel, mPaneHost, HubPaneHostViewBinder::bind);
    }

    @Test
    public void testSetRootView() {
        View root1 = new View(mActivity);
        View root2 = new View(mActivity);
        View root3 = new View(mActivity);

        ViewGroup paneFrame = mPaneHost.findViewById(R.id.pane_frame);
        assertEquals(0, paneFrame.getChildCount());

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        verifyChildren(paneFrame, root1);

        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        verifyChildren(paneFrame, root1, root2);

        ShadowLooper.runUiThreadTasks();
        verifyChildren(paneFrame, root2);

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        mPropertyModel.set(PANE_ROOT_VIEW, root3);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        verifyChildren(paneFrame, root2, root3);

        ShadowLooper.runUiThreadTasks();
        verifyChildren(paneFrame, root2);

        mPropertyModel.set(PANE_ROOT_VIEW, null);
        assertEquals(0, paneFrame.getChildCount());
    }

    @Test
    public void testSetRootView_alphaRestored() {
        View root1 = new View(mActivity);
        View root2 = new View(mActivity);

        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        mPropertyModel.set(PANE_ROOT_VIEW, root2);
        ShadowLooper.runUiThreadTasks();
        assertEquals(1, root2.getAlpha(), /* delta= */ 0);

        // Inspired by b/325372945 where the alpha needed to be reset, even when no animations ran.
        mPropertyModel.set(PANE_ROOT_VIEW, null);
        mPropertyModel.set(PANE_ROOT_VIEW, root1);
        assertEquals(1, root1.getAlpha(), /* delta= */ 0);
    }

    @Test
    public void testHairlineVisibility() {
        assertEquals(View.GONE, mHairline.getVisibility());

        mPropertyModel.set(HAIRLINE_VISIBILITY, true);
        assertEquals(View.VISIBLE, mHairline.getVisibility());

        mPropertyModel.set(HAIRLINE_VISIBILITY, false);
        assertEquals(View.GONE, mHairline.getVisibility());
    }

    @Test
    public void testSnackbarContainerSupplier() {
        mPropertyModel.set(SNACKBAR_CONTAINER_CALLBACK, mSnackbarContainerCallback);
        verify(mSnackbarContainerCallback).onResult(mSnackbarContainer);
    }

    /** Order of children does not matter. */
    private void verifyChildren(ViewGroup parent, View... children) {
        assertEquals(children.length, parent.getChildCount());
        List<View> expectedChildList = Arrays.asList(children);
        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            assertTrue(child.toString(), expectedChildList.contains(child));
        }
    }
}
