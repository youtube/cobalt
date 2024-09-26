// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Controller to manage when an IPH bubble for PageInfo is shown.
 */
public class PageInfoIPHController {
    private final UserEducationHelper mUserEducationHelper;
    private final View mStatusView;

    /**
     * Constructor
     * @param activity The activity.
     * @param statusView The status view in the omnibox. Used as anchor for IPH bubble.
     */
    public PageInfoIPHController(Activity activity, View statusView) {
        mUserEducationHelper =
                new UserEducationHelper(activity, new Handler(Looper.getMainLooper()));
        mStatusView = statusView;
    }

    /**
     * Called when a permission prompt was shown.
     * @param iphTimeout The timeout after which the IPH bubble should disappear if it was shown.
     */
    public void onPermissionDialogShown(int iphTimeout) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        tracker.notifyEvent(EventConstants.PERMISSION_REQUEST_SHOWN);

        mUserEducationHelper.requestShowIPH(new IPHCommandBuilder(
                mStatusView.getContext().getResources(), FeatureConstants.PAGE_INFO_FEATURE,
                R.string.page_info_iph, R.string.page_info_iph)
                                                    .setAutoDismissTimeout(iphTimeout)
                                                    .setAnchorView(mStatusView)
                                                    .build());
    }

    /**
     * Show the IPH for store icon in omnibox.
     * @param iphTimeout The timeout after which the IPH bubble should disappear if it was shown.
     * @param stringId Resource id of the string displayed. The string will also be used for
     *         accessibility.
     */
    public void showStoreIconIPH(int iphTimeout, @StringRes int stringId) {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mStatusView.getContext().getResources(),
                        FeatureConstants.PAGE_INFO_STORE_INFO_FEATURE, stringId, stringId)
                        .setAutoDismissTimeout(iphTimeout)
                        .setAnchorView(mStatusView)
                        .setDismissOnTouch(true)
                        .build());
    }
}
