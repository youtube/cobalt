// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * StatusViewBinder observes StatusModel changes and triggers StatusView updates.
 */
class StatusViewBinder implements ViewBinder<PropertyModel, StatusView, PropertyKey> {
    StatusViewBinder() {}

    @Override
    public void bind(PropertyModel model, StatusView view, PropertyKey propertyKey) {
        if (StatusProperties.ALPHA.equals(propertyKey)) {
            view.setAlpha(model.get(StatusProperties.ALPHA));
        } else if (StatusProperties.ANIMATIONS_ENABLED.equals(propertyKey)) {
            view.setAnimationsEnabled(model.get(StatusProperties.ANIMATIONS_ENABLED));
        } else if (StatusProperties.INCOGNITO_BADGE_VISIBLE.equals(propertyKey)) {
            view.setIncognitoBadgeVisibility(model.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        } else if (StatusProperties.SEPARATOR_COLOR.equals(propertyKey)) {
            view.setSeparatorColor(model.get(StatusProperties.SEPARATOR_COLOR));
        } else if (StatusProperties.SHOW_STATUS_ICON.equals(propertyKey)) {
            view.setStatusIconShown(model.get(StatusProperties.SHOW_STATUS_ICON));
        } else if (StatusProperties.SHOW_STATUS_ICON_BACKGROUND.equals(propertyKey)) {
            view.setStatusIconBackgroundVisibility(
                    model.get(StatusProperties.SHOW_STATUS_ICON_BACKGROUND));
        } else if (StatusProperties.STATUS_CLICK_LISTENER.equals(propertyKey)) {
            view.setStatusClickListener(model.get(StatusProperties.STATUS_CLICK_LISTENER));
        } else if (StatusProperties.STATUS_ACCESSIBILITY_TOAST_RES.equals(propertyKey)) {
            view.setStatusAccessibilityToast(
                    model.get(StatusProperties.STATUS_ACCESSIBILITY_TOAST_RES));
        } else if (StatusProperties.STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES.equals(
                           propertyKey)) {
            view.setStatusAccessibilityDoubleTapDescription(
                    model.get(StatusProperties.STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES));
        } else if (StatusProperties.STATUS_ICON_ALPHA.equals(propertyKey)) {
            view.setStatusIconAlpha(model.get(StatusProperties.STATUS_ICON_ALPHA));
        } else if (StatusProperties.STATUS_ICON_DESCRIPTION_RES.equals(propertyKey)) {
            view.setStatusIconDescription(model.get(StatusProperties.STATUS_ICON_DESCRIPTION_RES));
        } else if (StatusProperties.STATUS_ICON_RESOURCE.equals(propertyKey)) {
            StatusIconResource res = model.get(StatusProperties.STATUS_ICON_RESOURCE);
            if (res == null) {
                view.setStatusIconResources(null, StatusView.IconTransitionType.CROSSFADE, null);
                return;
            }
            view.setStatusIconResources(res.getDrawable(view.getContext(), view.getResources()),
                    res.getTransitionType(), res.getAnimationFinishedCallback());
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_COLOR.equals(propertyKey)) {
            view.setVerboseStatusTextColor(model.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES.equals(propertyKey)) {
            view.setVerboseStatusTextContent(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE.equals(propertyKey)) {
            view.setVerboseStatusTextVisible(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_WIDTH.equals(propertyKey)) {
            view.setVerboseStatusTextWidth(model.get(StatusProperties.VERBOSE_STATUS_TEXT_WIDTH));
        } else {
            assert false : "Unhandled property update";
        }
    }
}
