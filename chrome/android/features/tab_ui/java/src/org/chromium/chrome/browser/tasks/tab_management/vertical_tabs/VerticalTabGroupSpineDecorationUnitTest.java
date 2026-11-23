// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Canvas;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link VerticalTabGroupSpineDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS)
public class VerticalTabGroupSpineDecorationUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Canvas mCanvas;
    @Mock private RecyclerView mParent;
    @Mock private Runnable mInvalidationTrigger;
    @Mock private RecyclerView.State mState;

    private TabListModel mModel;
    private VerticalTabGroupSpineDecoration mSpineDecoration;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mParent.getContext()).thenReturn(activity);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        SettableMonotonicObservableSupplier<TabModel> currentTabModelSupplier =
                ObservableSuppliers.createMonotonic();
        currentTabModelSupplier.set(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(currentTabModelSupplier);

        mModel = new TabListModel();
        mSpineDecoration =
                new VerticalTabGroupSpineDecoration(
                        activity, mInvalidationTrigger, mModel, mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testOnDraw_WithValidGroup_DrawsSpinePath() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        PropertyModel headerModel = mock(PropertyModel.class);
        when(headerModel.get(TabProperties.TAB_GROUP_HEADER_ID)).thenReturn(groupId);
        when(headerModel.get(TabProperties.IS_COLLAPSED)).thenReturn(false);
        mModel.add(new ListItem(0, headerModel));

        PropertyModel tabModel = mock(PropertyModel.class);
        when(tabModel.get(TabProperties.TAB_GROUP_ID)).thenReturn(groupId);
        mModel.add(new ListItem(0, tabModel));

        when(mParent.getChildCount()).thenReturn(2);

        View headerView = mock(View.class);
        when(headerView.getHeight()).thenReturn(100);
        when(headerView.getTop()).thenReturn(0);
        when(headerView.getBottom()).thenReturn(100);
        when(headerView.getAlpha()).thenReturn(1f);

        View tabView = mock(View.class);
        when(tabView.getHeight()).thenReturn(100);
        when(tabView.getTop()).thenReturn(112);
        when(tabView.getBottom()).thenReturn(212);
        when(tabView.getAlpha()).thenReturn(1f);

        when(mParent.getChildAt(0)).thenReturn(headerView);
        when(mParent.getChildAdapterPosition(headerView)).thenReturn(0);

        when(mParent.getChildAt(1)).thenReturn(tabView);
        when(mParent.getChildAdapterPosition(tabView)).thenReturn(1);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        verify(mCanvas).drawRoundRect(any(), anyFloat(), anyFloat(), any());
    }

    @Test
    @SmallTest
    public void testOnDraw_CollapsedGroup_SkipsDrawing() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        PropertyModel headerModel = mock(PropertyModel.class);
        when(headerModel.get(TabProperties.TAB_GROUP_HEADER_ID)).thenReturn(groupId);
        when(headerModel.get(TabProperties.IS_COLLAPSED)).thenReturn(true);
        mModel.add(new ListItem(0, headerModel));

        when(mParent.getChildCount()).thenReturn(1);

        View headerView = mock(View.class);
        when(mParent.getChildAt(0)).thenReturn(headerView);
        when(mParent.getChildAdapterPosition(headerView)).thenReturn(0);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        verify(mCanvas, never()).drawRoundRect(any(), anyFloat(), anyFloat(), any());
    }

    @Test
    @SmallTest
    public void testDidChangeTabGroupColor_InvalidatesRecyclerView() {
        Token groupId = new Token(1L, 2L);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<TabGroupObserver> captor = ArgumentCaptor.forClass(TabGroupObserver.class);
        verify(mTabModel).addTabGroupObserver(captor.capture());

        captor.getValue().didChangeTabGroupColor(groupId, 0);

        verify(mInvalidationTrigger).run();
    }

    @Test
    @SmallTest
    public void testOnDraw_ScrambledChildren_SortsAndDrawsCorrectly() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        PropertyModel headerModel = mock(PropertyModel.class);
        when(headerModel.get(TabProperties.TAB_GROUP_HEADER_ID)).thenReturn(groupId);
        when(headerModel.get(TabProperties.IS_COLLAPSED)).thenReturn(false);
        mModel.add(new ListItem(0, headerModel));

        PropertyModel tabModel = mock(PropertyModel.class);
        when(tabModel.get(TabProperties.TAB_GROUP_ID)).thenReturn(groupId);
        mModel.add(new ListItem(0, tabModel));

        when(mParent.getChildCount()).thenReturn(2);

        View headerView = mock(View.class);
        when(headerView.getHeight()).thenReturn(100);
        when(headerView.getTop()).thenReturn(0);
        when(headerView.getBottom()).thenReturn(100);
        when(headerView.getAlpha()).thenReturn(1f);

        View tabView = mock(View.class);
        when(tabView.getHeight()).thenReturn(100);
        when(tabView.getTop()).thenReturn(112);
        when(tabView.getBottom()).thenReturn(212);
        when(tabView.getAlpha()).thenReturn(1f);

        // DELIBERATELY SCRAMBLE the physical ViewGroup order:
        // parent.getChildAt(0) returns the child tab (Adapter position 1)
        // parent.getChildAt(1) returns the Header (Adapter position 0)
        when(mParent.getChildAt(0)).thenReturn(tabView);
        when(mParent.getChildAdapterPosition(tabView)).thenReturn(1);

        when(mParent.getChildAt(1)).thenReturn(headerView);
        when(mParent.getChildAdapterPosition(headerView)).thenReturn(0);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        // If the sorting algorithm is working correctly, it should recognize the header is
        // logically first, and calculate the bottom correctly, invoking drawRoundRect.
        // If sorting failed, bottom calculation would break and it would skip drawing.
        verify(mCanvas).drawRoundRect(any(), anyFloat(), anyFloat(), any());
    }
}
