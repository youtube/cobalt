// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.DialogInterface;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.StringRes;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.TextViewWithLeading;
import org.chromium.url.GURL;

/** Dialog in the form of a notice shown for the Privacy Sandbox. */
public class PrivacySandboxDialogNoticeROW extends ChromeDialog
        implements DialogInterface.OnShowListener {
    private final PrivacySandboxBridge mPrivacySandboxBridge;
    private View mContentView;

    private LinearLayout mNoticeViewContainer;
    private final CheckableImageView mExpandArrowView;
    private LinearLayout mDropdownContainer;
    private LinearLayout mDropdownElement;
    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;
    private @SurfaceType int mSurfaceType;
    private @StringRes int mLearnMoreBullet2StringRes =
            R.string.privacy_sandbox_m1_notice_row_learn_more_bullet_2;

    private LinearLayout mPrivacyPolicyView;
    private FrameLayout mPrivacyPolicyContent;
    private ChromeImageButton mPrivacyPolicyBackButton;
    private TextViewWithLeading mLearnMoreDescription5V2;
    private ThinWebView mThinWebView;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private final Profile mProfile;
    private long mPrivacyPolicyClickedTimestamp;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private boolean mIsPrivacyPageLoaded;
    private View.OnClickListener mOnClickListener;

    public PrivacySandboxDialogNoticeROW(
            Context context,
            PrivacySandboxBridge privacySandboxBridge,
            @SurfaceType int surfaceType,
            Profile profile,
            ActivityWindowAndroid activityWindowAndroid) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mPrivacySandboxBridge = privacySandboxBridge;
        mSurfaceType = surfaceType;
        mProfile = profile;
        mActivityWindowAndroid = activityWindowAndroid;
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_notice_row, null);
        setContentView(mContentView);
        mOnClickListener = getOnClickListener();

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(mOnClickListener);
        ButtonCompat settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(mOnClickListener);

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        // Controls for the expanding section.
        mDropdownElement = mContentView.findViewById(R.id.dropdown_element);
        mDropdownElement.setOnClickListener(mOnClickListener);
        mDropdownContainer = mContentView.findViewById(R.id.dropdown_container);
        mExpandArrowView = mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(PrivacySandboxDialogUtils.createExpandDrawable(context));
        mExpandArrowView.setChecked(isDropdownExpanded());

        mNoticeViewContainer = mContentView.findViewById(R.id.privacy_sandbox_notice_row_view);
        mPrivacyPolicyView = mContentView.findViewById(R.id.privacy_policy_view);
        mPrivacyPolicyContent = mContentView.findViewById(R.id.privacy_policy_content);
        mPrivacyPolicyBackButton = mContentView.findViewById(R.id.privacy_policy_back_button);
        mPrivacyPolicyBackButton.setOnClickListener(mOnClickListener);
        mIsPrivacyPageLoaded = false;

        mMoreButton.setOnClickListener(mOnClickListener);
        setOnShowListener(this);
        setCancelable(false);

        mScrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        () -> {
                            if (!mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
                                mMoreButton.setVisibility(View.GONE);
                                mActionButtons.setVisibility(View.VISIBLE);
                                mScrollView.post(
                                        () -> {
                                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                                        });
                            }
                        });
        handleAdsApiUxEnhancements();
    }

    private View.OnClickListener getOnClickListener() {
        return new PrivacySandboxDebouncedOnClick(
                "ThreeAdsAPIsNoticeModal"
                        + PrivacySandboxDialogUtils.getSurfaceTypeAsString(mSurfaceType)) {
            @Override
            public void processClick(View v) {
                processClickImpl(v);
            }
        };
    }

    private void handleAdsApiUxEnhancements() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)) {
            return;
        }
        mContentView
                .findViewById(R.id.privacy_sandbox_m1_notice_row_description_2)
                .setVisibility(View.GONE);
        mContentView
                .findViewById(R.id.privacy_sandbox_m1_notice_row_description_2_v2)
                .setVisibility(View.VISIBLE);
        ((TextViewWithLeading)
                        mContentView.findViewById(R.id.privacy_sandbox_m1_notice_row_description_4))
                .setText(getContext().getString(R.string.privacy_sandbox_m1_notice_row_last_text));
        mLearnMoreBullet2StringRes = R.string.privacy_sandbox_m1_notice_row_learn_more_bullet_2_v2;
    }

    private void handleAdsApiUxEnhancementsDropdown() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)) {
            return;
        }
        ((TextViewWithLeading)
                        mContentView.findViewById(
                                R.id.privacy_sandbox_m1_notice_row_learn_more_description_2))
                .setText(
                        getContext()
                                .getString(
                                        R.string
                                                .privacy_sandbox_m1_notice_row_learn_more_description_2_v2));
        mContentView
                .findViewById(R.id.privacy_sandbox_m1_notice_row_learn_more_description_5)
                .setVisibility(View.GONE);
        mContentView
                .findViewById(R.id.privacy_sandbox_m1_notice_row_learn_more_heading_3)
                .setVisibility(View.VISIBLE);
        mContentView
                .findViewById(R.id.privacy_sandbox_m1_notice_row_learn_more_description_5_v2)
                .setVisibility(View.VISIBLE);
        mContentView
                .findViewById(R.id.privacy_sandbox_m1_notice_row_learn_more_heading_1)
                .setVisibility(View.GONE);
        mContentView
                .findViewById(R.id.privacy_sandbox_m1_notice_row_learn_more_description_android)
                .setVisibility(View.GONE);
        mContentView
                .findViewById(R.id.privacy_sandbox_m1_notice_row_learn_more_description_android_v2)
                .setVisibility(View.VISIBLE);
    }

    private void handlePrivacyPolicyBackButtonClicked() {
        mPrivacyPolicyView.setVisibility(View.GONE);
        mPrivacyPolicyContent.removeAllViews();
        mNoticeViewContainer.setVisibility(View.VISIBLE);
    }

    @Override
    public void show() {
        mPrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_SHOWN, mSurfaceType);
        super.show();
    }

    public void processClickImpl(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            RecordUserAction.record("Settings.PrivacySandbox.NoticeRowDialog.AckClicked");
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_ACKNOWLEDGE, mSurfaceType);
            dismiss();
        } else if (id == R.id.settings_button) {
            RecordUserAction.record("Settings.PrivacySandbox.NoticeRowDialog.OpenSettingsClicked");
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_OPEN_SETTINGS, mSurfaceType);
            dismiss();
            PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                    getContext(), PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
        } else if (id == R.id.more_button) {
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_MORE_BUTTON_CLICKED, mSurfaceType);
            if (mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
                mScrollView.post(
                        () -> {
                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                        });
            } else {
                mMoreButton.setVisibility(View.GONE);
                mActionButtons.setVisibility(View.VISIBLE);
                mScrollView.post(
                        () -> {
                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                        });
            }
        } else if (id == R.id.dropdown_element) {
            if (isDropdownExpanded()) {
                mPrivacySandboxBridge.promptActionOccurred(
                        PromptAction.NOTICE_MORE_INFO_CLOSED, mSurfaceType);
                mDropdownContainer.setVisibility(View.GONE);
                mDropdownContainer.removeAllViews();
            } else {
                mDropdownContainer.setVisibility(View.VISIBLE);
                mPrivacySandboxBridge.promptActionOccurred(
                        PromptAction.NOTICE_MORE_INFO_OPENED, mSurfaceType);
                LayoutInflater.from(getContext())
                        .inflate(R.layout.privacy_sandbox_notice_row_dropdown, mDropdownContainer);

                PrivacySandboxDialogUtils.setBulletText(
                        getContext(),
                        mContentView,
                        R.id.privacy_sandbox_m1_notice_row_learn_more_bullet_one,
                        R.string.privacy_sandbox_m1_notice_row_learn_more_bullet_1);
                PrivacySandboxDialogUtils.setBulletText(
                        getContext(),
                        mContentView,
                        R.id.privacy_sandbox_m1_notice_row_learn_more_bullet_two,
                        mLearnMoreBullet2StringRes);

                mScrollView.post(
                        () -> {
                            mScrollView.scrollTo(0, mDropdownElement.getTop());
                        });
                handleAdsApiUxEnhancementsDropdown();
                handlePrivacyPolicyFeature();
            }

            mExpandArrowView.setChecked(isDropdownExpanded());
            PrivacySandboxDialogUtils.updateDropdownControlContentDescription(
                    getContext(),
                    view,
                    isDropdownExpanded(),
                    R.string.privacy_sandbox_m1_notice_row_learn_more_expand_label);
            view.announceForAccessibility(
                    getContext()
                            .getString(
                                    isDropdownExpanded()
                                            ? R.string.accessibility_expanded_group
                                            : R.string.accessibility_collapsed_group));
        } else if (id == R.id.privacy_policy_back_button) {
            handlePrivacyPolicyBackButtonClicked();
        }
    }

    @Override
    public void onShow(DialogInterface dialogInterface) {
        if (mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
            mMoreButton.setVisibility(View.VISIBLE);
            mActionButtons.setVisibility(View.GONE);
        } else {
            mMoreButton.setVisibility(View.GONE);
            mActionButtons.setVisibility(View.VISIBLE);
        }
        mScrollView.setVisibility(View.VISIBLE);
    }

    @Override
    protected void onStop() {
        super.onStop();

        // Clean up the WebContents, WebContentsObserver and when the dialog is stopped
        if (mThinWebView != null) {
            mWebContents.destroy();
            mWebContents = null;
            mWebContentsObserver.observe(null);
            mWebContentsObserver = null;
            mThinWebView.destroy();
            mThinWebView = null;
        }
    }

    private void handlePrivacyPolicyFeature() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_PRIVACY_POLICY)) {
            mLearnMoreDescription5V2 =
                    mContentView.findViewById(
                            R.id.privacy_sandbox_m1_notice_row_learn_more_description_5_v2);
            mLearnMoreDescription5V2.setText(
                    SpanApplier.applySpans(
                            getContext()
                                    .getString(
                                            R.string
                                                    .privacy_sandbox_m1_notice_row_learn_more_description_5_v2_clank),
                            new SpanApplier.SpanInfo(
                                    "<link>",
                                    "</link>",
                                    new ChromeClickableSpan(
                                            getContext(), this::onPrivacyPolicyClicked))));
            mLearnMoreDescription5V2.setMovementMethod(LinkMovementMethod.getInstance());
            if (mThinWebView == null || mWebContents == null || mWebContents.isDestroyed()) {
                mWebContents = WebContentsFactory.createWebContents(mProfile, true, false);
                mWebContentsObserver =
                        new WebContentsObserver(mWebContents) {
                            @Override
                            public void didFirstVisuallyNonEmptyPaint() {
                                if (!mIsPrivacyPageLoaded) {
                                    RecordHistogram.recordTimesHistogram(
                                            "PrivacySandbox.PrivacyPolicy.LoadingTime",
                                            System.currentTimeMillis()
                                                    - mPrivacyPolicyClickedTimestamp);
                                    mIsPrivacyPageLoaded = true;
                                }
                            }

                            @Override
                            public void didFailLoad(
                                    boolean isInPrimaryMainFrame,
                                    int errorCode,
                                    GURL failingUrl,
                                    @LifecycleState int rfhLifecycleState) {
                                RecordHistogram.recordSparseHistogram(
                                        "PrivacySandbox.PrivacyPolicy.FailedLoadErrorCode",
                                        errorCode);
                            }
                        };
                mThinWebView =
                        PrivacySandboxDialogController.createPrivacyPolicyThinWebView(
                                mWebContents, mProfile, mActivityWindowAndroid);
            }
        }
    }

    /**
     * Handles clicks on the Privacy Policy link. If a ThinWebView is available, loads and displays
     * the privacy policy within it, replacing the consent view.
     *
     * @param unused_view The View that was clicked (typically the TextView containing the link).
     */
    private void onPrivacyPolicyClicked(View unused_view) {
        RecordUserAction.record("Settings.PrivacySandbox.Notice.PrivacyPolicyLinkClicked");
        mPrivacyPolicyClickedTimestamp = System.currentTimeMillis();
        mPrivacyPolicyContent.removeAllViews();
        if (mThinWebView != null && mThinWebView.getView() != null) {
            mNoticeViewContainer.setVisibility(View.GONE);
            mPrivacyPolicyContent.addView(mThinWebView.getView());
            mPrivacyPolicyView.setVisibility(View.VISIBLE);
        }
    }

    private boolean isDropdownExpanded() {
        return mDropdownContainer != null && mDropdownContainer.getVisibility() == View.VISIBLE;
    }
}
