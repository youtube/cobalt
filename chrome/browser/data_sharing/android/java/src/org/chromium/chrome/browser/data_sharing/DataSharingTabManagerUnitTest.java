// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.ACCESS_TOKEN1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.collaboration.CollaborationControllerDelegateFactory;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.DataSharingService.ParseUrlResult;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseUrlStatus;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.SharedDataPreview;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.data_sharing.SharedTabGroupPreview;
import org.chromium.components.data_sharing.TabPreview;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SyncedGroupTestHelper;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link DataSharingTabManager} */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.COLLABORATION_FLOW_ANDROID)
public class DataSharingTabManagerUnitTest {
    private static final LocalTabGroupId LOCAL_ID = new LocalTabGroupId(Token.createRandom());
    private static final Integer TAB_ID = 456;
    private static final int TAB_GROUP_ROOT_ID = 148;
    private static final String TITLE = "Title";
    private static final GURL TEST_URL = JUnitTestGURLs.URL_1;

    private static final class JoinTestHelper {
        /* package */ final Callback<Boolean> mOnJoinFinished = this::onJoinFinished;

        private boolean mWasJoinFinished;

        /* package */ JoinTestHelper() {}

        /* package */ void waitForCallback() {
            CriteriaHelper.pollUiThreadForJUnit(() -> mWasJoinFinished);
        }

        private void onJoinFinished(boolean success) {
            assertTrue(success);
            mWasJoinFinished = true;
        }
    }

    private final OneshotSupplierImpl<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier =
            new OneshotSupplierImpl<>();
    private final JoinTestHelper mJoinTestHelper = new JoinTestHelper();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private CollaborationControllerDelegate mCollaborationControllerDelegate;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private DataSharingTabGroupsDelegate mDataSharingTabGroupsDelegate;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock private Callback<Boolean> mCreateGroupFinishedCallback;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private TabModel mTabModel;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<Callback<Integer>> mOutcomeCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mTabGroupPreviewCallbackCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;
    @Captor private ArgumentCaptor<ShareParams> mShareParamsCaptor;

    private DataSharingTabManager mDataSharingTabManager;
    private SavedTabGroup mSavedTabGroup;
    private Activity mActivity;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    private Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private SyncedGroupTestHelper mSyncedGroupTestHelper;

