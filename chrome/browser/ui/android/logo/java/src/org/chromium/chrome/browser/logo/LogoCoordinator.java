// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.content.Context;
import android.view.View.MeasureSpec;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator used to fetch and load logo image for Start surface and NTP.*/
public class LogoCoordinator {
    private final LogoMediator mMediator;
    private final PropertyModel mLogoModel;
    private LogoView mLogoView;

    // The default google logo that is shared across all NTPs.
    static final CachedTintedBitmap sDefaultGoogleLogo =
            new CachedTintedBitmap(R.drawable.google_logo, R.color.google_logo_tint_color);

    /** Interface for the observers of the logo visibility change. */
    public interface VisibilityObserver {
        void onLogoVisibilityChanged();
    }

    /**
     * Creates a LogoCoordinator object.
     *
     * @param context Used to load colors and resources.
     * @param logoClickedCallback Supplies the StartSurface's parent tab.
     * @param logoView The view that shows the search provider logo.
     * @param shouldFetchDoodle Whether to fetch doodle if there is.
     * @param onLogoAvailableCallback The callback for when logo is available.
     * @param onCachedLogoRevalidatedRunnable The runnable for when cached logo is revalidated.
     * @param isParentSurfaceShown Whether Start surface homepage or NTP is shown. This value
     *                             is true when this class is used by NTP; while used by Start,
     *                             it's only true on Start homepage.
     * @param visibilityObserver Observer object monitoring logo visibility.
     */
    public LogoCoordinator(Context context, Callback<LoadUrlParams> logoClickedCallback,
            LogoView logoView, boolean shouldFetchDoodle, Callback<Logo> onLogoAvailableCallback,
            Runnable onCachedLogoRevalidatedRunnable, boolean isParentSurfaceShown,
            VisibilityObserver visibilityObserver) {
        // TODO(crbug.com/1394983): This is weird that we're passing in our view,
        //  and we have to expose our view via getView. We shouldn't only have to do one of these.
        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        mLogoView = logoView;
        PropertyModelChangeProcessor.create(mLogoModel, mLogoView, new LogoViewBinder());
        mMediator = new LogoMediator(context, logoClickedCallback, mLogoModel, shouldFetchDoodle,
                onLogoAvailableCallback, onCachedLogoRevalidatedRunnable, isParentSurfaceShown,
                visibilityObserver, sDefaultGoogleLogo);
    }

    /** @see LogoMediator#initWithNative */
    public void initWithNative() {
        // TODO(crbug.com/1394983): Would be more elegant if we were given an
        //  onNativeInitializedObserver and didn't rely on the good will of outside callers to
        //  invoke this.
        mMediator.initWithNative();
    }

    /** @see LogoMediator#loadSearchProviderLogoWithAnimation */
    public void loadSearchProviderLogoWithAnimation() {
        mMediator.loadSearchProviderLogoWithAnimation();
    }

    /** @see LogoMediator#updateVisibilityAndMaybeCleanUp */
    public void updateVisibilityAndMaybeCleanUp(
            boolean isParentSurfaceShown, boolean shouldDestroyBridge, boolean animationEnabled) {
        mMediator.updateVisibilityAndMaybeCleanUp(
                isParentSurfaceShown, shouldDestroyBridge, animationEnabled);
    }

    /** @see LogoMediator#destroy */
    public void destroy() {
        mMediator.destroy();
        mLogoView.destroy();
        mLogoView = null;
    }

    /**
     * Convenience method to call measure() on the logo view with MeasureSpecs converted from the
     * given dimensions (in pixels) with MeasureSpec.EXACTLY.
     */
    public void measureExactlyLogoView(int widthPx) {
        mLogoView.measure(MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(mLogoView.getMeasuredHeight(), MeasureSpec.EXACTLY));
    }

    /** Jumps to the end of the logo view's cross-fading animation, if any.*/
    public void endFadeAnimation() {
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setAlpha(float alpha) {
        mLogoModel.set(LogoProperties.ALPHA, alpha);
    }

    /**
     * Sets the top margin of the logo view.
     *
     * @param topMargin The expected top margin.
     */
    public void setTopMargin(int topMargin) {
        mLogoModel.set(LogoProperties.LOGO_TOP_MARGIN, topMargin);
    }

    /**
     * Sets the bottom margin of the logo view.
     *
     * @param bottomMargin The expected bottom margin.
     */
    public void setBottomMargin(int bottomMargin) {
        mLogoModel.set(LogoProperties.LOGO_BOTTOM_MARGIN, bottomMargin);
    }

    /** @see LogoMediator#isLogoVisible */
    public boolean isLogoVisible() {
        return mMediator.isLogoVisible();
    }

    /** @see LogoMediator#onTemplateURLServiceChanged */
    public void onTemplateURLServiceChangedForTesting() {
        mMediator.onTemplateURLServiceChanged();
    }

    /** @see LogoMediator#onLogoClicked */
    public void onLogoClickedForTesting(boolean isAnimatedLogoShowing) {
        mMediator.onLogoClicked(isAnimatedLogoShowing);
    }

    public void setLogoBridgeForTesting(LogoBridge logoBridge) {
        mMediator.setLogoBridgeForTesting(logoBridge);
    }

    public void setOnLogoClickUrlForTesting(String onLogoClickUrl) {
        mMediator.setOnLogoClickUrlForTesting(onLogoClickUrl);
    }
}
