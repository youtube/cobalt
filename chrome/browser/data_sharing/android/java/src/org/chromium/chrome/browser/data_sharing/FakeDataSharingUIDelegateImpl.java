// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingRuntimeDataConfig;
import org.chromium.url.GURL;

/** Fake implementation of {@link DataSharingUIDelegate} */
@NullMarked
public class FakeDataSharingUIDelegateImpl implements DataSharingUIDelegate {

    private @Nullable Callback<Boolean> mShowCreateFlowCallback;
    private @Nullable Callback<Boolean> mShowJoinFlowCallback;
    private @Nullable Callback<Boolean> mShowManageFlowCallback;
    private @Nullable DataSharingCreateUiConfig mCreateUiConfig;
    private @Nullable DataSharingJoinUiConfig mJoinUiConfig;
    private @Nullable DataSharingManageUiConfig mManageUiConfig;

    public FakeDataSharingUIDelegateImpl() {}

    @Override
    public void getAvatarBitmap(DataSharingAvatarBitmapConfig avatarBitmapConfig) {}

    @Override
    public void handleShareURLIntercepted(GURL url) {}

    @Override
    public String showCreateFlow(DataSharingCreateUiConfig createUiConfig) {
        assert this.mCreateUiConfig == null;
        this.mCreateUiConfig = createUiConfig;
        if (mShowCreateFlowCallback != null) {
            mShowCreateFlowCallback.onResult(true);
        }
        return "";
    }

    @Override
    public String showJoinFlow(DataSharingJoinUiConfig joinUiConfig) {
        assert this.mJoinUiConfig == null;
        this.mJoinUiConfig = joinUiConfig;
        if (mShowJoinFlowCallback != null) {
            mShowJoinFlowCallback.onResult(true);
        }
        return "";
    }

    @Override
    public String showManageFlow(DataSharingManageUiConfig manageUiConfig) {
        assert this.mManageUiConfig == null;
        this.mManageUiConfig = manageUiConfig;
        if (mShowManageFlowCallback != null) {
            mShowManageFlowCallback.onResult(true);
        }
        return "";
    }

    @Override
    public void updateRuntimeData(
            @Nullable String sessionId, DataSharingRuntimeDataConfig runtimeData) {}

    @Override
    public void destroyFlow(String sessionId) {}

    /* Set a callback to be called when showCreateFlow() is called. */
    public void setShowCreateFlowCallback(Callback<Boolean> callback) {
        mShowCreateFlowCallback = callback;
    }

    /* Set a callback to be called when showJoinFlow() is called. */
    public void setShowJoinFlowCallback(Callback<Boolean> callback) {
        mShowJoinFlowCallback = callback;
    }

    /* Set a callback to be called when showManageFlow() is called. */
    public void setShowManageFlowCallback(Callback<Boolean> callback) {
        mShowManageFlowCallback = callback;
    }

    /* Creates group data and calls onGroupCreatedWithWait when showCreateFlow() is called. */
    public void forceGroupCreation(String collaborationId, String accessToken) {
        org.chromium.components.sync.protocol.GroupData groupData =
                org.chromium.components.sync.protocol.GroupData.newBuilder()
                        .setGroupId(collaborationId)
                        .setAccessToken(accessToken)
                        .build();
        if (mCreateUiConfig == null || mCreateUiConfig.getCreateCallback() == null) return;
        mCreateUiConfig
                .getCreateCallback()
                .onGroupCreatedWithWait(
                        groupData,
                        (success) -> {
                            assert success;
                        });
    }

    /* Calls onCancelClicked when showCreateFlow() is called. */
    public void forceCreateFlowCancellation() {
        if (mCreateUiConfig == null || mCreateUiConfig.getCreateCallback() == null) return;
        mCreateUiConfig.getCreateCallback().onCancelClicked();
    }
}
