// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.DialogInterface;
import android.os.Bundle;
import android.widget.Button;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.widget.PromoDialog;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;

/** A dialog that forces the user to choose a default search engine. */
public class DefaultSearchEnginePromoDialog extends PromoDialog {
    /** Notified about events happening to the dialog. */
    public static interface DefaultSearchEnginePromoDialogObserver {
        void onDialogShown(DefaultSearchEnginePromoDialog shownDialog);
    }
    private static DefaultSearchEnginePromoDialogObserver sObserver;

    @SuppressLint("StaticFieldLeak")
    private static DefaultSearchEnginePromoDialog sCurrentDialog;

    /** Used to determine the promo dialog contents. */
    @SearchEnginePromoType
    private final int mDialogType;

    /** Called when the dialog is dismissed after the user has chosen a search engine. */
    private final Callback<Boolean> mOnSuccessCallback;

    private DefaultSearchEngineDialogHelper.Delegate mDelegate;

    /** Encapsulates most of the logic for filling the dialog and handling clicks. */
    private DefaultSearchEngineDialogHelper mHelper;

    /**
     * Construct the default search engine promo.
     *
     * @param activity    Activity to build the dialog with.
     * @param delegate    Delegate for getting search engine list/selection notification.
     * @param dialogType  Type of dialog to show.
     * @param onSuccessCallback Notified whether the user successfully chose a search engine and
     *                          dismissed the dialog.
     */
    public DefaultSearchEnginePromoDialog(Activity activity,
            DefaultSearchEngineDialogHelper.Delegate delegate, int dialogType,
            @Nullable Callback<Boolean> onSuccessCallback) {
        super(activity);
        mDelegate = delegate;
        mDialogType = dialogType;
        mOnSuccessCallback = onSuccessCallback;
        setOnDismissListener(this);

        // No one should be able to bypass this dialog by clicking outside or by hitting back.
        setCancelable(false);
        setCanceledOnTouchOutside(false);

        if (dialogType == SearchEnginePromoType.SHOW_NEW) forceOpaqueBackground();
    }

    @Override
    protected DialogParams getDialogParams() {
        PromoDialog.DialogParams params = new PromoDialog.DialogParams();
        params.headerStringResource = R.string.search_engine_dialog_title;
        params.footerStringResource = R.string.search_engine_dialog_footer;
        params.primaryButtonStringResource = R.string.ok;
        return params;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Button okButton = (Button) findViewById(R.id.button_primary);
        okButton.setEnabled(false);

        RadioButtonLayout radioButtons = new RadioButtonLayout(getContext());
        radioButtons.setId(R.id.default_search_engine_dialog_options);
        addControl(radioButtons);
        mHelper = new DefaultSearchEngineDialogHelper(
                mDialogType, mDelegate, radioButtons, okButton, this::dismiss);
    }

    @Override
    public void show() {
        super.show();
        if (sCurrentDialog != null) sCurrentDialog.dismiss();
        setCurrentDialog(this);

        if (mDialogType == SearchEnginePromoType.SHOW_NEW) {
            RecordUserAction.record("SearchEnginePromo.NewDevice.Shown.Dialog");
        } else if (mDialogType == SearchEnginePromoType.SHOW_EXISTING) {
            RecordUserAction.record("SearchEnginePromo.ExistingDevice.Shown.Dialog");
        }
        if (sObserver != null) sObserver.onDialogShown(this);
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        if (mHelper.getConfirmedKeyword() == null) {
            // If no selection, finish the Activity so that the user has to respond to the dialog
            // next time.
            if (getOwnerActivity() != null) getOwnerActivity().finish();
        }

        if (mOnSuccessCallback != null) {
            mOnSuccessCallback.onResult(mHelper.getConfirmedKeyword() != null);
        }

        if (sCurrentDialog == this) setCurrentDialog(null);
    }

    /** See {@link #sObserver}. */
    @VisibleForTesting
    public static void setObserverForTests(DefaultSearchEnginePromoDialogObserver observer) {
        sObserver = observer;
    }

    /** See {@link #sObserver}. */
    @VisibleForTesting
    public static void setObserverForTests2(DefaultSearchEnginePromoDialogObserver observer) {
        sObserver = observer;
    }

    /** @return The current visible Default Search Engine dialog. */
    static DefaultSearchEnginePromoDialog getCurrentDialog() {
        return sCurrentDialog;
    }

    private static void setCurrentDialog(DefaultSearchEnginePromoDialog dialog) {
        sCurrentDialog = dialog;
    }
}
