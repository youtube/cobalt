// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import android.text.TextUtils;
import android.view.View;

import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for binding view properties from {@link ModalDialogProperties} to a
 * {@link ModalDialogView}.
 */
public class ModalDialogViewBinder
        implements PropertyModelChangeProcessor
                           .ViewBinder<PropertyModel, ModalDialogView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
        if (ModalDialogProperties.TITLE == propertyKey) {
            view.setTitle(model.get(ModalDialogProperties.TITLE));
        } else if (ModalDialogProperties.TITLE_MAX_LINES == propertyKey) {
            view.setTitleMaxLines(model.get(ModalDialogProperties.TITLE_MAX_LINES));
        } else if (ModalDialogProperties.TITLE_ICON == propertyKey) {
            view.setTitleIcon(model.get(ModalDialogProperties.TITLE_ICON));
        } else if (ModalDialogProperties.MESSAGE_PARAGRAPH_1 == propertyKey) {
            view.setMessageParagraph1(model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
        } else if (ModalDialogProperties.MESSAGE_PARAGRAPH_2 == propertyKey) {
            view.setMessageParagraph2(model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_2));
        } else if (ModalDialogProperties.CUSTOM_VIEW == propertyKey) {
            view.setCustomView(model.get(ModalDialogProperties.CUSTOM_VIEW));
        } else if (ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW == propertyKey) {
            view.setCustomButtonBar(model.get(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_TEXT == propertyKey) {
            assert checkFilterTouchConsistency(model);
            view.setButtonText(ModalDialogProperties.ButtonType.POSITIVE,
                    model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            view.setButtonContentDescription(ModalDialogProperties.ButtonType.POSITIVE,
                    model.get(ModalDialogProperties.POSITIVE_BUTTON_CONTENT_DESCRIPTION));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_DISABLED == propertyKey) {
            view.setButtonEnabled(ModalDialogProperties.ButtonType.POSITIVE,
                    !model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_TEXT == propertyKey) {
            assert checkFilterTouchConsistency(model);
            assert checkFilledButtonConsistency(model);
            view.setButtonText(ModalDialogProperties.ButtonType.NEGATIVE,
                    model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            view.setButtonContentDescription(ModalDialogProperties.ButtonType.NEGATIVE,
                    model.get(ModalDialogProperties.NEGATIVE_BUTTON_CONTENT_DESCRIPTION));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_DISABLED == propertyKey) {
            view.setButtonEnabled(ModalDialogProperties.ButtonType.NEGATIVE,
                    !model.get(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED));
        } else if (ModalDialogProperties.FOOTER_MESSAGE == propertyKey) {
            view.setFooterMessage(model.get(ModalDialogProperties.FOOTER_MESSAGE));
        } else if (ModalDialogProperties.TITLE_SCROLLABLE == propertyKey) {
            view.setTitleScrollable(model.get(ModalDialogProperties.TITLE_SCROLLABLE));
        } else if (ModalDialogProperties.CONTROLLER == propertyKey) {
            view.setOnButtonClickedCallback((buttonType) -> {
                model.get(ModalDialogProperties.CONTROLLER).onClick(model, buttonType);
            });
        } else if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
            // Intentionally left empty since this is a property for the dialog container.
        } else if (ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY == propertyKey) {
            assert checkFilterTouchConsistency(model);
            view.setFilterTouchForSecurity(
                    model.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY));
        } else if (ModalDialogProperties.TOUCH_FILTERED_CALLBACK == propertyKey) {
            view.setOnTouchFilteredCallback(
                    model.get(ModalDialogProperties.TOUCH_FILTERED_CALLBACK));
        } else if (ModalDialogProperties.CONTENT_DESCRIPTION == propertyKey) {
            // Intentionally left empty since this is a property used for the dialog container.
        } else if (ModalDialogProperties.BUTTON_STYLES == propertyKey) {
            assert checkFilledButtonConsistency(model);
            assert checkCustomButtonsConsistency(model);
            // Intentionally left empty since this is only read once before the dialog is inflated.
        } else if (ModalDialogProperties.FULLSCREEN_DIALOG == propertyKey
                || ModalDialogProperties.DIALOG_WHEN_LARGE == propertyKey
                || ModalDialogProperties.EXCEED_MAX_HEIGHT == propertyKey) {
            boolean ignoreWidthConstraints = model.get(ModalDialogProperties.FULLSCREEN_DIALOG)
                    || model.get(ModalDialogProperties.DIALOG_WHEN_LARGE);
            boolean ignoreHeightConstraint = model.get(ModalDialogProperties.EXCEED_MAX_HEIGHT);
            view.setIgnoreConstraints(ignoreWidthConstraints, ignoreHeightConstraint);
            assert !(model.get(ModalDialogProperties.FULLSCREEN_DIALOG)
                    && model.get(ModalDialogProperties.DIALOG_WHEN_LARGE))
                : "Both FULLSCREEN_DIALOG and DIALOG_WHEN_LARGE cannot be set to true.";
        } else if (ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS == propertyKey) {
            view.setButtonTapProtectionDurationMs(
                    model.get(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS));
        } else if (ModalDialogProperties.FOCUS_DIALOG == propertyKey) {
            // Intentionally left empty since this is a property for the dialog container.
        } else {
            assert false : "Unhandled property detected in ModalDialogViewBinder!";
        }
    }

    /**
     * Checks if FILTER_TOUCH_FOR_SECURITY flag is consistent with the set of enabled buttons.
     * Touch event filtering in ModalDialogView is only applied to standard buttons. When buttons
     * are hidden, filtering touch events doesn't have effect.
     * @return false if security sensitive dialog doesn't have standard buttons.
     */
    private static boolean checkFilterTouchConsistency(PropertyModel model) {
        return !model.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY)
                || !TextUtils.isEmpty(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT))
                || !TextUtils.isEmpty(model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    /**
     * Checks if the BUTTON_STYLES property is consistent with the set of enabled buttons.
     * If the primary button (== positive button for dialog view) is in filled state,  it could be
     * disabled while the negative button is enabled. On the contrary, if the negative button is
     * filled, it could also be disabled while the primary button is enabled.
     * @return false if one button is in filled state while the other button doesn't present. True
     *         otherwise.
     */
    private static boolean checkFilledButtonConsistency(PropertyModel model) {
        int styles = model.get(ModalDialogProperties.BUTTON_STYLES);
        if (styles == ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE) {
            return !TextUtils.isEmpty(model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        } else if (styles == ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED) {
            return !TextUtils.isEmpty(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        }

        return true;
    }

    /**
     * Checks that BUTTON_STYLES isn't present together with CUSTOM_BUTTON_BAR_VIEW because the
     * custom button bar overrides the default positive and negative buttons..
     */
    private static boolean checkCustomButtonsConsistency(PropertyModel model) {
        int styles = model.get(ModalDialogProperties.BUTTON_STYLES);
        View customButtons = model.get(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW);
        return styles == 0 || customButtons == null;
    }
}
