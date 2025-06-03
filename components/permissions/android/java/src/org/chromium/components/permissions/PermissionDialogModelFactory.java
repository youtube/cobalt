// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class creates the model for the permission dialog.
 */
class PermissionDialogModelFactory {
    public static PropertyModel getModel(
            ModalDialogProperties.Controller controller,
            PermissionDialogDelegate delegate,
            View customView,
            Runnable touchFilteredCallback) {
        Context context = delegate.getWindow().getContext().get();
        assert context != null;

        String messageText = delegate.getMessageText();
        assert !TextUtils.isEmpty(messageText);

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, controller)
                .with(ModalDialogProperties.FOCUS_DIALOG, true)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, delegate.getPrimaryButtonText())
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, delegate.getSecondaryButtonText())
                .with(ModalDialogProperties.CONTENT_DESCRIPTION, messageText)
                .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true)
                .with(ModalDialogProperties.TOUCH_FILTERED_CALLBACK, touchFilteredCallback)
                .with(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                        UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                .build();
    }
}
