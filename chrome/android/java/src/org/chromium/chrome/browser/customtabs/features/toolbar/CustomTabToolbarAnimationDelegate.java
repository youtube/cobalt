// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.util.TypedValue;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.components.omnibox.SecurityButtonAnimationDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;

/**
 * A delegate class to handle the title animation and security icon animation in
 * {@link CustomTabToolbar}.
 * <p>
 * How does the title animation work?
 * <p>
 * 1. The title bar is set from {@link View#GONE} to {@link View#VISIBLE}, which triggers a relayout
 * of the location bar. 2. On relayout, the newly positioned urlbar will be moved&scaled to look
 * exactly the same as before the relayout. (Note the scale factor is calculated based on
 * {@link TextView#getTextSize()}, not height or width.) 3. Finally the urlbar will be animated to
 * its new position.
 *
 * <p>
 * How does the security button animation work?
 * </p>
 * See {@link SecurityButtonAnimationDelegate} and {@link BrandingSecurityButtonAnimationDelegate}.
 */
class CustomTabToolbarAnimationDelegate {
    private final SecurityButtonAnimationDelegate mSecurityButtonAnimationDelegate;
    private final BrandingSecurityButtonAnimationDelegate mBrandingAnimationDelegate;

    private TextView mUrlBar;
    private TextView mTitleBar;
    // A flag controlling whether the animation has run before.
    private boolean mShouldRunTitleAnimation;
    private boolean mUseRotationTransition;
    private @DrawableRes int mSecurityIconRes;
    private boolean mIsInAnimation;

    /**
     * Constructs an instance of {@link CustomTabToolbarAnimationDelegate}.
     */
    CustomTabToolbarAnimationDelegate(ImageButton securityButton, final View titleUrlContainer,
            @DimenRes int securityStatusIconSize) {
        int securityButtonWidth =
                securityButton.getResources().getDimensionPixelSize(securityStatusIconSize);
        titleUrlContainer.setTranslationX(-securityButtonWidth);
        mSecurityButtonAnimationDelegate = new SecurityButtonAnimationDelegate(
                securityButton, titleUrlContainer, securityStatusIconSize);
        mBrandingAnimationDelegate = new BrandingSecurityButtonAnimationDelegate(securityButton);
    }

    /**
     * Sets whether the title scaling animation is enabled.
     */
    void setTitleAnimationEnabled(boolean enabled) {
        mShouldRunTitleAnimation = enabled;
    }

    void prepareTitleAnim(TextView urlBar, TextView titleBar) {
        mTitleBar = titleBar;
        mUrlBar = urlBar;
        mUrlBar.setPivotX(0f);
        mUrlBar.setPivotY(0f);
        mShouldRunTitleAnimation = true;
    }

    /**
     * Starts animation for urlbar scaling and title fading-in. If this animation has already run
     * once, does nothing.
     */
    void startTitleAnimation(Context context) {
        if (!mShouldRunTitleAnimation) return;
        mShouldRunTitleAnimation = false;

        mTitleBar.setVisibility(View.VISIBLE);
        mTitleBar.setAlpha(0f);

        float newSizeSp = context.getResources().getDimension(R.dimen.custom_tabs_url_text_size);

        float oldSizePx = mUrlBar.getTextSize();
        mUrlBar.setTextSize(TypedValue.COMPLEX_UNIT_PX, newSizeSp);

        // View#getY() cannot be used because the boundary of the parent will change after relayout.
        final int[] oldLoc = new int[2];
        mUrlBar.getLocationInWindow(oldLoc);

        ViewUtils.requestLayout(mUrlBar, "CustomTabToolbarAnimationDelegate.startTitleAnimation");

        mUrlBar.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                mUrlBar.removeOnLayoutChangeListener(this);

                int[] newLoc = new int[2];
                mUrlBar.getLocationInWindow(newLoc);

                // The size may change during the measuring pass, so we should calculate the new
                // size here, after the layout is done.
                float newSizePx = mUrlBar.getTextSize();
                final float scale = oldSizePx / newSizePx;

                mUrlBar.setScaleX(scale);
                mUrlBar.setScaleY(scale);
                mUrlBar.setTranslationX(oldLoc[0] - newLoc[0]);
                mUrlBar.setTranslationY(oldLoc[1] - newLoc[1]);

                mIsInAnimation = true;
                mUrlBar.animate()
                        .scaleX(1f)
                        .scaleY(1f)
                        .translationX(0)
                        .translationY(0)
                        .setDuration(SecurityButtonAnimationDelegate.SLIDE_DURATION_MS)
                        .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                        .setListener(new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationEnd(Animator animation) {
                                mTitleBar.animate()
                                        .alpha(1f)
                                        .setInterpolator(
                                                Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR)
                                        .setDuration(
                                                SecurityButtonAnimationDelegate.FADE_DURATION_MS)
                                        .setListener(new AnimatorListenerAdapter() {
                                            @Override
                                            public void onAnimationEnd(Animator animation) {
                                                mIsInAnimation = false;
                                            }
                                        })
                                        .start();
                            }
                        })
                        .start();
            }
        });
    }

    /**
     * Starts the animation to show/hide the security button,
     * @param securityIconResource  The updated resource to be assigned to the security status icon.
     * When this is null, the icon is animated to the left and faded out.
     */
    void updateSecurityButton(@DrawableRes int securityIconResource, boolean animate) {
        if (mUseRotationTransition && animate) {
            mBrandingAnimationDelegate.updateDrawableResource(securityIconResource);
        } else {
            boolean isActualResourceChange = true;
            if (ToolbarFeatures.shouldSuppressCaptures()) {
                isActualResourceChange = securityIconResource != mSecurityIconRes;
            }
            mSecurityButtonAnimationDelegate.updateSecurityButton(
                    securityIconResource, animate, isActualResourceChange);
        }
        mSecurityIconRes = securityIconResource;
    }

    void setUseRotationSecurityButtonTransition(boolean useRotation) {
        mUseRotationTransition = useRotation;
    }

    /** Returns the resource id for the current icon when in a steady state. */
    @DrawableRes
    int getSecurityIconRes() {
        return mSecurityIconRes;
    }

    /** Returns whether an animation is currently running. */
    boolean isInAnimation() {
        return mIsInAnimation || mBrandingAnimationDelegate.isInAnimation()
                || mSecurityButtonAnimationDelegate.isInAnimation();
    }
}
