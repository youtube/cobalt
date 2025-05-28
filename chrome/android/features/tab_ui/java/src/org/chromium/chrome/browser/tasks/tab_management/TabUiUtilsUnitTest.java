// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.EMAIL1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.EMAIL2;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GAIA_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GAIA_ID2;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;
import static org.chromium.ui.test.util.MockitoHelper.runWithValue;

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

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.MaybeBlockingResult;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SyncedGroupTestHelper;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/** Unit tests for {@link TabUiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabUiUtilsUnitTest {
    private static final int TAB_ID = 123;
    private static final int ROOT_ID = TAB_ID;
    private static final String GROUP_TITLE = "My Group";
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mFilter;
    @Mock private TabRemover mTabRemover;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private Callback<Boolean> mDidCloseTabsCallback;
    @Mock private Callback<Boolean> mContentSensitivitySetter;
    @Mock private Runnable mFinishBlocking;

    @Captor private ArgumentCaptor<TabModelActionListener> mTabModelActionListenerCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mOutcomeCaptor;

    private List<Tab> mTabsToClose;
    private SyncedGroupTestHelper mSyncedGroupTestHelper;

    @Before
    public void setUp() {
        mTabsToClose = List.of(mTab);
        mSyncedGroupTestHelper = new SyncedGroupTestHelper(mTabGroupSyncService);

        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mFilter.getTabModel()).thenReturn(mTabModel);
        when(mFilter.isIncognitoBranded()).thenReturn(false);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.getRootId()).thenReturn(ROOT_ID);
        when(mFilter.getRelatedTabListForRootId(ROOT_ID)).thenReturn(mTabsToClose);
        when(mFilter.getRelatedTabCountForRootId(ROOT_ID)).thenReturn(mTabsToClose.size());
        when(mFilter.getTabGroupTitle(ROOT_ID)).thenReturn(GROUP_TITLE);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.isClosing()).thenReturn(false);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
    }

    @Test
    public void testCloseTabGroup_NoTab() {
        TabUiUtils.closeTabGroup(
                mFilter, Tab.INVALID_TAB_ID, /* hideTabGroups= */ false, mDidCloseTabsCallback);
        verify(mDidCloseTabsCallback).onResult(false);
    }

    @Test
    public void testCloseTabGroup_NoHide() {
        boolean hideTabGroups = false;

        TabUiUtils.closeTabGroup(mFilter, TAB_ID, hideTabGroups, mDidCloseTabsCallback);

        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mFilter, ROOT_ID)
                                        .hideTabGroups(hideTabGroups)
                                        .allowUndo(true)
                                        .build()),
                        eq(true),
                        mTabModelActionListenerCaptor.capture());

        // These are the known valid combinations only:
        TabModelActionListener listener = mTabModelActionListenerCaptor.getValue();

        listener.onConfirmationDialogResult(
                DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mDidCloseTabsCallback).onResult(true);

        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mDidCloseTabsCallback, times(2)).onResult(true);

        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mDidCloseTabsCallback, times(3)).onResult(true);

        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mDidCloseTabsCallback).onResult(false);

        listener.onConfirmationDialogResult(
                DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mDidCloseTabsCallback, times(4)).onResult(true);

        listener.onConfirmationDialogResult(
                DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mDidCloseTabsCallback, times(5)).onResult(true);
    }

    @Test
    public void testCloseTabGroup_Hide() {
        boolean hideTabGroups = true;

        TabUiUtils.closeTabGroup(mFilter, TAB_ID, hideTabGroups, mDidCloseTabsCallback);

        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mFilter, ROOT_ID)
                                        .hideTabGroups(hideTabGroups)
                                        .allowUndo(true)
                                        .build()),
                        eq(true),
                        mTabModelActionListenerCaptor.capture());
    }

    @Test
    public void testDeleteSharedTabGroup_Positive() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());
        mockIdentity(EMAIL1, GAIA_ID1);
        createSyncGroup(COLLABORATION_ID1);
        createSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.OWNER);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager).processDeleteSharedGroupAttempt(eq(GROUP_TITLE), any());
        verify(mCollaborationService).deleteGroup(eq(COLLABORATION_ID1), mOutcomeCaptor.capture());

        mOutcomeCaptor.getValue().onResult(false);
        verify(mModalDialogManager).showDialog(any(), anyInt());
        verify(mFinishBlocking).run();
    }

    @Test
    public void testDeleteSharedTabGroup_Negative() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_NEGATIVE, null))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());
        mockIdentity(EMAIL1, GAIA_ID1);
        createSyncGroup(COLLABORATION_ID1);
        createSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.OWNER);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager).processDeleteSharedGroupAttempt(eq(GROUP_TITLE), any());
        verify(mCollaborationService, never()).deleteGroup(any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testDeleteSharedTabGroup_NullTab() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());

        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        createSyncGroup(COLLABORATION_ID1);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processDeleteSharedGroupAttempt(any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testDeleteSharedTabGroup_NullTabGroupId() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());
        when(mTab.getTabGroupId()).thenReturn(null);
        createSyncGroup(COLLABORATION_ID1);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processDeleteSharedGroupAttempt(any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testDeleteSharedTabGroup_NullSavedTabGroup() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processDeleteSharedGroupAttempt(any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testDeleteSharedTabGroup_NullCollaborationId() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());
        createSyncGroup(/* collaborationId= */ null);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processDeleteSharedGroupAttempt(any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testLeaveSharedTabGroup_Positive() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());
        mockIdentity(EMAIL2, GAIA_ID2);
        createSyncGroup(COLLABORATION_ID1);
        createSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.MEMBER);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager).processLeaveGroupAttempt(eq(GROUP_TITLE), any());
        verify(mCollaborationService).leaveGroup(eq(COLLABORATION_ID1), mOutcomeCaptor.capture());

        mOutcomeCaptor.getValue().onResult(false);
        verify(mModalDialogManager).showDialog(any(), anyInt());
        verify(mFinishBlocking).run();
    }

    @Test
    public void testLeaveSharedTabGroup_Negative() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_NEGATIVE, null))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());
        mockIdentity(EMAIL2, GAIA_ID2);
        SavedTabGroup group = createSyncGroup(COLLABORATION_ID1);
        group.title = null;
        when(mFilter.getTabGroupTitle(ROOT_ID)).thenReturn(null);
        createSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.MEMBER);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager).processLeaveGroupAttempt(eq("1 tab"), any());
        verify(mDataSharingService, never()).removeMember(any(), any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testLeaveSharedTabGroup_NullTab() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        mockIdentity(EMAIL1, GAIA_ID1);
        createSyncGroup(COLLABORATION_ID1);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processLeaveGroupAttempt(any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testLeaveSharedTabGroup_NullSavedTabGroup() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());
        mockIdentity(EMAIL1, GAIA_ID1);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processLeaveGroupAttempt(any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testLeaveSharedTabGroup_NullCoreAccountInfo() {
        runWithValue(
                        1,
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, mFinishBlocking))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());
        createSyncGroup(COLLABORATION_ID1);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(null);

        TabUiUtils.exitSharedTabGroupWithDialog(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processLeaveGroupAttempt(any(), any());
        verify(mFinishBlocking, never()).run();
    }

    @Test
    public void testUpdateViewContentSensitivityForListOfTabs() {
        List<Tab> tabList = List.of(mTab);
        final String histogram = "SensitiveContent.TabSwitching.RegularTabSwitcherPane.Sensitivity";

        HistogramWatcher histogramWatcherForTrueBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ true);
        when(mTab.getTabHasSensitiveContent()).thenReturn(true);
        TabUiUtils.updateViewContentSensitivityForTabs(
                tabList, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(true);
        histogramWatcherForTrueBucket.assertExpected();

        HistogramWatcher histogramWatcherForFalseBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ false);
        when(mTab.getTabHasSensitiveContent()).thenReturn(false);
        TabUiUtils.updateViewContentSensitivityForTabs(
                tabList, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(false);
        histogramWatcherForFalseBucket.assertExpected();
    }

    @Test
    public void testUpdateViewContentSensitivityForTabList() {
        final String histogram = "SensitiveContent.TabSwitching.BottomTabStripGroupUI.Sensitivity";

        when(mTabModel.getCount()).thenAnswer(invocation -> 1);
        when(mTabModel.getTabAt(0)).thenAnswer(invocation -> mTab);

        HistogramWatcher histogramWatcherForTrueBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ true);
        when(mTab.getTabHasSensitiveContent()).thenReturn(true);
        TabUiUtils.updateViewContentSensitivityForTabs(
                mTabModel, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(true);
        histogramWatcherForTrueBucket.assertExpected();

        HistogramWatcher histogramWatcherForFalseBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ false);
        when(mTab.getTabHasSensitiveContent()).thenReturn(false);
        TabUiUtils.updateViewContentSensitivityForTabs(
                mTabModel, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(false);
        histogramWatcherForFalseBucket.assertExpected();
    }

    private SavedTabGroup createSyncGroup(String collaborationId) {
        SavedTabGroup syncGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1, TAB_GROUP_ID);
        syncGroup.title = GROUP_TITLE;
        syncGroup.collaborationId = collaborationId;
        return syncGroup;
    }

    private GroupData createSharedGroup(GroupMember... members) {
        GroupData sharedGroup = SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, members);
        when(mCollaborationService.getGroupData(eq(COLLABORATION_ID1))).thenReturn(sharedGroup);
        return sharedGroup;
    }

    private void mockIdentity(String email, GaiaId gaiaId) {
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(email, gaiaId);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);
    }
}
