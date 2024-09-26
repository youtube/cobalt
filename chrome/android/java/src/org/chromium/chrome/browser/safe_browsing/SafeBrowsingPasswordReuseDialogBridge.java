// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogContents;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/** JNI call glue between the native and Java for password reuse dialogs. */
@JNINamespace("safe_browsing")
public class SafeBrowsingPasswordReuseDialogBridge {
    // The address of its C++ counterpart.
    private long mNativePasswordReuseDialogViewAndroid;
    // The coordinator for the password manager illustration modal dialog. Manages the sub-component
    // objects.
    private final PasswordManagerDialogCoordinator mDialogCoordinator;
    // Used to initialize the custom view of the dialog.
    private final WindowAndroid mWindowAndroid;

    private SafeBrowsingPasswordReuseDialogBridge(
            WindowAndroid windowAndroid, long nativePasswordReuseDialogViewAndroid) {
        mNativePasswordReuseDialogViewAndroid = nativePasswordReuseDialogViewAndroid;
        mWindowAndroid = windowAndroid;
        mDialogCoordinator =
                new PasswordManagerDialogCoordinator(mWindowAndroid.getModalDialogManager(),
                        mWindowAndroid.getActivity().get().findViewById(android.R.id.content),
                        BrowserControlsManagerSupplier.getValueOrNullFrom(mWindowAndroid));
    }

    @CalledByNative
    public static SafeBrowsingPasswordReuseDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new SafeBrowsingPasswordReuseDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String dialogTitle, String dialogDetails, String primaryButtonText,
            @Nullable String secondaryButtonText) {
        if (mWindowAndroid.getActivity().get() == null) return;

        PasswordManagerDialogContents contents = createDialogContents(
                dialogTitle, dialogDetails, primaryButtonText, secondaryButtonText);
        contents.setPrimaryButtonFilled(secondaryButtonText != null);

        mDialogCoordinator.initialize(mWindowAndroid.getActivity().get(), contents);
        mDialogCoordinator.showDialog();
    }

    private PasswordManagerDialogContents createDialogContents(String credentialLeakTitle,
            String credentialLeakDetails, String positiveButton, @Nullable String negativeButton) {
        Callback<Integer> onClick = negativeButton != null
                ? this::onClickWithNegativeButtonEnabled
                : this::onClickWithNegativeButtonDisabled;

        return new PasswordManagerDialogContents(credentialLeakTitle, credentialLeakDetails,
                R.drawable.password_checkup_warning, positiveButton, negativeButton, onClick);
    }

    @CalledByNative
    private void destroy() {
        mNativePasswordReuseDialogViewAndroid = 0;
        mDialogCoordinator.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onClickWithNegativeButtonDisabled(@DialogDismissalCause int dismissalCause) {
        // 0 indicates its C++ counterpart has already been destroyed.
        if (mNativePasswordReuseDialogViewAndroid == 0) return;

        SafeBrowsingPasswordReuseDialogBridgeJni.get().close(
                mNativePasswordReuseDialogViewAndroid, SafeBrowsingPasswordReuseDialogBridge.this);
    }

    private void onClickWithNegativeButtonEnabled(@DialogDismissalCause int dismissalCause) {
        // 0 indicates its C++ counterpart has already been destroyed.
        if (mNativePasswordReuseDialogViewAndroid == 0) return;

        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                SafeBrowsingPasswordReuseDialogBridgeJni.get().checkPasswords(
                        mNativePasswordReuseDialogViewAndroid,
                        SafeBrowsingPasswordReuseDialogBridge.this);
                return;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                SafeBrowsingPasswordReuseDialogBridgeJni.get().ignore(
                        mNativePasswordReuseDialogViewAndroid,
                        SafeBrowsingPasswordReuseDialogBridge.this);
                return;
            default:
                SafeBrowsingPasswordReuseDialogBridgeJni.get().close(
                        mNativePasswordReuseDialogViewAndroid,
                        SafeBrowsingPasswordReuseDialogBridge.this);
        }
    }

    @NativeMethods
    interface Natives {
        void checkPasswords(long nativePasswordReuseDialogViewAndroid,
                SafeBrowsingPasswordReuseDialogBridge caller);

        void ignore(long nativePasswordReuseDialogViewAndroid,
                SafeBrowsingPasswordReuseDialogBridge caller);

        void close(long nativePasswordReuseDialogViewAndroid,
                SafeBrowsingPasswordReuseDialogBridge caller);
    }
}
