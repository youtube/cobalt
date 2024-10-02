// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayoutMediator;

import org.chromium.chrome.browser.privacy_guide.PrivacyGuideUtils.CustomTabIntentHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Fragment containing the Privacy Guide (a walk-through of the most important privacy settings).
 */
public class PrivacyGuideFragment extends Fragment {
    /**
     * The types of fragments supported. Each fragment corresponds to a step in the privacy guide.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({FragmentType.WELCOME, FragmentType.MSBB, FragmentType.HISTORY_SYNC,
            FragmentType.SAFE_BROWSING, FragmentType.COOKIES, FragmentType.DONE})
    @interface FragmentType {
        int WELCOME = 0;
        int MSBB = 1;
        int HISTORY_SYNC = 2;
        int SAFE_BROWSING = 3;
        int COOKIES = 4;
        int DONE = 5;
        int MAX_VALUE = DONE;
    }

    private BottomSheetController mBottomSheetController;
    private CustomTabIntentHelper mCustomTabHelper;
    private SettingsLauncher mSettingsLauncher;
    private PrivacyGuidePagerAdapter mPagerAdapter;
    private View mView;
    private ViewPager2 mViewPager;
    private TabLayout mTabLayout;
    private ButtonCompat mStartButton;
    private ButtonCompat mNextButton;
    private ButtonCompat mBackButton;
    private ButtonCompat mFinishButton;
    private ButtonCompat mDoneButton;
    private PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegate;
    private NavbarVisibilityDelegate mNavbarVisibilityDelegate;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
        mPrivacyGuideMetricsDelegate = new PrivacyGuideMetricsDelegate();
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        modifyAppBar();
        mView = inflater.inflate(R.layout.privacy_guide_steps, container, false);

        mViewPager = (ViewPager2) mView.findViewById(R.id.review_viewpager);
        mPagerAdapter = new PrivacyGuidePagerAdapter(this, new StepDisplayHandlerImpl());
        mNavbarVisibilityDelegate = new NavbarVisibilityDelegate(mPagerAdapter.getItemCount());
        mViewPager.setAdapter(mPagerAdapter);
        mViewPager.setUserInputEnabled(false);

        mTabLayout = mView.findViewById(R.id.tab_layout);
        new TabLayoutMediator(mTabLayout, mViewPager, (tab, position) -> {
            tab.view.setClickable(false);
            tab.view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            if (position == 0 || position == mPagerAdapter.getItemCount() - 1) {
                tab.view.setVisibility(View.GONE);
            }
        }).attach();

        mStartButton = (ButtonCompat) mView.findViewById(R.id.start_button);
        mStartButton.setOnClickListener((View v) -> nextStep());

        mNextButton = (ButtonCompat) mView.findViewById(R.id.next_button);
        mNextButton.setOnClickListener((View v) -> nextStep());

        mBackButton = (ButtonCompat) mView.findViewById(R.id.back_button);
        mBackButton.setOnClickListener((View v) -> previousStep());

        mFinishButton = (ButtonCompat) mView.findViewById(R.id.finish_button);
        mFinishButton.setOnClickListener((View v) -> nextStep());

        mDoneButton = (ButtonCompat) mView.findViewById(R.id.done_button);
        mDoneButton.setOnClickListener((View v) -> {
            PrivacyGuideMetricsDelegate.recordMetricsForDoneButton();
            getActivity().onBackPressed();
        });

        return mView;
    }

    @Override
    public void onResume() {
        super.onResume();
        updateButtonVisibility();
    }

    private void modifyAppBar() {
        AppCompatActivity settingsActivity = (AppCompatActivity) getActivity();
        settingsActivity.setTitle(R.string.privacy_guide_fragment_title);
        settingsActivity.getSupportActionBar().setDisplayHomeAsUpEnabled(false);
    }

    private void nextStep() {
        int currentIdx = mViewPager.getCurrentItem();
        int nextIdx = currentIdx + 1;

        mViewPager.setCurrentItem(nextIdx);
        updateButtonVisibility();
        recordMetricsOnButtonPress(currentIdx, nextIdx);
    }

    private void previousStep() {
        int currentIdx = mViewPager.getCurrentItem();
        int prevIdx = currentIdx - 1;

        mViewPager.setCurrentItem(prevIdx);
        updateButtonVisibility();
        recordMetricsOnButtonPress(currentIdx, prevIdx);
    }

    private void updateButtonVisibility() {
        int currentIdx = mViewPager.getCurrentItem();

        mStartButton.setVisibility(mNavbarVisibilityDelegate.getStartButtonVisibility(currentIdx));
        mNextButton.setVisibility(mNavbarVisibilityDelegate.getNextButtonVisibility(currentIdx));
        mBackButton.setVisibility(mNavbarVisibilityDelegate.getBackButtonVisibility(currentIdx));
        mFinishButton.setVisibility(
                mNavbarVisibilityDelegate.getFinishButtonVisibility(currentIdx));
        mDoneButton.setVisibility(mNavbarVisibilityDelegate.getDoneButtonVisibility(currentIdx));

        mTabLayout.setVisibility(
                mNavbarVisibilityDelegate.getProgressIndicatorVisibility(currentIdx));
    }

    private void recordMetricsOnButtonPress(int currentStepIdx, int followingStepIdx) {
        assert currentStepIdx != followingStepIdx : "currentStepIdx is equal to followingStepIdx";

        // Record metrics when the user clicks the next/back button
        if (currentStepIdx > followingStepIdx) {
            PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                    mPagerAdapter.getFragmentType(currentStepIdx));
        } else {
            mPrivacyGuideMetricsDelegate.recordMetricsOnNextForCard(
                    mPagerAdapter.getFragmentType(currentStepIdx));
        }

        // Record the initial state of the next/previous card
        mPrivacyGuideMetricsDelegate.setInitialStateForCard(
                mPagerAdapter.getFragmentType(followingStepIdx));
    }

    @Override
    public void onAttachFragment(@NonNull Fragment childFragment) {
        if (childFragment instanceof SafeBrowsingFragment) {
            ((SafeBrowsingFragment) childFragment).setBottomSheetController(mBottomSheetController);
        }
        if (childFragment instanceof DoneFragment) {
            DoneFragment doneFragment = (DoneFragment) childFragment;
            doneFragment.setCustomTabIntentHelper(mCustomTabHelper);
            doneFragment.setSettingsLauncher(mSettingsLauncher);
        }
    }

    @Override
    public void onCreateOptionsMenu(@NonNull Menu menu, @NonNull MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        menu.clear();
        inflater.inflate(R.menu.privacy_guide_toolbar_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == R.id.close_menu_id) {
            getActivity().onBackPressed();
            return true;
        }

        return false;
    }

    public void setBottomSheetController(BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
    }

    public void setCustomTabIntentHelper(CustomTabIntentHelper tabHelper) {
        mCustomTabHelper = tabHelper;
    }

    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    void setPrivacyGuideMetricsDelegateForTesting(
            @Nullable PrivacyGuideMetricsDelegate privacyGuideMetricsDelegate) {
        mPrivacyGuideMetricsDelegate = privacyGuideMetricsDelegate;
    }
}
