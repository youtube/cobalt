// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java part of PasswordEditBridge pair providing communication between native password manager code
 * and Java password edit dialog UI components.
 */
public class PasswordEditDialogBridge implements PasswordEditDialogCoordinator.Delegate {
    private long mNativeDialog;
    private final PasswordEditDialogCoordinator mDialogCoordinator;

    @CalledByNative
    static PasswordEditDialogBridge create(
            long nativeDialog, @NonNull WindowAndroid windowAndroid) {
        return new PasswordEditDialogBridge(nativeDialog, windowAndroid);
    }

    private PasswordEditDialogBridge(long nativeDialog, @NonNull WindowAndroid windowAndroid) {
        mNativeDialog = nativeDialog;
        mDialogCoordinator = PasswordEditDialogCoordinator.create(windowAndroid, this);
    }

    /** Called when PasswordEditDialogWithDetails feature flag is enabled */
    @CalledByNative
    void showPasswordEditDialog(@NonNull String[] savedUsernames, @NonNull String username,
            @NonNull String password, @Nullable String account) {
        mDialogCoordinator.showPasswordEditDialog(savedUsernames, username, password, account);
    }

    /** Called when PasswordEditDialogWithDetails feature flag is disabled */
    @CalledByNative
    void showLegacyPasswordEditDialog(
            @NonNull String[] savedUsernames, int selectedUsernameIndex, @Nullable String account) {
        mDialogCoordinator.showLegacyPasswordEditDialog(
                savedUsernames, selectedUsernameIndex, account);
    }

    @CalledByNative
    void dismiss() {
        mDialogCoordinator.dismiss();
    }

    @Override
    public void onDialogAccepted(String username, String password) {
        assert mNativeDialog != 0;
        PasswordEditDialogBridgeJni.get().onDialogAccepted(mNativeDialog, username, password);
    }

    @Override
    public void onLegacyDialogAccepted(int usernameIndex) {
        assert mNativeDialog != 0;
        PasswordEditDialogBridgeJni.get().onLegacyDialogAccepted(mNativeDialog, usernameIndex);
    }

    @Override
    public void onDialogDismissed(boolean dialogAccepted) {
        assert mNativeDialog != 0;
        PasswordEditDialogBridgeJni.get().onDialogDismissed(mNativeDialog, dialogAccepted);
        mNativeDialog = 0;
    }

    @NativeMethods
    interface Natives {
        void onDialogAccepted(
                long nativePasswordEditDialogBridge, String username, String password);
        void onLegacyDialogAccepted(long nativePasswordEditDialogBridge, int usernameIndex);
        void onDialogDismissed(long nativePasswordEditDialogBridge, boolean dialogAccepted);
    }
}