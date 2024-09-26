// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import java.util.List;

/**
 * Class for controlling the page info ad personalization section.
 */
public class PageInfoAdPersonalizationController extends PageInfoPreferenceSubpageController {
    public static final int ROW_ID = View.generateViewId();
    private static List<String> sTopicsForTesting;

    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private PageInfoAdPersonalizationPreference mSubPage;

    private boolean mHasJoinedUserToInterestGroup;
    private List<String> mTopics;

    public PageInfoAdPersonalizationController(PageInfoMainController mainController,
            PageInfoRowView rowView, PageInfoControllerDelegate delegate) {
        super(delegate);
        mMainController = mainController;
        mRowView = rowView;
    }

    public void setAdPersonalizationInfo(
            boolean hasJoinedUserToInterestGroup, List<String> topics) {
        mHasJoinedUserToInterestGroup = hasJoinedUserToInterestGroup;
        mTopics = topics;
        if (mTopics.isEmpty() && sTopicsForTesting != null) {
            mTopics = sTopicsForTesting;
        }
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = hasJoinedUserToInterestGroup || !mTopics.isEmpty();
        rowParams.title = getSubpageTitle();
        rowParams.iconResId = R.drawable.gm_ads_click_24;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }

    private void launchSubpage() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_AD_PERSONALIZATION_PAGE_OPENED);
        mMainController.launchSubpage(this);
    }

    @NonNull
    @Override
    public String getSubpageTitle() {
        var siteSettingsDelegate = getDelegate().getSiteSettingsDelegate();
        return mRowView.getContext().getResources().getString(
                siteSettingsDelegate.isPrivacySandboxSettings4Enabled()
                        ? R.string.page_info_ad_privacy_header
                        : R.string.page_info_ad_personalization_title);
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        assert mSubPage == null;
        mSubPage = new PageInfoAdPersonalizationPreference();
        PageInfoAdPersonalizationPreference.Params params =
                new PageInfoAdPersonalizationPreference.Params();
        params.hasJoinedUserToInterestGroup = mHasJoinedUserToInterestGroup;
        params.topicInfo = mTopics;
        params.onManageInterestsButtonClicked = () -> {
            mMainController.recordAction(
                    PageInfoAction.PAGE_INFO_AD_PERSONALIZATION_SETTINGS_OPENED);
            getDelegate().showAdPersonalizationSettings();
        };
        mSubPage.setParams(params);
        return addSubpageFragment(mSubPage);
    }

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}

    @Override
    public void onSubpageRemoved() {
        removeSubpageFragment();
        mSubPage = null;
    }

    @VisibleForTesting
    public static void setTopicsForTesting(List<String> topics) {
        sTopicsForTesting = topics;
    }
}
