// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService.Observer;
import org.chromium.components.tab_group_sync.TriggerSource;

import java.util.List;
import java.util.Objects;

/** Provides a simple interface to watch shared state for a single tab group. */
public class SharedGroupObserver implements Destroyable {
    private final DataSharingService.Observer mShareObserver =
            new DataSharingService.Observer() {
                @Override
                public void onGroupChanged(GroupData groupData) {
                    updateForNonDeletedGroupData(groupData);
                }

                @Override
                public void onGroupAdded(GroupData groupData) {
                    updateForNonDeletedGroupData(groupData);
                }

                @Override
                public void onGroupRemoved(String groupId) {
                    updateForDeletedGroupId(groupId);
                }
            };

    private final TabGroupSyncService.Observer mSyncObserver =
            new Observer() {
                @Override
                public void onTabGroupUpdated(SavedTabGroup group, @TriggerSource int source) {
                    updateForSyncChange(group);
                }
            };

    private final ObservableSupplierImpl<Integer> mGroupSharedStateSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<List<GroupMember>> mGroupMembersSupplier =
            new ObservableSupplierImpl<>();
    // Track a matching collaboration id because it allows us to not assume sync will still give the
    // old collaboration id if the group is deleted.
    private final ObservableSupplierImpl<String> mCurrentCollaborationIdSupplier =
            new ObservableSupplierImpl<>();
    private final LocalTabGroupId mLocalTabGroupId;
    private final TabGroupSyncService mTabGroupSyncService;
    private final DataSharingService mDataSharingService;
    private final CollaborationService mCollaborationService;

    /**
     * @param tabGroupId The id of the tab group.
     * @param tabGroupSyncService Used to fetch the current collaboration id of the group.
     * @param dataSharingService Used to observe current share data.
     * @param collaborationService Used to fetch current share data.
     */
    public SharedGroupObserver(
            @NonNull Token tabGroupId,
            @NonNull TabGroupSyncService tabGroupSyncService,
            @NonNull DataSharingService dataSharingService,
            @NonNull CollaborationService collaborationService) {
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
        mCollaborationService = collaborationService;
        mLocalTabGroupId = new LocalTabGroupId(tabGroupId);

        @Nullable SavedTabGroup group = mTabGroupSyncService.getGroup(mLocalTabGroupId);
        if (group == null || !TabShareUtils.isCollaborationIdValid(group.collaborationId)) {
            mGroupSharedStateSupplier.set(GroupSharedState.NOT_SHARED);
            mGroupMembersSupplier.set(null);
        } else {
            mCurrentCollaborationIdSupplier.set(group.collaborationId);
            @Nullable
            GroupData groupData = collaborationService.getGroupData(group.collaborationId);
            updateOurGroupData(groupData);
        }

        tabGroupSyncService.addObserver(mSyncObserver);
        dataSharingService.addObserver(mShareObserver);
    }

    @Override
    public void destroy() {
        mTabGroupSyncService.removeObserver(mSyncObserver);
        mDataSharingService.removeObserver(mShareObserver);
    }

    /**
     * The held value corresponds to {@link GroupSharedState}. Though upon the initial construction
     * of this class it is possible there's no value set yet. This would be because there is a
     * collaboration id and an outstanding request to read the group from the sharing service.
     */
    public ObservableSupplier<Integer> getGroupSharedStateSupplier() {
        return mGroupSharedStateSupplier;
    }

    /**
     * The held value contains the list of members of the group. Upon the initial construction of
     * this class it is possible there's no value set yet.
     */
    public ObservableSupplier<List<GroupMember>> getGroupMembersSupplier() {
        return mGroupMembersSupplier;
    }

    /**
     * The held value corresponds to the collaboration id for the group. Upon initial construction
     * of this class the value will be up-to-date. May be transiently out of sync with the state
     * held by {@link #getGroupSharedStateSupplier()} if async update are in flight.
     */
    public ObservableSupplier<String> getCollaborationIdSupplier() {
        return mCurrentCollaborationIdSupplier;
    }

    private void updateOurGroupData(@Nullable GroupData groupData) {
        mGroupSharedStateSupplier.set(TabShareUtils.discernSharedGroupState(groupData));
        mGroupMembersSupplier.set(TabShareUtils.getGroupMembers(groupData));
    }

    private void updateForNonDeletedGroupData(@Nullable GroupData groupData) {
        if (isOurGroup(groupData)) {
            updateOurGroupData(groupData);
        }
    }

    private void updateForDeletedGroupId(@Nullable String groupId) {
        if (Objects.equals(groupId, mCurrentCollaborationIdSupplier.get())) {
            mCurrentCollaborationIdSupplier.set(null);
            mGroupSharedStateSupplier.set(GroupSharedState.NOT_SHARED);
            mGroupMembersSupplier.set(null);
        }
    }

    private boolean isOurGroup(@Nullable GroupData groupData) {
        @Nullable String currentCollaborationId = mCurrentCollaborationIdSupplier.get();
        if (groupData == null
                || groupData.groupToken == null
                || !TabShareUtils.isCollaborationIdValid(groupData.groupToken.collaborationId)) {
            return false;
        } else if (TabShareUtils.isCollaborationIdValid(currentCollaborationId)) {
            return Objects.equals(groupData.groupToken.collaborationId, currentCollaborationId);
        } else {
            @Nullable SavedTabGroup syncGroup = mTabGroupSyncService.getGroup(mLocalTabGroupId);
            boolean matches =
                    syncGroup != null
                            && Objects.equals(
                                    syncGroup.collaborationId,
                                    groupData.groupToken.collaborationId);
            if (matches) {
                mCurrentCollaborationIdSupplier.set(syncGroup.collaborationId);
            }
            return matches;
        }
    }

    private void updateForSyncChange(SavedTabGroup group) {
        if (!Objects.equals(mLocalTabGroupId, group.localId)
                || TextUtils.equals(group.collaborationId, mCurrentCollaborationIdSupplier.get())) {
            return;
        }
        String newCollaborationId = group.collaborationId;
        GroupData groupData = mCollaborationService.getGroupData(newCollaborationId);
        mCurrentCollaborationIdSupplier.set(newCollaborationId);
        updateOurGroupData(groupData);
    }
}
