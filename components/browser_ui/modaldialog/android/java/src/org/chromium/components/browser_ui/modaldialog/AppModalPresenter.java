// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static java.lang.Boolean.TRUE;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.view.Window;
import android.view.WindowManager;

import androidx.activity.ComponentDialog;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.OnApplyWindowInsetsListener;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.DialogStyles;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The presenter that shows a {@link ModalDialogView} in an Android dialog. */
public class AppModalPresenter extends ModalDialogManager.Presenter {
    // Duration of enter animation. This is an estimation because there is no reliable way to
    // get duration of AlertDialog's enter animation.
    private static final long ENTER_ANIMATION_ESTIMATION_MS = 200;
    private final Context mContext;
    private ComponentDialog mDialog;
    private ModalDialogView mDialogView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor<PropertyModel, ModalDialogView, PropertyKey>
            mModelChangeProcessor;

    private InsetObserver mInsetObserver;
    private OnApplyWindowInsetsListener mWindowInsetsListener;
    private ObservableSupplier<Boolean> mEdgeToEdgeStateSupplier;
    private Callback<Boolean> mEdgeToEdgeStateObserver;

    private int mFixedMargin;

    private class ViewBinder extends ModalDialogViewBinder {
        @Override
        public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
            if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
                mDialog.setCanceledOnTouchOutside(
                        model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
            } else if (ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER == propertyKey) {
                mDialog.getOnBackPressedDispatcher()
                        .addCallback(
                                model.get(
                                        ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER));
            } else {
                super.bind(model, view, propertyKey);
            }
        }
    }

    /**
     * @param context The {@link Context} associated with the dialog views.
     */
    public AppModalPresenter(Context context) {
        mContext = context;
    }

    private ModalDialogView loadDialogView() {
        return (ModalDialogView)
                LayoutInflaterUtils.inflate(mDialog.getContext(), R.layout.modal_dialog_view, null);
    }

    @Override
    protected void addDialogView(
            PropertyModel model, @Nullable Callback<ComponentDialog> onDialogCreatedCallback) {
        mModel = model;
        int[][] styles = {
            {
                R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton_Fullscreen,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton_DialogWhenLarge,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton_Fullscreen_Dark
            },
            {
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton_Fullscreen,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton_DialogWhenLarge,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton_Fullscreen_Dark
            },
            {
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton_Fullscreen,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton_DialogWhenLarge,
                R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledNegativeButton_Fullscreen_Dark
            }
        };
        int dialogIndex = 0;
        int dialogStyle = mModel.get(ModalDialogProperties.DIALOG_STYLES);

        if (dialogStyle == ModalDialogProperties.DialogStyles.FULLSCREEN_DIALOG) {
            dialogIndex = 1;
        } else if (dialogStyle == ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE) {
            dialogIndex = 2;
        } else if (dialogStyle == ModalDialogProperties.DialogStyles.FULLSCREEN_DARK_DIALOG) {
            dialogIndex = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ? 3 : 1;
        }
        int buttonIndex = 0;
        int buttonStyle = mModel.get(ModalDialogProperties.BUTTON_STYLES);
        if (buttonStyle == ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE
                || buttonStyle == ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE) {
            buttonIndex = 1;
        } else if (buttonStyle
                == ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED) {
            buttonIndex = 2;
        }
        mDialog = new ComponentDialog(mContext, styles[buttonIndex][dialogIndex]);
        mDialog.setOnCancelListener(
                dialogInterface -> {
                    dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
                });

        // Cancel on touch outside should be disabled by default. The ModelChangeProcessor wouldn't
        // notify change if the property is not set during initialization.
        mDialog.setCanceledOnTouchOutside(false);
        mDialogView = loadDialogView();

        // Observe application of dialog window insets, to calculate margins to avoid drawing the
        // dialog into the insets' regions. See crbug/365110749 for more details on why we use
        // |mInsetObserver|, and for tracking a more favorable long-term solution.
        if (ModalDialogFeatureMap.sModalDialogLayoutWithSystemInsets.isEnabled()) {
            mWindowInsetsListener =
                    (view, windowInsetsCompat) -> {
                        updateMargins();
                        return windowInsetsCompat;
                    };
            ViewCompat.setOnApplyWindowInsetsListener(
                    getWindow().getDecorView().getRootView(), mWindowInsetsListener);
        }

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mDialogView, new ViewBinder());
        // setContentView() can trigger using LayoutInflater, which may read from disk.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mDialog.setContentView(mDialogView);
        }

        mDialog.setOnShowListener(
                (ignored) -> {
                    if (mDialogView != null) {
                        mDialogView.onEnterAnimationStarted(ENTER_ANIMATION_ESTIMATION_MS);
                    }
                });

        if (onDialogCreatedCallback != null) {
            onDialogCreatedCallback.onResult(mDialog);
        }

        try {
            mDialog.show();
        } catch (WindowManager.BadTokenException badToken) {
            // See https://crbug.com/926688.
            dismissCurrentDialog(DialogDismissalCause.NOT_ATTACHED_TO_WINDOW);
        }
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }

        if (mDialog != null) {
            if (mWindowInsetsListener != null) {
                mWindowInsetsListener = null;
                ViewCompat.setOnApplyWindowInsetsListener(
                        getWindow().getDecorView().getRootView(), null);
            }
            mDialog.dismiss();
            mDialog = null;
            mDialogView = null;
            mModel = null;
        }

        if (mEdgeToEdgeStateSupplier != null) {
            mEdgeToEdgeStateSupplier.removeObserver(mEdgeToEdgeStateObserver);
        }
    }

    @Override
    protected void setInsetObserver(InsetObserver insetObserver) {
        mInsetObserver = insetObserver;
    }

    @Override
    protected void setEdgeToEdgeStateSupplier(ObservableSupplier<Boolean> edgeToEdgeStateSupplier) {
        if (!ModalDialogFeatureMap.sModalDialogLayoutWithSystemInsets.isEnabled()) return;
        mEdgeToEdgeStateSupplier = edgeToEdgeStateSupplier;
        mEdgeToEdgeStateObserver = isEdgeToEdgeActive -> updateMargins();
        mEdgeToEdgeStateSupplier.addObserver(mEdgeToEdgeStateObserver);
    }

    /**
     * Updates dialog margins to maintain a fixed distance from the app window's edges and to avoid
     * drawing into system insets' regions when edge-to-edge is active.
     */
    private void updateMargins() {
        if (mDialog == null || isFullScreenDialog(mContext, mModel)) return;

        // All modals should maintain a fixed distance from the app window's edges.
        if (mFixedMargin == 0) {
            // Extract the resource if not already extracted.
            mFixedMargin =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.modal_dialog_view_external_margin);
        }
        int horizontalMargin = mFixedMargin;
        int verticalMargin = mFixedMargin;

        // Recalculate the margins to account for system insets if applicable.
        if (mInsetObserver != null && isEdgeToEdgeActive()) {
            var windowInsets = mInsetObserver.getLastRawWindowInsets();
            if (windowInsets != null) {
                var systemInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
                horizontalMargin =
                        Math.max(Math.max(systemInsets.left, systemInsets.right), mFixedMargin);
                verticalMargin =
                        Math.max(Math.max(systemInsets.top, systemInsets.bottom), mFixedMargin);
            }
        }

        int currHorizontalMargin = mModel.get(ModalDialogProperties.HORIZONTAL_MARGIN);
        int currVerticalMargin = mModel.get(ModalDialogProperties.VERTICAL_MARGIN);

        // Margins for the current modal are already updated as needed.
        if (currHorizontalMargin == horizontalMargin && currVerticalMargin == verticalMargin) {
            return;
        }

        mModel.set(ModalDialogProperties.HORIZONTAL_MARGIN, horizontalMargin);
        mModel.set(ModalDialogProperties.VERTICAL_MARGIN, verticalMargin);

        // If the dialog is already showing when the insets are applied, request a layout for the
        // margins to take effect immediately.
        if (mDialog.isShowing()) {
            ViewUtils.requestLayout(mDialogView, "AppModalPresenter.updateMargins");
        }
    }

    private boolean isEdgeToEdgeActive() {
        return mEdgeToEdgeStateSupplier != null && TRUE.equals(mEdgeToEdgeStateSupplier.get());
    }

    private static boolean isFullScreenDialog(Context context, PropertyModel model) {
        assert model != null : "Model should not be null.";
        int dialogStyle = model.get(ModalDialogProperties.DIALOG_STYLES);

        int screenSize =
                context.getResources().getConfiguration().screenLayout
                        & Configuration.SCREENLAYOUT_SIZE_MASK;
        return (dialogStyle == DialogStyles.DIALOG_WHEN_LARGE
                        && screenSize < Configuration.SCREENLAYOUT_SIZE_LARGE)
                || dialogStyle == DialogStyles.FULLSCREEN_DIALOG
                || dialogStyle == DialogStyles.FULLSCREEN_DARK_DIALOG;
    }

    @VisibleForTesting
    public Window getWindow() {
        return mDialog.getWindow();
    }

    public ModalDialogView getDialogViewForTesting() {
        return mDialogView;
    }

    OnApplyWindowInsetsListener getWindowInsetsListenerForTesting() {
        return mWindowInsetsListener;
    }
}
