// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityActionHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Implementation for {@code RecentActivityActionHandler}. */
public class RecentActivityActionHandlerImpl implements RecentActivityActionHandler {
    private final TabGroupSyncService mTabGroupSyncService;
    private final TabModelSelector mTabModelSelector;
    private final DataSharingTabGroupsDelegate mDataSharingTabGroupsDelegate;
    private final String mCollaborationId;
    private final String mSyncTabGroupId;
    private final Runnable mManageSharingCallback;

    /**
     * Constructor.
     *
     * @param tabGroupSyncService The tab group sync backend.
     * @param tabModelSelector The tab model selector to provide access to tab model for focusing
     *     and opening tabs.
     * @param dataSharingTabGroupsDelegate Delegate to open the tab group edit dialog.
     * @param collaborationId The collaboration ID.
     * @param syncTabGroupId The tab group sync ID as referred in {@link TabGroupSyncService}.
     * @param manageSharingCallback The callback to open people group management screen.
     */
    public RecentActivityActionHandlerImpl(
            TabGroupSyncService tabGroupSyncService,
            TabModelSelector tabModelSelector,
            DataSharingTabGroupsDelegate dataSharingTabGroupsDelegate,
            String collaborationId,
            String syncTabGroupId,
            Runnable manageSharingCallback) {
        mTabGroupSyncService = tabGroupSyncService;
        mTabModelSelector = tabModelSelector;
        mDataSharingTabGroupsDelegate = dataSharingTabGroupsDelegate;
        mCollaborationId = collaborationId;
        mSyncTabGroupId = syncTabGroupId;
        mManageSharingCallback = manageSharingCallback;
        assert mCollaborationId != null;
    }

    @Override
    public void focusTab(int tabId) {
        mDataSharingTabGroupsDelegate.hideTabSwitcherAndShowTab(tabId);
    }

    @Override
    public void reopenTab(String url) {
        SavedTabGroup savedTabGroup = getSavedTabGroup();
        assert savedTabGroup != null;
        assert savedTabGroup.localId != null;
        TabGroupModelFilter tabGroupModelFilter =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(/* isIncognito= */ false);
        int rootId = tabGroupModelFilter.getRootIdFromStableId(savedTabGroup.localId.tabGroupId);
        assert rootId != Tab.INVALID_TAB_ID;

        TabGroupUtils.openUrlInGroup(
                tabGroupModelFilter, url, rootId, TabLaunchType.FROM_TAB_GROUP_UI);
    }

    @Override
    public void openTabGroupEditDialog() {
        SavedTabGroup savedTabGroup = getSavedTabGroup();
        assert savedTabGroup != null;
        assert !savedTabGroup.savedTabs.isEmpty();
        Integer tabId = savedTabGroup.savedTabs.get(0).localId;
        assert tabId != null;
        mDataSharingTabGroupsDelegate.openTabGroupWithTabId(tabId);
    }

    @Override
    public void manageSharing() {
        mManageSharingCallback.run();
    }

    private SavedTabGroup getSavedTabGroup() {
        return mTabGroupSyncService.getGroup(mSyncTabGroupId);
    }
}
