// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;
import java.util.Objects;

/** A wrapper for {@link SharedGroupObserver} that supports changing the observed tab group id. */
public class TransitiveSharedGroupObserver implements Destroyable {
    private final ObservableSupplierImpl<SharedGroupObserver> mCurrentSharedGroupObserverSupplier =
            new ObservableSupplierImpl<>();
    private final TransitiveObservableSupplier<SharedGroupObserver, Integer>
            mGroupSharedStateSupplier;
    private final TransitiveObservableSupplier<SharedGroupObserver, List<GroupMember>>
            mGroupMembersSupplier;
    private final TransitiveObservableSupplier<SharedGroupObserver, String>
            mCollaborationIdSupplier;
    private final TabGroupSyncService mTabGroupSyncService;
    private final DataSharingService mDataSharingService;
    private final CollaborationService mCollaborationService;

    private @Nullable Token mCurrentTabGroupId;

    /**
     * @param tabGroupSyncService Used to fetch the current collaboration id of the group.
     * @param dataSharingService Used to observe current share data.
     * @param collaborationService Used to fetch current shared data.
     */
    public TransitiveSharedGroupObserver(
            @NonNull TabGroupSyncService tabGroupSyncService,
            @NonNull DataSharingService dataSharingService,
            @NonNull CollaborationService collaborationService) {
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
        mCollaborationService = collaborationService;

        mGroupSharedStateSupplier =
                new TransitiveObservableSupplier<>(
                        mCurrentSharedGroupObserverSupplier,
                        sharedGroupStateObserver ->
                                sharedGroupStateObserver.getGroupSharedStateSupplier());
        mGroupMembersSupplier =
                new TransitiveObservableSupplier<>(
                        mCurrentSharedGroupObserverSupplier,
                        sharedGroupStateObserver ->
                                sharedGroupStateObserver.getGroupMembersSupplier());
        mCollaborationIdSupplier =
                new TransitiveObservableSupplier<>(
                        mCurrentSharedGroupObserverSupplier,
                        sharedGroupStateObserver ->
                                sharedGroupStateObserver.getCollaborationIdSupplier());
    }

    @Override
    public void destroy() {
        mCurrentTabGroupId = null;
        swapSharedGroupObserver(/* newObserver= */ null);
    }

    /** Sets the tab group id to observe and create a corresponding observer. */
    public void setTabGroupId(@Nullable Token tabGroupId) {
        if (Objects.equals(tabGroupId, mCurrentTabGroupId)) return;

        mCurrentTabGroupId = tabGroupId;

        @Nullable
        SharedGroupObserver newObserver =
                tabGroupId == null
                        ? null
                        : new SharedGroupObserver(
                                tabGroupId,
                                mTabGroupSyncService,
                                mDataSharingService,
                                mCollaborationService);

        swapSharedGroupObserver(newObserver);
    }

    /** The held value corresponds to {@link GroupSharedState}. */
    public ObservableSupplier<Integer> getGroupSharedStateSupplier() {
        return mGroupSharedStateSupplier;
    }

    /** The held value corresponds to the list of {@link GroupMember} for the group. */
    public ObservableSupplier<List<GroupMember>> getGroupMembersSupplier() {
        return mGroupMembersSupplier;
    }

    /** The held value corresponds to the collaboration id for the group. */
    public ObservableSupplier<String> getCollaborationIdSupplier() {
        return mCollaborationIdSupplier;
    }

    private void swapSharedGroupObserver(@Nullable SharedGroupObserver newObserver) {
        if (mCurrentSharedGroupObserverSupplier.hasValue()) {
            mCurrentSharedGroupObserverSupplier.get().destroy();
        }

        mCurrentSharedGroupObserverSupplier.set(newObserver);
    }
}
