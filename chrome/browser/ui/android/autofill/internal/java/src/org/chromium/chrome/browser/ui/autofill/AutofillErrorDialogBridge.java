// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller that allows the native autofill code to show an error dialog.
 * For example: When unmasking a virtual card returns an error, we show an error dialog with more
 * information about the error.
 *
 * Note: The error dialog only shows a positive button which dismisses the dialog.
 */
@JNINamespace("autofill")
public class AutofillErrorDialogBridge {
    private final long mNativeAutofillErrorDialogView;
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private PropertyModel mDialogModel;

    private final ModalDialogProperties.Controller mModalDialogController =

            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    mModalDialogManager.dismissDialog(
                            mDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    AutofillErrorDialogBridgeJni.get().onDismissed(mNativeAutofillErrorDialogView);
                }
            };

    public AutofillErrorDialogBridge(long nativeAutofillErrorDialogView,
            ModalDialogManager modalDialogManager, Context context) {
        this.mNativeAutofillErrorDialogView = nativeAutofillErrorDialogView;
        this.mModalDialogManager = modalDialogManager;
        this.mContext = context;
    }

    @CalledByNative
    public static AutofillErrorDialogBridge create(
            long nativeAutofillErrorDialogView, WindowAndroid windowAndroid) {
        return new AutofillErrorDialogBridge(nativeAutofillErrorDialogView,
                windowAndroid.getModalDialogManager(), windowAndroid.getActivity().get());
    }

    /**
     * Shows an error dialog.
     *
     * @param title Title for the error dialog.
     * @param description Description for the error dialog which shows below the title.
     * @param buttonLabel Label for the positive button which acts as a cancel button.
     * @param titleIconId The resource id for the icon to be displayed to the left of the title.
     */
    @CalledByNative
    public void show(String title, String description, String buttonLabel, int titleIconId) {
        View errorDialogContentView =
                LayoutInflater.from(mContext).inflate(R.layout.autofill_error_dialog, null);
        ((TextView) errorDialogContentView.findViewById(R.id.error_message)).setText(description);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, errorDialogContentView)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, buttonLabel);
        if (titleIconId != 0) {
            builder.with(ModalDialogProperties.TITLE_ICON,
                    ResourcesCompat.getDrawable(
                            mContext.getResources(), titleIconId, mContext.getTheme()));
        }
        mDialogModel = builder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /**
     * Dismisses the currently showing dialog.
     */
    @CalledByNative
    public void dismiss() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeAutofillErrorDialogViewAndroid);
    }
}
