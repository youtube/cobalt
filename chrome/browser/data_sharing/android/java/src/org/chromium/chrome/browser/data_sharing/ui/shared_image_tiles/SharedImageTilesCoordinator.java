// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.configs.AvatarConfig;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * A coordinator for SharedImageTiles component. This component is used to build a view and populate
 * shared image tilese details.
 */
public class SharedImageTilesCoordinator {

    // The maximum amount of tiles that can show, including icon tile and count tile.
    private static final int MAX_TILES_UI_LIMIT = 3;

    private final Context mContext;
    private final PropertyModel mModel;
    private final SharedImageTilesView mView;
    private final @SharedImageTilesType int mType;
    private final @NonNull DataSharingService mDataSharingService;
    private @NonNull String mCollaborationId;
    private int mAvailableMemberCount;
    private int mIconTilesCount;

    /**
     * Constructor for {@link SharedImageTilesCoordinator} component.
     *
     * @param context The Android context used to inflate the views.
     * @param type The {@link SharedImageTilesType} of the SharedImageTiles.
     * @param color The {@link SharedImageTilesColor} of the SharedImageTiles.
     * @param dataSharingService Used to fetch tab group data.
     */
    public SharedImageTilesCoordinator(
            Context context,
            @SharedImageTilesType int type,
            @SharedImageTilesColor int color,
            @NonNull DataSharingService dataSharingService) {
        mModel =
                new PropertyModel.Builder(SharedImageTilesProperties.ALL_KEYS)
                        .with(SharedImageTilesProperties.COLOR_THEME, color)
                        .build();
        mContext = context;
        mDataSharingService = dataSharingService;
        mType = type;

        mView =
                (SharedImageTilesView)
                        LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles, null);

        PropertyModelChangeProcessor.create(mModel, mView, SharedImageTilesViewBinder::bind);
        new SharedImageTilesMediator(mModel);
    }

    /** Cleans up any resources or observers this class used. */
    public void destroy() {}

    /**
     * Update the collaborationId for a SharedImageTiles component.
     *
     * @param collaborationId The new collaborationId or null to reset.
     */
    public void updateCollaborationId(@Nullable String collaborationId) {
        mCollaborationId = collaborationId;
        if (mCollaborationId == null) {
            updateMembersCount(0);
            return;
        }

        // Fetch group information from DataSharingService.
        mDataSharingService.readGroup(
                mCollaborationId,
                (result) -> {
                    if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN) {
                        // Error occurred. Remove all view.
                        updateMembersCount(0);
                        return;
                    }

                    assert result.groupData != null;
                    extractGroupMemberInfo(result.groupData);
                });
    }

    /**
     * Update the collaborationId for a SharedImageTiles component with a finished callback.
     *
     * @param collaborationId The new collaborationId or null to reset.
     * @param finishedCallback The callback to notify about the SharedImageTiles update status.
     */
    public void updateCollaborationId(
            @Nullable String collaborationId, Callback<Boolean> finishedCallback) {
        updateCollaborationId(collaborationId);
        // TODO(crbug.com/376329480): Implement face piles using bitmap and run finishedRunnable
        // only when all views are populated.
        finishedCallback.onResult(true);
    }

    private void extractGroupMemberInfo(GroupData groupData) {
        List<String> emails = new ArrayList<>();
        for (GroupMember member : groupData.members) {
            if (member.email != null && !member.email.isEmpty()) {
                emails.add(member.email);
            }
        }
        updateMembersCount(emails.size());

        // Let the UI delegate draw the icon tiles.
        DataSharingUIDelegate dataSharingUiDelegate = mDataSharingService.getUiDelegate();
        assert dataSharingUiDelegate != null;

        Callback<Boolean> successCallback =
                (success) -> {
                    if (!success) {
                        assert false;
                    }
                };

        List<ViewGroup> iconViews = getAllIconViews();
        int sizeInDp =
                (mType == SharedImageTilesType.SMALL)
                        ? R.dimen.small_shared_image_tiles_icon_height
                        : R.dimen.shared_image_tiles_icon_height;
        AvatarConfig config =
                new AvatarConfig.Builder()
                        .setAvatarSizeInPixels(getAvatarSizeInPixels(sizeInDp))
                        .build();

        dataSharingUiDelegate.showAvatars(
                mContext,
                iconViews,
                emails.subList(0, iconViews.size()),
                /* success= */ successCallback,
                config);
    }

    private int getAvatarSizeInPixels(int sizeInDp) {
        return mContext.getResources().getDimensionPixelSize(sizeInDp);
    }

    /** Populate the shared_image_tiles container with the specific icons. */
    private void initializeSharedImageTiles() {
        if (mAvailableMemberCount == 0) {
            return;
        }

        int maxTilesToShowWithNumberTile = MAX_TILES_UI_LIMIT - 1;
        boolean showNumberTile = mAvailableMemberCount > MAX_TILES_UI_LIMIT;
        mIconTilesCount =
                showNumberTile
                        ? MAX_TILES_UI_LIMIT - 1
                        : Math.min(mAvailableMemberCount, MAX_TILES_UI_LIMIT);

        // Add icon tile(s).
        mModel.set(SharedImageTilesProperties.ICON_TILES, mIconTilesCount);

        // Add number tile.
        if (showNumberTile) {
            // Compute a count bubble.
            mModel.set(
                    SharedImageTilesProperties.REMAINING_TILES,
                    mAvailableMemberCount - maxTilesToShowWithNumberTile);
        }

        // Re-style everything.
        mModel.set(SharedImageTilesProperties.TYPE, mType);
    }

    /** Get the view component of SharedImageTiles. */
    public @NonNull SharedImageTilesView getView() {
        return mView;
    }

    /** Get all icon views. */
    public List<ViewGroup> getAllIconViews() {
        assert (mView.getChildCount() >= mIconTilesCount);
        List<ViewGroup> list = new ArrayList<>();
        for (int i = 0; i < mIconTilesCount; i++) {
            ViewGroup view_group = (ViewGroup) mView.getChildAt(i);
            assert view_group.getChildCount() == 1;
            View view = view_group.getChildAt(0);
            list.add((ViewGroup) view);
        }
        return list;
    }

    /** Get the Android context used by the component. */
    public @NonNull Context getContext() {
        return mContext;
    }

    @VisibleForTesting
    void updateMembersCount(int count) {
        mAvailableMemberCount = count;
        mModel.set(SharedImageTilesProperties.REMAINING_TILES, 0);
        mModel.set(SharedImageTilesProperties.ICON_TILES, 0);
        initializeSharedImageTiles();
    }
}
