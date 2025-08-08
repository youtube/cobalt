// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;

import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/**
 * The Mediator for the PWA Restore bottom sheet.
 */
class PwaRestoreBottomSheetMediator {
    // The current activity.
    private final Activity mActivity;

    // The underlying property model for the bottom sheeet.
    private final PropertyModel mModel;

    PwaRestoreBottomSheetMediator(
            Activity activity, Runnable onReviewButtonClicked, Runnable onBackButtonClicked) {
        mActivity = activity;
        mModel = PwaRestoreProperties.createModel(onReviewButtonClicked, onBackButtonClicked,
                this::onDeselectButtonClicked, this::onRestoreButtonClicked);

        initializeState();
        setPeekingState();
    }

    private void initializeState() {
        mModel.set(PwaRestoreProperties.PEEK_TITLE,
                mActivity.getString(R.string.pwa_restore_title_peeking));
        mModel.set(PwaRestoreProperties.PEEK_DESCRIPTION,
                mActivity.getString(R.string.pwa_restore_description_peeking));
        mModel.set(PwaRestoreProperties.PEEK_BUTTON_LABEL,
                mActivity.getString(R.string.pwa_restore_button_peeking));

        mModel.set(PwaRestoreProperties.EXPANDED_TITLE,
                mActivity.getString(R.string.pwa_restore_title_expanded));
        mModel.set(PwaRestoreProperties.EXPANDED_DESCRIPTION,
                mActivity.getString(R.string.pwa_restore_description_expanded));
        mModel.set(
                PwaRestoreProperties.RECENT_APPS_TITLE,
                mActivity.getString(R.string.pwa_restore_recent_apps_list));
        mModel.set(
                PwaRestoreProperties.OLDER_APPS_TITLE,
                mActivity.getString(R.string.pwa_restore_older_apps_list));
        mModel.set(PwaRestoreProperties.EXPANDED_BUTTON_LABEL,
                mActivity.getString(R.string.pwa_restore_button_expanded));
        mModel.set(PwaRestoreProperties.DESELECT_BUTTON_LABEL,
                mActivity.getString(R.string.pwa_restore_button_deselect));

        // TODO(finnur): Replace with actual apps, queried from profile.
        ArrayList apps = new ArrayList();
        apps.add(new PwaRestoreProperties.AppInfo("foo", "Bar"));
        apps.add(new PwaRestoreProperties.AppInfo("bar", "Foo"));
        apps.add(new PwaRestoreProperties.AppInfo("foobar", "Barfoo"));
        mModel.set(PwaRestoreProperties.APPS, apps);
    }

    protected void setPeekingState() {
        mModel.set(PwaRestoreProperties.VIEW_STATE, ViewState.PREVIEW);
    }

    protected void setPreviewState() {
        mModel.set(PwaRestoreProperties.VIEW_STATE, ViewState.VIEW_PWA_LIST);
    }

    private void onDeselectButtonClicked() {
        // TODO(finnur): Implement.
    }

    private void onRestoreButtonClicked() {
        // TODO(finnur): Implement.
    }

    PropertyModel getModel() {
        return mModel;
    }
}
