// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.SendTabToSelfTabCardLabelData;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.Collections;
import java.util.Map;

/** Unit tests for {@link SendTabToSelfTabLabeller}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SendTabToSelfTabLabellerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TAB_ID = 1;

    @Mock private TabListNotificationHandler mTabListNotificationHandler;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<Map<Integer, TabCardLabelData>> mLabelDataCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private final SettableNullableObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createNullable();

    private Context mContext;
    private UserDataHost mUserDataHost;
    private SendTabToSelfTabLabeller mLabeller;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mUserDataHost = new UserDataHost();

        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);

        mTabModelSupplier.set(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);

        mLabeller = new SendTabToSelfTabLabeller(mTabListNotificationHandler, mTabModelSupplier);
    }

    @Test
    public void testShowAll_ValidLabelPushed() {
        SendTabToSelfTabCardLabelData sttsData =
                new SendTabToSelfTabCardLabelData(
                        mTab, "Example Phone", System.currentTimeMillis());
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, sttsData);

        mLabeller.showAll(Collections.singletonList(mTab));

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        Map<Integer, TabCardLabelData> labelDataMap = mLabelDataCaptor.getValue();

        assertTrue(labelDataMap.containsKey(TAB_ID));
        TabCardLabelData labelData = labelDataMap.get(TAB_ID);
        assertNotNull(labelData);
        assertEquals("From Example Phone", labelData.textResolver.resolve(mContext));
    }

    @Test
    public void testShowAll_Expired() {
        SendTabToSelfTabCardLabelData data =
                new SendTabToSelfTabCardLabelData(
                        mTab, "Example Phone", System.currentTimeMillis());
        data.setAdditionTimestampMsForTesting(
                System.currentTimeMillis() - 6L * 24 * 60 * 60 * 1000); // 6 days old
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, data);

        mLabeller.showAll(Collections.singletonList(mTab));

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        Map<Integer, TabCardLabelData> labelDataMap = mLabelDataCaptor.getValue();

        assertTrue(labelDataMap.containsKey(TAB_ID));
        assertNull(labelDataMap.get(TAB_ID));
        assertNull(mUserDataHost.getUserData(SendTabToSelfTabCardLabelData.class));
    }

    @Test
    public void testShowAll_StaleLabelClearedOnUpdate() {
        SendTabToSelfTabCardLabelData sttsData =
                new SendTabToSelfTabCardLabelData(
                        mTab, "Example Phone", System.currentTimeMillis());
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, sttsData);
        verify(mTab).addObserver(mTabObserverCaptor.capture());

        // Initial showAll pushes mTab's label.
        mLabeller.showAll(Collections.singletonList(mTab));

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertNotNull(mLabelDataCaptor.getValue().get(TAB_ID));

        // Simulate user interaction on mTab, which deletes sttsData from mUserDataHost.
        mTabObserverCaptor.getValue().onShown(mTab, TabSelectionType.FROM_USER);

        // Re-run showAll. mTab now has no data, so it pushes null to clear the label.
        mLabeller.showAll(Collections.singletonList(mTab));

        verify(mTabListNotificationHandler, times(2))
                .updateTabCardLabels(mLabelDataCaptor.capture());
        assertNull(mLabelDataCaptor.getValue().get(TAB_ID));
    }

    @Test
    public void testShowAll_LabelPreservedIfDifferentTabInteracted() {
        SendTabToSelfTabCardLabelData sttsData =
                new SendTabToSelfTabCardLabelData(
                        mTab, "Example Phone", System.currentTimeMillis());
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, sttsData);
        verify(mTab).addObserver(mTabObserverCaptor.capture());

        // Initial showAll pushes mTab's label.
        mLabeller.showAll(Collections.singletonList(mTab));

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertNotNull(mLabelDataCaptor.getValue().get(TAB_ID));

        // Simulate user interaction on a different tab. mTab's sttsData remains active.
        Tab otherTab = mock(Tab.class);
        when(otherTab.getUserDataHost()).thenReturn(new UserDataHost());
        mTabObserverCaptor.getValue().onShown(otherTab, TabSelectionType.FROM_USER);

        // Re-run showAll. mTab's label is perfectly preserved.
        mLabeller.showAll(Collections.singletonList(mTab));

        verify(mTabListNotificationHandler, times(2))
                .updateTabCardLabels(mLabelDataCaptor.capture());
        assertNotNull(mLabelDataCaptor.getValue().get(TAB_ID));
    }

    @Test
    public void testShowAll_NullList() {
        SendTabToSelfTabCardLabelData sttsData =
                new SendTabToSelfTabCardLabelData(
                        mTab, "Example Phone", System.currentTimeMillis());
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, sttsData);

        // Passing null triggers the fallback to mCurrentTabModel/mTabModelSupplier.
        mLabeller.showAll(null);

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        Map<Integer, TabCardLabelData> labelDataMap = mLabelDataCaptor.getValue();
        assertNotNull(labelDataMap.get(TAB_ID));
    }

    @Test
    public void testDidAddTab() {
        SendTabToSelfTabCardLabelData sttsData =
                new SendTabToSelfTabCardLabelData(
                        mTab, "Example Phone", System.currentTimeMillis());
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, sttsData);

        mLabeller.didAddTab(mTab, 0, 0, false);

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertNotNull(mLabelDataCaptor.getValue().get(TAB_ID));
    }

    @Test
    public void testDestroy() {
        mLabeller.destroy();
        verify(mTabModel).removeObserver(mLabeller);
    }

    @Test
    public void testOnTabModelChange() {
        TabModel newTabModel = mock(TabModel.class);
        mTabModelSupplier.set(newTabModel);

        verify(mTabModel).removeObserver(mLabeller);
        verify(newTabModel).addObserver(mLabeller);
    }
}
