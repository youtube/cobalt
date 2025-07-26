// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingRuntimeDataConfig;
import org.chromium.url.GURL;

/** An interface that shows sharing UI screens. */
@JNINamespace("data_sharing")
public interface DataSharingUIDelegate {

    /**
     * Handle the intercepted URL to show relevant data sharing group information.
     *
     * @param url The URL of the current share action.
     */
    @CalledByNative
    public void handleShareURLIntercepted(GURL url);

    /**
     * Method to show create flow.
     *
     * @param createUiConfig Used to set properties for data sharing create flow.
     * @return A unique identifier for create flow.
     */
    default String showCreateFlow(DataSharingCreateUiConfig createUiConfig) {
        return null;
    }

    /**
     * Method to show join flow.
     *
     * @param joinUiConfig Used to set properties for data sharing join flow.
     * @return A unique identifier for join flow.
     */
    default String showJoinFlow(DataSharingJoinUiConfig joinUiConfig) {
        return null;
    }

    /**
     * Method to show manage flow.
     *
     * @param manageUiConfig Used to set properties for data sharing manage flow.
     * @return A unique identifier for manage flow.
     */
    default String showManageFlow(DataSharingManageUiConfig manageUiConfig) {
        return null;
    }

    /**
     * Update the runtime data needed for the flows after they are started.
     *
     * <p>If the flow is already stopped, then noop.
     *
     * @param sessionId The session ID returned by the showFlow() calls.
     * @param runtimeData The runtime data to update the flow with.
     */
    default void updateRuntimeData(String sessionId, DataSharingRuntimeDataConfig runtimeData) {}

    /**
     * Method to destroy UI flow based on `sessionId`.
     *
     * @param sessionId Used to identify the flow to be destroyed.
     */
    default void destroyFlow(String sessionId) {}

    /**
     * Method to get bitmap for an avatar.
     *
     * @param avatarBitmapConfig Used to set properties for getting bitmap for an avatar.
     */
    default void getAvatarBitmap(DataSharingAvatarBitmapConfig avatarBitmapConfig) {}
}
