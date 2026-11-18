// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Point;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link VerticalTabListCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures({
    ChromeFeatureList.GLIC,
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.DATA_SHARING_JOIN_ONLY
})
public class VerticalTabListCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabCreator mTabCreator;
    @Mock private Profile mProfile;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mSelectorObserverCaptor;
    @Mock private VerticalTabsActionDelegate mVerticalTabsActionDelegate;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabStripContextMenuCoordinator mTabStripContextMenuCoordinator;

    private Activity mActivity;
    private final SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier =
            ObservableSuppliers.createMonotonic();
    private final List<TabGroupObserver> mTabGroupObservers = new ArrayList<>();
    private VerticalTabListCoordinator mCoordinator;

    @Before
    public void setUp() {
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        ServiceStatus serviceStatus = mock(ServiceStatus.class);
        when(mCollaborationService.getServiceStatus()).thenReturn(serviceStatus);
        when(serviceStatus.isAllowedToJoin()).thenReturn(false);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mCurrentTabModelSupplier.set(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mCurrentTabModelSupplier);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModel.isTabModelRestored()).thenReturn(true);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabModel.iterator()).thenReturn(java.util.Collections.emptyIterator());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        GlicEnabling.setEnabledForTesting(false);

        doAnswer(
                        invocation -> {
                            mTabGroupObservers.add(invocation.getArgument(0));
                            return null;
                        })
                .when(mTabModel)
                .addTabGroupObserver(any(TabGroupObserver.class));
    }

    private void createCoordinator() {
        mCoordinator =
                new VerticalTabListCoordinator(
                        mActivity,
                        mTabModelSelector,
                        mProfile,
                        mVerticalTabsActionDelegate,
                        mWindowAndroid,
                        mMultiInstanceManager,
                        mSnackbarManager);
    }

    private Tab prepareMockTab(int id) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.isInitialized()).thenReturn(true);
        when(tab.getTitle()).thenReturn("Tab " + id);
        GURL gurl = new GURL("https://google.com");
        when(tab.getUrl()).thenReturn(gurl);
        return tab;
    }

    private MotionEvent obtainMotionEvent(int action, float x, float y) {
        // We get the current time since Android rejects times that are 0 or in the past.
        // When the finger/mouse first touches the screen.
        long downTime = SystemClock.uptimeMillis();
        // When the specific event happens (finger down, lift finger, etc.).
        long eventTime = SystemClock.uptimeMillis();

        // Manually create a fake event.
        return MotionEvent.obtain(
                /* downTime= */ downTime,
                /* eventTime= */ eventTime,
                /* action= */ action,
                /* x= */ x,
                /* y= */ y,
                /* metaState= */ 0);
    }

    @Test
    @SmallTest
    public void testConstructor() {
        doNothing().when(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        createCoordinator();
        assertNotNull(mCoordinator.getView());

        ViewGroup view = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        assertNotNull(recyclerView);
        assertNotNull(recyclerView.getAdapter());
        assertNotNull(recyclerView.getLayoutManager());

        GridLayoutManager layoutManager = (GridLayoutManager) recyclerView.getLayoutManager();
        assertEquals(4, layoutManager.getSpanCount());

        assertNotNull(mSelectorObserverCaptor.getValue());
        verify(mTabModelSelector).addObserver(mSelectorObserverCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        doNothing().when(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        createCoordinator();
        mCoordinator.setTabStripContextMenuCoordinatorForTesting(mTabStripContextMenuCoordinator);

        TabModelSelectorObserver observer = mSelectorObserverCaptor.getValue();
        assertNotNull(observer);

        mCoordinator.destroy();

        verify(mTabModelSelector).removeObserver(observer);
        verify(mTabStripContextMenuCoordinator).destroy();
        assertNull(
                "The tab strip context menu reference must be nullified upon destruction.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    @Test
    @SmallTest
    public void testItemTouchListener_OnInterceptTouchEventOverride() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);

        assertNotNull("RecyclerView should be initialized inside the layout.", recyclerView);

        // Simulate an action down at coordinates (250, 400).
        MotionEvent downEvent = obtainMotionEvent(MotionEvent.ACTION_DOWN, 250f, 400f);

        // Dispatch the touch event directly into the real RecyclerView's touch pipeline.
        boolean intercepted = recyclerView.onInterceptTouchEvent(downEvent);
        assertFalse("Touch interceptor must remain passive.", intercepted);

        Point savedPoint = mCoordinator.getLastTouchPointForTesting();
        assertEquals("X coordinate should be saved.", 250, savedPoint.x);
        assertEquals("Y coordinate should be saved.", 400, savedPoint.y);

        // The tab strip coordinator should not have been instantiated because we have not advanced
        // the shadow looper to reach the long-press (500ms) milestone.
        assertNull(mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    @Test
    @SmallTest
    public void testVTEmptySpaceLongPress_LaunchesContextMenu() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);

        assertNotNull(recyclerView);

        // Ensure the context menu coordinator reference starts fresh as null.
        assertNull(mCoordinator.getTabStripContextMenuCoordinatorForTesting());

        // Simulate an action down at coordinates (250, 400).
        MotionEvent downEvent = obtainMotionEvent(MotionEvent.ACTION_DOWN, 250f, 400f);
        recyclerView.onInterceptTouchEvent(downEvent);

        // Advance Robolectric's clock by 500ms to trigger the long-press timeout.
        // This triggers the gestureDetector's long-press callback that we overrode.
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);
        assertNotNull(
                "Long press on empty space should instantiate the context menu coordinator.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    @Test
    @SmallTest
    public void testVTEmptySpaceRightClick_LaunchesContextMenu() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);

        assertNotNull(recyclerView);
        assertNull(mCoordinator.getTabStripContextMenuCoordinatorForTesting());

        // Simulate a mouse right-click.
        recyclerView.performContextClick();

        assertNotNull(
                "Right click on empty space should instantiate the context menu coordinator.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    @Test
    @SmallTest
    public void testDestroy_RemovesSupplierObserver() {
        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        mCoordinator.destroy();

        TabModel newTabModel = mock(TabModel.class);
        when(newTabModel.getProfile()).thenReturn(mProfile);
        when(newTabModel.isTabModelRestored()).thenReturn(true);
        Tab newTab = prepareMockTab(789);
        when(newTabModel.getRepresentativeTabList()).thenReturn(List.of(newTab));

        mCurrentTabModelSupplier.set(newTabModel);
        assertEquals(0, adapter.getModelList().size());
    }

    @Test
    @SmallTest
    public void testAdapterInterceptionAndSpanLookup() {
        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();
        assertNotNull(recycler.getLayoutManager());
        GridLayoutManager.SpanSizeLookup lookup =
                ((GridLayoutManager) recycler.getLayoutManager()).getSpanSizeLookup();

        PropertyModel reg = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        PropertyModel pin = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        PropertyModel group = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        pin.set(TabProperties.IS_PINNED, true);
        group.set(TabProperties.TAB_GROUP_HEADER_ID, new Token(1L, 2L));

        assertNotNull(adapter);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, reg));
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, pin));
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, group));

        assertEquals(UiType.TAB, adapter.getItemViewType(0));
        assertEquals(UiType.PINNED_TAB, adapter.getItemViewType(1));
        assertEquals(UiType.TAB_GROUP, adapter.getItemViewType(2));
        assertEquals(4, lookup.getSpanSize(0));
        assertEquals(1, lookup.getSpanSize(1));
        assertEquals(4, lookup.getSpanSize(2));
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_Expand() {
        Tab tab123 = prepareMockTab(123);
        Token tabGroupId123 = new Token(1L, 2L);
        when(tab123.getTabGroupId()).thenReturn(tabGroupId123);

        when(mTabModel.getTabById(anyInt())).thenReturn(tab123);
        when(mTabModel.getTabsInGroup(tabGroupId123)).thenReturn(List.of(tab123));
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab123));
        when(mTabModel.getGroupLastShownTabId(tabGroupId123)).thenReturn(123);
        when(mTabModel.getRelatedTabList(123)).thenReturn(List.of(tab123));
        when(mTabModel.isTabInTabGroup(tab123)).thenReturn(true);

        final boolean[] collapsedState = {true};
        doAnswer(invocation -> collapsedState[0])
                .when(mTabModel)
                .getTabGroupCollapsed(any(Token.class));
        doAnswer(
                        invocation -> {
                            collapsedState[0] = invocation.getArgument(1);
                            for (TabGroupObserver observer : mTabGroupObservers) {
                                observer.didChangeTabGroupCollapsed(
                                        invocation.getArgument(0), collapsedState[0], false);
                            }
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupCollapsed(any(Token.class), anyBoolean(), anyBoolean());

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel groupModel = adapter.getModelList().get(0).model;
        assertTrue(groupModel.get(TabProperties.IS_COLLAPSED));
        assertNull(groupModel.get(TabProperties.TAB_ACTION_BUTTON_DATA));

        mCoordinator.toggleTabGroupExpansion(123);
        assertFalse(
                "Tab group should be expanded (IS_COLLAPSED = false) after first toggle click.",
                groupModel.get(TabProperties.IS_COLLAPSED));
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_Collapse() {
        Tab tab123 = prepareMockTab(123);
        Token tabGroupId123 = new Token(1L, 2L);
        when(tab123.getTabGroupId()).thenReturn(tabGroupId123);

        when(mTabModel.getTabById(anyInt())).thenReturn(tab123);
        when(mTabModel.getTabsInGroup(tabGroupId123)).thenReturn(List.of(tab123));
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab123));
        when(mTabModel.getGroupLastShownTabId(tabGroupId123)).thenReturn(123);
        when(mTabModel.getRelatedTabList(123)).thenReturn(List.of(tab123));
        when(mTabModel.isTabInTabGroup(tab123)).thenReturn(true);

        final boolean[] collapsedState = {false};
        doAnswer(invocation -> collapsedState[0])
                .when(mTabModel)
                .getTabGroupCollapsed(any(Token.class));
        doAnswer(
                        invocation -> {
                            collapsedState[0] = invocation.getArgument(1);
                            for (TabGroupObserver observer : mTabGroupObservers) {
                                observer.didChangeTabGroupCollapsed(
                                        invocation.getArgument(0), collapsedState[0], false);
                            }
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupCollapsed(any(Token.class), anyBoolean(), anyBoolean());

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(2, adapter.getModelList().size());
        PropertyModel groupModel = adapter.getModelList().get(0).model;
        assertFalse(groupModel.get(TabProperties.IS_COLLAPSED));

        mCoordinator.toggleTabGroupExpansion(123);
        assertTrue(
                "Tab group should be collapsed (IS_COLLAPSED = true) after toggle click from"
                        + " expanded state.",
                groupModel.get(TabProperties.IS_COLLAPSED));
        assertEquals(1, adapter.getModelList().size());
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_RegularTabCannotToggle() {
        Tab tab456 = prepareMockTab(456);
        when(mTabModel.getTabById(anyInt())).thenReturn(tab456);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab456));
        when(mTabModel.isTabInTabGroup(tab456)).thenReturn(false);

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel tabModel = adapter.getModelList().get(0).model;

        mCoordinator.toggleTabGroupExpansion(456);
        assertFalse(tabModel.get(TabProperties.IS_COLLAPSED));
    }

    @Test
    @SmallTest
    public void testTabSelection_SelectsTabInSelector() {
        Tab tab456 = prepareMockTab(456);
        when(mTabModelSelector.getModelForTabId(456)).thenReturn(mTabModel);
        when(mTabModel.getTabById(anyInt())).thenReturn(tab456);
        when(mTabModel.indexOf(tab456)).thenReturn(0);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab456));
        when(mTabModel.isTabInTabGroup(tab456)).thenReturn(false);
        when(mTabModel.iterator()).thenReturn(List.of(tab456).iterator());

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel tabModel = adapter.getModelList().get(0).model;

        TabActionListener clickListener = tabModel.get(TabProperties.TAB_CLICK_LISTENER);
        assertNotNull("Tab click listener should be bound to model", clickListener);
        clickListener.run(null, 456, null);

        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
    }

    @Test
    @SmallTest
    public void testTabModelSwap_ResetsTabs() {
        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        TabModel newTabModel = mock(TabModel.class);
        when(newTabModel.getProfile()).thenReturn(mProfile);
        when(newTabModel.isTabModelRestored()).thenReturn(true);
        Tab newTab = prepareMockTab(789);
        when(newTabModel.getRepresentativeTabList()).thenReturn(List.of(newTab));
        when(newTabModel.iterator()).thenReturn(List.of(newTab).iterator());
        when(newTabModel.getTabById(789)).thenReturn(newTab);

        mCurrentTabModelSupplier.set(newTabModel);

        assertEquals(1, adapter.getModelList().size());
        assertEquals(789, adapter.getModelList().get(0).model.get(TabProperties.TAB_ID));
    }

    @Test
    @SmallTest
    public void testGridButtonClick() {
        createCoordinator();
        ImageButton gridButton = mCoordinator.getView().findViewById(R.id.grid_button);
        assertNotNull(gridButton);
        gridButton.performClick();
        verify(mVerticalTabsActionDelegate).openHubPane(PaneId.TAB_GROUPS);
    }

    @Test
    @SmallTest
    public void testTabSearchButtonClick() {
        createCoordinator();
        ImageButton tabSearchButton = mCoordinator.getView().findViewById(R.id.tab_search_button);
        assertNotNull(tabSearchButton);
        tabSearchButton.performClick();
        verify(mVerticalTabsActionDelegate).openHubPane(PaneId.TAB_SWITCHER);
    }

    @Test
    @SmallTest
    public void testNewTabButtonClick() {
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        createCoordinator();
        ImageButton newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        newTabButton.performClick();
        verify(mTabModel).commitAllTabClosures();
        verify(mTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
    }

    @Test
    @SmallTest
    public void testNewTabButtonClick_Incognito() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        createCoordinator();
        ImageButton newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        newTabButton.performClick();
        verify(mTabModel, never()).commitAllTabClosures();
        verify(mTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
    }
}
