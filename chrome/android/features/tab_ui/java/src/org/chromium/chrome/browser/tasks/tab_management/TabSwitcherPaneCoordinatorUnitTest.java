// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_CLIP_TO_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_CONTENT_SENSITIVE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.PAGE_KEY_LISTENER;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.Collections;

/**
 * Unit tests for {@link TabSwitcherPaneCoordinator}. These are mostly for coverage and to confirm
 * nothing will crash since the bulk of the behaviors from the coordinator are either unit tested by
 * classes hosted insider the coordinator or have to be verified in an integration test.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabContentManager mTabContentManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ScrimManager mScrimManager;
    @Mock private DataSharingService mDataSharingService;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabSwitcherMessageManager mMessageManager;
    @Mock private TabSwitcherResetHandler mResetHandler;
    @Mock private Callback<Integer> mOnTabClickedCallback;
    @Mock private Callback<Boolean> mHairlineVisibilityCallback;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private Tracker mTracker;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private ShareDelegateSupplier mShareDelegateSupplier;
    @Mock private TabBookmarker mTabBookmarker;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private UndoBarThrottle mUndoBarThrottle;
    @Mock private TabGridContextMenuCoordinator mTabGridContextMenuCoordinator;
    @Mock private TabListGroupMenuCoordinator mTabListGroupMenuCoordinator;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<TabGroupModelFilter> mTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsVisibleSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsAnimatingSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<TabBookmarker> mTabBookmarkerSupplier =
            new ObservableSupplierImpl<>(mTabBookmarker);

    private MockTabModel mTabModel;
    private Activity mActivity;
    private FrameLayout mRootView;
    private FrameLayout mContainerView;
    private FrameLayout mCoordinatorView;
    private TabSwitcherPaneCoordinator mCoordinator;
    private boolean mDestroyed;

    @Before
    public void setUp() {
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);

        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);

        TrackerFactory.setTrackerForTests(mTracker);

        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        mTabModel = new MockTabModel(mProfile, null);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);

        mProfileProviderSupplier.set(mProfileProvider);
        mTabGroupModelFilterSupplier.set(mTabGroupModelFilter);
        mIsVisibleSupplier.set(false);
        mIsAnimatingSupplier.set(false);

        BookmarkModel.setInstanceForTesting(mBookmarkModel);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
        mRootView = new FrameLayout(activity);
        mCoordinatorView = new FrameLayout(activity);
        mContainerView = new FrameLayout(activity);
        mRootView.addView(mContainerView);
        mCoordinatorView.setId(R.id.coordinator);
        mRootView.addView(mCoordinatorView);
        activity.setContentView(mRootView);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSwitcher.SetupRecyclerView.Time");
        mDestroyed = false;
        mCoordinator =
                new TabSwitcherPaneCoordinator(
                        activity,
                        mProfileProviderSupplier,
                        mTabGroupModelFilterSupplier,
                        mTabContentManager,
                        mBrowserControlsStateProvider,
                        mScrimManager,
                        mModalDialogManager,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mMessageManager,
                        mContainerView,
                        mResetHandler,
                        mIsVisibleSupplier,
                        mIsAnimatingSupplier,
                        mOnTabClickedCallback,
                        mHairlineVisibilityCallback,
                        TabListMode.GRID,
                        /* supportsEmptyState= */ true,
                        /* onTabGroupCreation= */ null,
                        () -> {
                            mDestroyed = true;
                        },
                        mEdgeToEdgeSupplier,
                        /* desktopWindowStateManager= */ null,
                        mShareDelegateSupplier,
                        mTabBookmarkerSupplier,
                        mUndoBarThrottle);
        watcher.assertExpected();

        mCoordinator.initWithNative();

        mIsVisibleSupplier.set(true);

        verify(mMessageManager).registerMessages(any());
        verify(mMessageManager).bind(any(), any(), any(), any());
    }

    DialogController showTabGridDialogWithTabs() {
        ViewStub dialogStub = new ViewStub(mActivity);
        mCoordinatorView.addView(dialogStub);
        dialogStub.setId(R.id.tab_grid_dialog_stub);

        DialogController controller = mCoordinator.getTabGridDialogControllerForTesting();
        MockTab tab = MockTab.createAndInitialize(/* id= */ 1, mProfile);
        tab.setIsInitialized(true);
        int index = 0;
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        when(mTabGroupModelFilter.representativeIndexOf(tab)).thenReturn(index);
        when(mTabGroupModelFilter.getRepresentativeTabAt(index)).thenReturn(tab);
        controller.resetWithListOfTabs(Collections.singletonList(tab));

        return controller;
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        assertTrue(mDestroyed);
    }

    @Test
    public void testShowTabListEditor() {
        ObservableSupplier<Boolean> handlesBackPressSupplier =
                mCoordinator.getHandleBackPressChangedSupplier();
        assertFalse(handlesBackPressSupplier.get());

        mCoordinator.showTabListEditor();
        assertTrue(handlesBackPressSupplier.get());

        assertNotNull(mActivity.findViewById(R.id.selectable_list));

        assertEquals(BackPressResult.SUCCESS, mCoordinator.handleBackPress());
        assertFalse(handlesBackPressSupplier.get());

        assertNull(mActivity.findViewById(R.id.selectable_list));
    }

    @Test
    public void testSetInitialScrollIndexOffset() {
        int index = 8;
        when(mTabGroupModelFilter.getCurrentRepresentativeTabIndex()).thenReturn(index);
        mCoordinator.setInitialScrollIndexOffset();

        assertEquals(
                index,
                mCoordinator
                        .getContainerViewModelForTesting()
                        .get(INITIAL_SCROLL_INDEX)
                        .intValue());
    }

    @Test
    public void testRequestAccessibilityFocusOnCurrentTab() {
        int index = 2;
        when(mTabGroupModelFilter.getCurrentRepresentativeTabIndex()).thenReturn(index);
        mCoordinator.requestAccessibilityFocusOnCurrentTab();

        assertEquals(
                index,
                mCoordinator
                        .getContainerViewModelForTesting()
                        .get(FOCUS_TAB_INDEX_FOR_ACCESSIBILITY)
                        .intValue());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.DATA_SHARING})
    public void testTabGridDialogVisibilitySupplier() {

        Supplier<Boolean> tabGridDialogVisibilitySupplier =
                mCoordinator.getTabGridDialogVisibilitySupplier();

        assertFalse(tabGridDialogVisibilitySupplier.get());

        DialogController controller = showTabGridDialogWithTabs();
        assertTrue(tabGridDialogVisibilitySupplier.get());

        controller.hideDialog(false);
        assertFalse(tabGridDialogVisibilitySupplier.get());
    }

    @Test
    public void testCustomViewManager() {
        TabSwitcherCustomViewManager.Delegate customViewManagerDelegate =
                mCoordinator.getTabSwitcherCustomViewManagerDelegate();
        assertNotNull(customViewManagerDelegate);

        FrameLayout customView = new FrameLayout(mActivity);
        customViewManagerDelegate.addCustomView(customView, null, false);
        boolean found = false;
        for (int i = 0; i < mContainerView.getChildCount(); i++) {
            if (mContainerView.getChildAt(i) == customView) {
                found = true;
            }
        }
        assertTrue("Did not find added custom view.", found);

        customViewManagerDelegate.removeCustomView(customView);
        found = false;
        for (int i = 0; i < mContainerView.getChildCount(); i++) {
            if (mContainerView.getChildAt(i) == customView) {
                found = true;
            }
        }
        assertFalse("Did not remove custom view", found);
    }

    @Test
    public void testShowTab() {
        @TabId int tabId = 1;
        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        tab.setIsInitialized(true);
        int index = 0;
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        when(mTabGroupModelFilter.representativeIndexOf(tab)).thenReturn(index);
        when(mTabGroupModelFilter.getRepresentativeTabAt(index)).thenReturn(tab);
        when(mTabGroupModelFilter.getIndividualTabAndGroupCount()).thenReturn(1);
        when(mTabGroupModelFilter.getRelatedTabList(tabId))
                .thenReturn(Collections.singletonList(tab));

        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        doCallback(2, (Callback<Bitmap> callback) -> callback.onResult(bitmap))
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(eq(tabId), any(), any());
        mCoordinator.resetWithListOfTabs(Collections.singletonList(tab));

        TabListRecyclerView recyclerView = mActivity.findViewById(R.id.tab_list_recycler_view);
        // Manually size the view so that the children get added this is to work around robolectric
        // view testing limitations.
        recyclerView.measure(0, 0);
        recyclerView.layout(0, 0, 100, 1000);

        assertEquals(1, recyclerView.getAdapter().getItemCount());
        assertEquals(1, recyclerView.getChildCount());
        // This gets called three times
        // 1) Once when the fetcher is set.
        // 2) Twice due to thumbnail size changes on initial and repeat layout.
        verify(mTabContentManager, times(3)).getTabThumbnailWithCallback(eq(tabId), any(), any());

        TabThumbnailView thumbnailView = mActivity.findViewById(R.id.tab_thumbnail);
        assertNotNull(thumbnailView);
        assertFalse(thumbnailView.isPlaceholder());

        mIsVisibleSupplier.set(false);

        verify(mMessageManager, times(2)).unbind(any());

        mCoordinator.softCleanup();
        assertTrue(thumbnailView.isPlaceholder());

        mCoordinator.hardCleanup();
        assertEquals(0, recyclerView.getAdapter().getItemCount());
        // Don't assert on the actual child count, robolectric isn't removing the child view for
        // some reason.
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    public void testEdgeToEdgePadAdjuster() {
        int originalPadding = mCoordinator.getContainerViewModelForTesting().get(BOTTOM_PADDING);
        var padAdjuster = mCoordinator.getEdgeToEdgePadAdjusterForTesting();
        assertNotNull("Pad adjuster should be created when feature enabled.", padAdjuster);

        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(eq(padAdjuster));

        int bottomInsets = 50;
        padAdjuster.overrideBottomInset(bottomInsets);
        assertFalse(
                "Not clip to padding when bottom insets > 0",
                mCoordinator.getContainerViewModelForTesting().get(IS_CLIP_TO_PADDING));
        assertEquals(
                "Bottom insets should be added to the bottom padding.",
                originalPadding + bottomInsets,
                mCoordinator.getContainerViewModelForTesting().get(BOTTOM_PADDING));
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    public void testEdgeToEdgePadAdjuster_FeatureDisabled() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        var padAdjuster = mCoordinator.getEdgeToEdgePadAdjusterForTesting();
        assertNull("Pad adjuster should be created when feature enabled.", padAdjuster);
    }

    @Test
    public void testSetTabSwitcherContentSensitivity() {
        PropertyModel containerViewModel = mCoordinator.getContainerViewModelForTesting();
        assertFalse(containerViewModel.get(IS_CONTENT_SENSITIVE));
        mCoordinator.setTabSwitcherContentSensitivity(/* contentIsSensitive= */ true);
        assertTrue(containerViewModel.get(IS_CONTENT_SENSITIVE));
        mCoordinator.setTabSwitcherContentSensitivity(/* contentIsSensitive= */ false);
        assertFalse(containerViewModel.get(IS_CONTENT_SENSITIVE));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void testOnLongPressOnTabCard_FeatureDisabled() {
        View cardView = new View(mActivity);
        mCoordinator.onLongPressOnTabCard(
                mTabGridContextMenuCoordinator, mTabListGroupMenuCoordinator, 1, cardView);

        verify(mTabGridContextMenuCoordinator, never()).showMenu(any(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void testOnLongPressOnTabCard_FeatureEnabled_NotGrouped() {
        View cardView = new View(mActivity);

        @TabId int tabId = 1;
        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mCoordinator.onLongPressOnTabCard(
                mTabGridContextMenuCoordinator, mTabListGroupMenuCoordinator, tabId, cardView);
        verify(mTabGridContextMenuCoordinator).showMenu(any(ViewRectProvider.class), eq(tabId));
        verify(mTabListGroupMenuCoordinator, never()).showMenu(any(ViewRectProvider.class), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void testOnLongPressOnTabCard_FeatureEnabled_Grouped() {
        View cardView = new View(mActivity);

        @TabId int tabId = 1;
        Token groupId = Token.createRandom();

        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        tab.setTabGroupId(groupId);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mCoordinator.onLongPressOnTabCard(
                mTabGridContextMenuCoordinator, mTabListGroupMenuCoordinator, tabId, cardView);
        verify(mTabGridContextMenuCoordinator, never()).showMenu(any(), anyInt());
        verify(mTabListGroupMenuCoordinator).showMenu(any(), eq(groupId));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void testOnLongPressOnTabCard_FeatureEnabled_NullCardView() {
        @TabId int tabId = 1;
        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mCoordinator.onLongPressOnTabCard(
                mTabGridContextMenuCoordinator, mTabListGroupMenuCoordinator, tabId, null);
        verify(mTabGridContextMenuCoordinator, never()).showMenu(any(), anyInt());
        verify(mTabListGroupMenuCoordinator, never()).showMenu(any(), any());
    }

    @Test
    public void testGetPageKeyListener() {
        assertNotNull(mCoordinator.getContainerViewModelForTesting().get(PAGE_KEY_LISTENER));
        showTabGridDialogWithTabs();
        assertNotNull(
                mCoordinator
                        .getTabGridDialogCoordinatorForTesting()
                        .getModelForTesting()
                        .get(TabGridDialogProperties.PAGE_KEY_LISTENER));
    }
}