    @Before
    public void setUp() {
        DomDistillerUrlUtilsJni.setInstanceForTesting(mDistillerUrlUtilsJniMock);

        DataSharingServiceFactory.setForTesting(mDataSharingService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>(mTabModelSelector);
        mBottomSheetControllerSupplier = new ObservableSupplierImpl<>(mBottomSheetController);
        mShareDelegateSupplier = new ObservableSupplierImpl<>(mShareDelegate);
        mTabGroupUiActionHandlerSupplier.set(mTabGroupUiActionHandler);

        CollaborationControllerDelegateFactory collaborationControllerDelegateFactory =
                (type, runnable) -> {
                    return mCollaborationControllerDelegate;
                };

        mDataSharingTabManager =
                new DataSharingTabManager(
                        mTabModelSelectorSupplier,
                        mDataSharingTabGroupsDelegate,
                        mBottomSheetControllerSupplier,
                        mShareDelegateSupplier,
                        mWindowAndroid,
                        ApplicationProvider.getApplicationContext().getResources(),
                        mTabGroupUiActionHandlerSupplier,
                        collaborationControllerDelegateFactory);

        mDataSharingTabManager.initWithProfile(
                mProfile, mDataSharingService, mMessagingBackendService, mCollaborationService);

        mSyncedGroupTestHelper = new SyncedGroupTestHelper(mTabGroupSyncService);
        mSavedTabGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        mSavedTabGroup.collaborationId = COLLABORATION_ID1;
        mSavedTabGroup.localId = LOCAL_ID;
        mSavedTabGroup.savedTabs = SyncedGroupTestHelper.tabsFromIds(TAB_ID);
        mSavedTabGroup.savedTabs.get(0).url = new GURL("https://www.example.com/");

        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
    }

    private void mockSuccessfulParseDataSharingUrl() {
        GroupToken groupToken = new GroupToken(COLLABORATION_ID1, ACCESS_TOKEN1);
        ParseUrlResult result =
                new DataSharingService.ParseUrlResult(groupToken, ParseUrlStatus.SUCCESS);
        when(mDataSharingService.parseDataSharingUrl(any())).thenReturn(result);
    }

    private void mockPreviewApiFetch() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        ArgumentCaptor<Callback<DataSharingService.SharedDataPreviewOrFailureOutcome>>
                previewCallbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mDataSharingService)
                .getSharedEntitiesPreview(any(), previewCallbackCaptor.capture());
        previewCallbackCaptor.getValue().onResult(getPreviewData());
    }

    private void mockUnsuccessfulParseDataSharingUrl(@ParseUrlStatus int status) {
        assertNotEquals(ParseUrlStatus.SUCCESS, status);
        ParseUrlResult result =
                new DataSharingService.ParseUrlResult(/* groupToken= */ null, status);
        when(mDataSharingService.parseDataSharingUrl(any())).thenReturn(result);
    }

    private org.chromium.components.sync.protocol.GroupData getSyncGroupData() {
        return org.chromium.components.sync.protocol.GroupData.newBuilder()
                .setGroupId(COLLABORATION_ID1)
                .setDisplayName(TITLE)
                .build();
    }

    private DataSharingService.SharedDataPreviewOrFailureOutcome getPreviewData() {
        List<TabPreview> tabs = new ArrayList<>();
        tabs.add(new TabPreview(new GURL("https://example.com"), "example.com"));
        return new DataSharingService.SharedDataPreviewOrFailureOutcome(
                new SharedDataPreview(new SharedTabGroupPreview("title", tabs)),
                PeopleGroupActionFailure.UNKNOWN);
    }

    private void setupTabGroupFilterForOpenGroupWithId() {
        doReturn(mTabGroupModelFilterProvider)
                .when(mTabModelSelector)
                .getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getTabGroupModelFilter(anyBoolean());
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(SYNC_GROUP_ID1);
        doReturn(TAB_GROUP_ROOT_ID).when(mTabGroupModelFilter).getRootIdFromStableId(any());
    }

    @Test
    public void testNoProfile() {
        mDataSharingTabManager =
                new DataSharingTabManager(
                        mTabModelSelectorSupplier,
                        mDataSharingTabGroupsDelegate,
                        mBottomSheetControllerSupplier,
                        mShareDelegateSupplier,
                        mWindowAndroid,
                        ApplicationProvider.getApplicationContext().getResources(),
                        mTabGroupUiActionHandlerSupplier,
                        null);
        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);

        // Verify we never parse the URL without a profile.
        verify(mDataSharingService, never()).parseDataSharingUrl(TEST_URL);
    }

    @Test
    public void testInvalidUrl() {
        mockUnsuccessfulParseDataSharingUrl(ParseUrlStatus.UNKNOWN);

        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);

        // Verify sync is never checked when parsing error occurs.
        verify(mTabGroupSyncService, never()).getAllGroupIds();
    }

    @Test
    public void testJoinFlowWithExistingTabGroup() {
        mockSuccessfulParseDataSharingUrl();

        // Mock exist in local tab model.
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        Tab tab = new MockTab(TAB_ID, mProfile);
        doReturn(tab).when(mTabModel).getTabById(TAB_ID);
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(tab);
        setupTabGroupFilterForOpenGroupWithId();

        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);
        verify(mDataSharingTabGroupsDelegate).openTabGroupWithTabId(TAB_GROUP_ROOT_ID);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.COLLABORATION_FLOW_ANDROID)
    public void testJoinFlowWithCollaborationService() {
        mockSuccessfulParseDataSharingUrl();
        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);

        verify(mCollaborationService).startJoinFlow(any(), eq(TEST_URL));
    }

    @Test
    public void testJoinFlowWithExistingTabGroupSyncOnly() {
        mockSuccessfulParseDataSharingUrl();

        // Mock does not exist in local tab model.
        mSavedTabGroup.localId = null;

        doAnswer(
                        invocation -> {
                            mSavedTabGroup.localId = LOCAL_ID; // Set the localId when opened.
                            return null;
                        })
                .when(mTabGroupUiActionHandler)
                .openTabGroup(any());

        setupTabGroupFilterForOpenGroupWithId();

        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);

        verify(mTabGroupUiActionHandler).openTabGroup(SYNC_GROUP_ID1);
        verify(mDataSharingTabGroupsDelegate).openTabGroupWithTabId(TAB_GROUP_ROOT_ID);
    }

    @Test
    public void testJoinFlowWithNewTabGroup() {
        mockSuccessfulParseDataSharingUrl();

        mSyncedGroupTestHelper.removeTabGroup(SYNC_GROUP_ID1);

        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);

        mockPreviewApiFetch();

        ArgumentCaptor<DataSharingJoinUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingJoinUiConfig.class);
        verify(mDataSharingUiDelegate).showJoinFlow(uiConfigCaptor.capture());

        ArgumentCaptor<TabGroupSyncService.Observer> syncObserverCaptor =
                ArgumentCaptor.forClass(TabGroupSyncService.Observer.class);
        verify(mTabGroupSyncService).addObserver(syncObserverCaptor.capture());

        DataSharingJoinUiConfig uiConfig = uiConfigCaptor.getValue();
        assertEquals(COLLABORATION_ID1, uiConfig.getGroupToken().collaborationId);
        assertEquals(ACCESS_TOKEN1, uiConfig.getGroupToken().accessToken);

        assertNotNull(uiConfig.getJoinCallback());

        uiConfig.getJoinCallback()
                .onGroupJoinedWithWait(getSyncGroupData(), mJoinTestHelper.mOnJoinFinished);

        setupTabGroupFilterForOpenGroupWithId();
        syncObserverCaptor.getValue().onTabGroupAdded(mSavedTabGroup, TriggerSource.REMOTE);

        mJoinTestHelper.waitForCallback();
        verify(mDataSharingUiDelegate).destroyFlow(any());
    }

    @Test
    public void testJoinFlowWithNewTabGroupOpenedBeforeJoinCallback() {
        mockSuccessfulParseDataSharingUrl();

        mSyncedGroupTestHelper.removeTabGroup(SYNC_GROUP_ID1);

        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);

        mockPreviewApiFetch();

        ArgumentCaptor<DataSharingJoinUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingJoinUiConfig.class);
        verify(mDataSharingUiDelegate).showJoinFlow(uiConfigCaptor.capture());

        ArgumentCaptor<TabGroupSyncService.Observer> syncObserverCaptor =
                ArgumentCaptor.forClass(TabGroupSyncService.Observer.class);
        verify(mTabGroupSyncService).addObserver(syncObserverCaptor.capture());

        DataSharingJoinUiConfig uiConfig = uiConfigCaptor.getValue();
        assertEquals(COLLABORATION_ID1, uiConfig.getGroupToken().collaborationId);
        assertEquals(ACCESS_TOKEN1, uiConfig.getGroupToken().accessToken);

        assertNotNull(uiConfig.getJoinCallback());

        setupTabGroupFilterForOpenGroupWithId();
        syncObserverCaptor.getValue().onTabGroupAdded(mSavedTabGroup, TriggerSource.REMOTE);
        uiConfig.getJoinCallback()
                .onGroupJoinedWithWait(getSyncGroupData(), mJoinTestHelper.mOnJoinFinished);

        mJoinTestHelper.waitForCallback();
        verify(mDataSharingUiDelegate).destroyFlow(any());
    }

    @Test
    public void testManageSharing() {
        mDataSharingTabManager.showManageSharing(
                mActivity, COLLABORATION_ID1, /* finishRunnable= */ null);
    }

    @Test
    public void testDestroy() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mockSuccessfulParseDataSharingUrl();
        mSyncedGroupTestHelper.removeTabGroup(SYNC_GROUP_ID1);
        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);
        // Need to get an observer to verify destroy removes it.
        verify(mTabGroupSyncService).addObserver(any());

        mDataSharingTabManager.destroy();
        verify(mTabGroupSyncService).removeObserver(any());
        verify(mFaviconHelper).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.COLLABORATION_FLOW_ANDROID)
    public void testShareOrManageFlowWithCollaborationService() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);
        mDataSharingTabManager.createOrManageFlow(mActivity, /* syncId= */ null, LOCAL_ID, null);

        verify(mCollaborationService).startShareOrManageFlow(any(), eq(SYNC_GROUP_ID1));
    }

    @Test
    public void testCreateFlowWithExistingGroup() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        doReturn(TEST_URL).when(mDataSharingService).getDataSharingUrl(any());
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));

        mDataSharingTabManager.createOrManageFlow(mActivity, /* syncId= */ null, LOCAL_ID, null);

        ArgumentCaptor<DataSharingManageUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingManageUiConfig.class);
        verify(mDataSharingUiDelegate).showManageFlow(uiConfigCaptor.capture());

        DataSharingManageUiConfig uiConfig = uiConfigCaptor.getValue();
        assertEquals(COLLABORATION_ID1, uiConfig.getGroupToken().collaborationId);
        // Manage should not pass access token.
        assertNull(uiConfig.getGroupToken().accessToken);

        assertNotNull(uiConfig.getManageCallback());
        uiConfig.getManageCallback()
                .onShareInviteLinkClicked(new GroupToken(COLLABORATION_ID1, ACCESS_TOKEN1));

        verify(mDataSharingTabGroupsDelegate)
                .getPreviewBitmap(
                        eq(COLLABORATION_ID1), anyInt(), mTabGroupPreviewCallbackCaptor.capture());
        Bitmap preview = Bitmap.createBitmap(80, 80, Bitmap.Config.ARGB_8888);
        mTabGroupPreviewCallbackCaptor.getValue().onResult(preview);

        verify(mShareDelegate)
                .share(
                        mShareParamsCaptor.capture(),
                        any(),
                        eq(ShareDelegate.ShareOrigin.TAB_GROUP));
        ShareParams shareParams = mShareParamsCaptor.getValue();
        assertEquals("Shared tab group link expires in 48 hours", shareParams.getText());
        assertEquals(shareParams.getUrl(), TEST_URL.getSpec());
        assertEquals("Collaborate on tab group", shareParams.getTitle());
    }

    @Test
    public void testCreateFlowWithNewTabGroup() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(null).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        GroupData groupData = SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, GROUP_MEMBER1);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        doCallback(1, (Callback<GroupDataOrFailureOutcome> callback) -> callback.onResult(outcome))
                .when(mDataSharingService)
                .createGroup(any(), any());

        var groupDataProto = getSyncGroupData();

        doReturn(TEST_URL).when(mDataSharingService).getDataSharingUrl(any());
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));

        mSavedTabGroup.collaborationId = null;
        mSavedTabGroup.title = "test title";
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createOrManageFlow(
                mActivity, /* syncId= */ null, LOCAL_ID, mCreateGroupFinishedCallback);

        ArgumentCaptor<DataSharingCreateUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingCreateUiConfig.class);
        verify(mDataSharingUiDelegate).showCreateFlow(uiConfigCaptor.capture());

        DataSharingCreateUiConfig uiConfig = uiConfigCaptor.getValue();

        mSavedTabGroup.collaborationId = COLLABORATION_ID1;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(SYNC_GROUP_ID1);
        doReturn(new String[] {SYNC_GROUP_ID1}).when(mTabGroupSyncService).getAllGroupIds();

        assertNotNull(uiConfig.getCreateCallback());
        uiConfig.getCreateCallback()
                .onGroupCreatedWithWait(
                        groupDataProto,
                        (result) -> {
                            assertTrue(result);
                        });
        assertEquals("test title", uiConfig.getCommonConfig().getTabGroupName());

        verify(mDataSharingTabGroupsDelegate)
                .getPreviewBitmap(
                        eq(COLLABORATION_ID1), anyInt(), mTabGroupPreviewCallbackCaptor.capture());
        Bitmap preview = Bitmap.createBitmap(80, 80, Bitmap.Config.ARGB_8888);
        mTabGroupPreviewCallbackCaptor.getValue().onResult(preview);

        // Verifying DataSharingService createGroup API is called.
        verify(mTabGroupSyncService).makeTabGroupShared(LOCAL_ID, COLLABORATION_ID1);
        verify(mShareDelegate)
                .share(
                        mShareParamsCaptor.capture(),
                        any(),
                        eq(ShareDelegate.ShareOrigin.TAB_GROUP));
        ShareParams shareParams = mShareParamsCaptor.getValue();
        assertEquals("test title link expires in 48 hours", shareParams.getText());
        assertEquals(shareParams.getUrl(), TEST_URL.getSpec());
        assertEquals("Collaborate on tab group", shareParams.getTitle());
    }

    @Test
    public void testCreateFlowCancelled() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(null).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        GroupData groupData = SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, GROUP_MEMBER1);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        doCallback(1, (Callback<GroupDataOrFailureOutcome> callback) -> callback.onResult(outcome))
                .when(mDataSharingService)
                .createGroup(any(), any());

        doReturn(TEST_URL).when(mDataSharingService).getDataSharingUrl(any());
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));

        mSavedTabGroup.collaborationId = null;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createOrManageFlow(
                mActivity, /* syncId= */ null, LOCAL_ID, mCreateGroupFinishedCallback);

        ArgumentCaptor<DataSharingCreateUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingCreateUiConfig.class);
        verify(mDataSharingUiDelegate).showCreateFlow(uiConfigCaptor.capture());

        DataSharingCreateUiConfig uiConfig = uiConfigCaptor.getValue();

        assertNotNull(uiConfig.getCreateCallback());
        assertEquals("1 tab", uiConfig.getCommonConfig().getTabGroupName());
        uiConfig.getCreateCallback().onCancelClicked();

        // Verifying DataSharingService createGroup API is called.
        verify(mTabGroupSyncService, never()).makeTabGroupShared(any(), any());
        verify(mShareDelegate, never())
                .share(any(), any(), eq(ShareDelegate.ShareOrigin.TAB_GROUP));
    }

    @Test
    public void testCreateTabGroupAndShare_withSingleTab() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        doReturn(mProfile).when(mProfile).getOriginalProfile();

        mTabModelSelectorSupplier.set(mTabModelSelector);
        doReturn(mTabGroupModelFilterProvider)
                .when(mTabModelSelector)
                .getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getTabGroupModelFilter(anyBoolean());
        when(mTab.getTabGroupId()).thenReturn(null).thenReturn(LOCAL_ID.tabGroupId);

        mSavedTabGroup.collaborationId = null;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createTabGroupAndShare(
                mActivity, mTab, mCreateGroupFinishedCallback);

        verify(mTabGroupModelFilter).createSingleTabGroup(eq(mTab));
        verify(mDataSharingUiDelegate).showCreateFlow(any());
    }

    @Test
    public void testCreateTabGroupAndShare_withExistingTabGroup() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        doReturn(mProfile).when(mProfile).getOriginalProfile();

        mTabModelSelectorSupplier.set(mTabModelSelector);
        doReturn(mTabGroupModelFilterProvider)
                .when(mTabModelSelector)
                .getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getTabGroupModelFilter(anyBoolean());
        doReturn(LOCAL_ID.tabGroupId).when(mTab).getTabGroupId();

        mSavedTabGroup.collaborationId = null;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createTabGroupAndShare(
                mActivity, mTab, mCreateGroupFinishedCallback);

        verify(mDataSharingUiDelegate).showCreateFlow(any());
    }

    @Test
    public void testParseDataSharingUrlFailure() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mockUnsuccessfulParseDataSharingUrl(ParseUrlStatus.HOST_OR_PATH_MISMATCH_FAILURE);

        mDataSharingTabManager.initiateJoinFlow(mActivity, TEST_URL);
        verify(mModalDialogManager).showDialog(mPropertyModelCaptor.capture(), anyInt());

        ModalDialogProperties.Controller controller =
                mPropertyModelCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelCaptor.getValue(), ButtonType.POSITIVE);
        verify(mModalDialogManager).dismissDialog(any(), anyInt());
    }

    @Test
    public void testShowRecentActivity() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        // mDataSharingTabManager.setFaviconHelperForTesting(mFaviconHelper);
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);
        setupActivityLogItemsOnTheBackend();
        mDataSharingTabManager.showRecentActivity(mActivity, COLLABORATION_ID1);
        verify(mMessagingBackendService).getActivityLog(any());
    }

    @Test
    public void testPromoteTabGroup() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        setupTabGroupFilterForOpenGroupWithId();

        mDataSharingTabManager.promoteTabGroup(COLLABORATION_ID1);
        verify(mDataSharingTabGroupsDelegate).openTabGroupWithTabId(TAB_GROUP_ROOT_ID);
    }

    private void setupActivityLogItemsOnTheBackend() {
        List<ActivityLogItem> logItems = new ArrayList<>();

        // Add item that has action to focus tab.
        ActivityLogItem logItem1 = new ActivityLogItem();
        logItem1.collaborationEvent = CollaborationEvent.TAB_ADDED;
        setTabMetadata(logItem1, mSavedTabGroup.savedTabs.get(0).localId, null);
        logItems.add(logItem1);

        // Add item that has action to reopen tab.
        ActivityLogItem logItem2 = new ActivityLogItem();
        logItem2.collaborationEvent = CollaborationEvent.TAB_REMOVED;
        setTabMetadata(logItem2, null, "https://google.com");
        logItems.add(logItem2);

        // Add item that has action to show tab group dialog.
        ActivityLogItem logItem3 = new ActivityLogItem();
        logItem3.collaborationEvent = CollaborationEvent.TAB_GROUP_COLOR_UPDATED;
        setTabGroupMetadata(logItem3, mSavedTabGroup.localId);
        setTabMetadata(logItem3, null, null);
        logItems.add(logItem3);

        // Add item that has action to show share UI.
        ActivityLogItem logItem4 = new ActivityLogItem();
        logItem4.collaborationEvent = CollaborationEvent.COLLABORATION_MEMBER_ADDED;
        setCollaborationId(logItem4, COLLABORATION_ID1);
        setTabMetadata(logItem4, null, null);
        logItems.add(logItem4);

        when(mMessagingBackendService.getActivityLog(any())).thenReturn(logItems);
    }

    private void setCollaborationId(ActivityLogItem logItem, String collaborationId) {
        if (logItem.activityMetadata == null) logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.collaborationId = collaborationId;
    }

    private void setTabGroupMetadata(ActivityLogItem logItem, LocalTabGroupId localTabGroupId) {
        if (logItem.activityMetadata == null) logItem.activityMetadata = new MessageAttribution();
        if (logItem.activityMetadata.tabGroupMetadata == null) {
            logItem.activityMetadata.tabGroupMetadata = new TabGroupMessageMetadata();
        }
        logItem.activityMetadata.tabGroupMetadata.localTabGroupId = localTabGroupId;
    }

    private void setTabMetadata(
            ActivityLogItem logItem, @Nullable Integer tabId, @Nullable String lastKnownUrl) {
        if (logItem.activityMetadata == null) logItem.activityMetadata = new MessageAttribution();
        if (logItem.activityMetadata.tabMetadata == null) {
            logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        }
        TabMessageMetadata tabMessageMetadata = logItem.activityMetadata.tabMetadata;
        if (tabId != null) {
            tabMessageMetadata.localTabId = tabId;
        }
        if (!TextUtils.isEmpty(lastKnownUrl)) {
            tabMessageMetadata.lastKnownUrl = lastKnownUrl;
        }
    }
}
