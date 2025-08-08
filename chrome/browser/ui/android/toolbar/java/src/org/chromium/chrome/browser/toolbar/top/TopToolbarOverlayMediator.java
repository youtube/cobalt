// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsOffsetTagsInfo;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

/** The business logic for controlling the top toolbar's cc texture. */
public class TopToolbarOverlayMediator {
    // Forced testing params.
    private static Boolean sIsTabletForTesting;
    private static Integer sToolbarBackgroundColorForTesting;
    private static Integer sUrlBarColorForTesting;

    /** An Android Context. */
    private final Context mContext;

    /** A handle to the layout manager for observing scene changes. */
    private final LayoutStateProvider mLayoutStateProvider;

    /** The observer of changes to the active layout. */
    private final LayoutStateObserver mSceneChangeObserver;

    /** A means of populating draw info for the progress bar. */
    private final Callback<ClipDrawableProgressBar.DrawingInfo> mProgressInfoCallback;

    /** An observer that watches for changes in the active tab. */
    private final CurrentTabObserver mTabObserver;

    /** Access to the current state of the browser controls. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** An observer of the browser controls offsets. */
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    private final TopUiThemeColorProvider mTopUiThemeColorProvider;

    /** The view state for this overlay. */
    private final PropertyModel mModel;

    private final Supplier<Integer> mBottomToolbarControlsOffsetSupplier;

    /** Whether visibility is controlled internally or manually by the feature. */
    private boolean mIsVisibilityManuallyControlled;

    /** Whether the android view for this overlay is visible. */
    private boolean mIsToolbarAndroidViewVisible;

    /** Whether the parent of the view for this overlay is visible. */
    private boolean mIsBrowserControlsAndroidViewVisible;

    /** Whether the overlay should be visible despite other signals. */
    private boolean mManualVisibility;

    /** Whether a layout that this overlay can be displayed on is showing. */
    private boolean mIsOnValidLayout;

    private ObservableSupplier<Tab> mTabSupplier;
    private float mViewportHeight;

    TopToolbarOverlayMediator(
            PropertyModel model,
            Context context,
            LayoutStateProvider layoutStateProvider,
            Callback<ClipDrawableProgressBar.DrawingInfo> progressInfoCallback,
            ObservableSupplier<Tab> tabSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            TopUiThemeColorProvider topUiThemeColorProvider,
            Supplier<Integer> bottomToolbarControlsOffsetSupplier,
            int layoutsToShowOn,
            boolean manualVisibilityControl) {
        mContext = context;
        mLayoutStateProvider = layoutStateProvider;
        mProgressInfoCallback = progressInfoCallback;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mModel = model;
        mBottomToolbarControlsOffsetSupplier = bottomToolbarControlsOffsetSupplier;
        mIsVisibilityManuallyControlled = manualVisibilityControl;
        mIsOnValidLayout = (mLayoutStateProvider.getActiveLayoutType() & layoutsToShowOn) > 0;
        mTabSupplier = tabSupplier;
        updateVisibility();

        mSceneChangeObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        mIsOnValidLayout = (layoutType & layoutsToShowOn) > 0;
                        updateVisibility();
                    }
                };
        mLayoutStateProvider.addObserver(mSceneChangeObserver);

        // Keep an observer attached to the visible tab (and only the visible tab) to update
        // properties including theme color.
        Callback<Tab> activityTabCallback =
                (tab) -> {
                    if (tab == null) return;
                    updateVisibility();
                    updateThemeColor(tab);
                    updateProgress();
                    updateAnonymize(tab);
                };
        mTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onDidChangeThemeColor(Tab tab, int color) {
                                updateThemeColor(tab);
                            }

                            @Override
                            public void onLoadProgressChanged(Tab tab, float progress) {
                                updateProgress();
                            }

                            @Override
                            public void onContentChanged(Tab tab) {
                                updateVisibility();
                                updateThemeColor(tab);
                                updateAnonymize(tab);
                            }

                            @Override
                            public void didBackForwardTransitionAnimationChange(Tab tab) {
                                updateVisibility();
                            }
                        },
                        activityTabCallback);

        activityTabCallback.onResult(tabSupplier.get());
        mTabObserver.triggerWithCurrentTab();

        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate,
                            boolean isVisibilityForced) {
                        if (!ChromeFeatureList.sBrowserControlsInViz.isEnabled()
                                || needsAnimate
                                || isVisibilityForced) {
                            updateContentOffset();
                        }

                        // TODO(peilinwang) Clean up this flag and remove the updateVisibility call
                        // when stable experiment is finished.
                        if (!ChromeFeatureList.sBcivZeroBrowserFrames.isEnabled()) {
                            updateShadowState();
                            updateVisibility();
                        }
                    }

                    @Override
                    public void onAndroidControlsVisibilityChanged(int visibility) {
                        if (ToolbarFeatures.shouldSuppressCaptures()) {
                            mIsBrowserControlsAndroidViewVisible = visibility == View.VISIBLE;
                            updateShadowState();
                        }
                    }

                    @Override
                    public void onControlsConstraintsChanged(
                            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                            BrowserControlsOffsetTagsInfo offsetTagsInfo,
                            @BrowserControlsState int constraints) {
                        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
                            if (ChromeFeatureList.sBcivZeroBrowserFrames.isEnabled()) {
                                mModel.set(
                                        TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG,
                                        offsetTagsInfo.getTopControlsOffsetTag());
                            } else {
                                mModel.set(
                                        TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG,
                                        offsetTagsInfo.getContentOffsetTag());
                            }

                            if (mBrowserControlsStateProvider
                                    .shouldUpdateOffsetsWhenConstraintsChange()) {
                                mModel.set(
                                        TopToolbarOverlayProperties.CONTENT_OFFSET,
                                        mBrowserControlsStateProvider.getContentOffset());
                            }
                        }
                    }
                };
        mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
        if (ToolbarFeatures.shouldSuppressCaptures()) {
            mIsBrowserControlsAndroidViewVisible =
                    mBrowserControlsStateProvider.getAndroidControlsVisibility() == View.VISIBLE;
        }
    }

    /**
     * Set whether the android view corresponding with this overlay is showing.
     * @param isVisible Whether the android view is visible.
     */
    void setIsAndroidViewVisible(boolean isVisible) {
        mIsToolbarAndroidViewVisible = isVisible;
        updateShadowState();
    }

    /**
     * Compute whether the texture's shadow should be visible. The shadow is visible whenever the
     * android view is not shown.
     */
    private void updateShadowState() {
        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()
                && ChromeFeatureList.sBcivZeroBrowserFrames.isEnabled()) {
            // With BCIV enabled, we show the hairline on the composited toolbar by default,
            // and we don't want to update its visibility from the browser, because that incurs a
            // compositor frame.
            return;
        }

        boolean drawControlsAsTexture;
        if (ToolbarFeatures.shouldSuppressCaptures()) {
            drawControlsAsTexture = !mIsBrowserControlsAndroidViewVisible;
        } else {
            drawControlsAsTexture =
                    BrowserControlsUtils.drawControlsAsTexture(mBrowserControlsStateProvider);
        }
        boolean showShadow =
                drawControlsAsTexture
                        || !mIsToolbarAndroidViewVisible
                        || mIsVisibilityManuallyControlled;

        mModel.set(TopToolbarOverlayProperties.SHOW_SHADOW, showShadow);
    }

    /**
     * Update the colors of the layer based on the specified tab.
     * @param tab The tab to base the colors on.
     */
    private void updateThemeColor(Tab tab) {
        @ColorInt int color = getToolbarBackgroundColor(tab);
        mModel.set(TopToolbarOverlayProperties.TOOLBAR_BACKGROUND_COLOR, color);
        mModel.set(TopToolbarOverlayProperties.URL_BAR_COLOR, getUrlBarBackgroundColor(tab, color));
    }

    /**
     * @param tab The tab to get the background color for.
     * @return The background color.
     */
    private @ColorInt int getToolbarBackgroundColor(Tab tab) {
        if (sToolbarBackgroundColorForTesting != null) return sToolbarBackgroundColorForTesting;
        return mTopUiThemeColorProvider.getSceneLayerBackground(tab);
    }

    /**
     * @param tab The tab to get the background color for.
     * @param backgroundColor The tab's background color.
     * @return The url bar color.
     */
    private @ColorInt int getUrlBarBackgroundColor(Tab tab, @ColorInt int backgroundColor) {
        if (sUrlBarColorForTesting != null) return sUrlBarColorForTesting;
        return ThemeUtils.getTextBoxColorForToolbarBackground(mContext, tab, backgroundColor);
    }

    /** Update the state of the composited progress bar. */
    private void updateProgress() {
        // Tablets have their own version of a progress "spinner".
        if (isTablet()) return;

        if (mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO) == null) {
            mModel.set(
                    TopToolbarOverlayProperties.PROGRESS_BAR_INFO,
                    new ClipDrawableProgressBar.DrawingInfo());
        }

        // Update and set the progress info to trigger an update; the PROGRESS_BAR_INFO
        // property skips the object equality check.
        mProgressInfoCallback.onResult(mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
        mModel.set(
                TopToolbarOverlayProperties.PROGRESS_BAR_INFO,
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
    }

    /** @return Whether this component is in tablet mode. */
    private boolean isTablet() {
        if (sIsTabletForTesting != null) return sIsTabletForTesting;
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    /** Clean up any state and observers. */
    void destroy() {
        mTabObserver.destroy();

        mLayoutStateProvider.removeObserver(mSceneChangeObserver);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
    }

    /** Update the visibility of the overlay. */
    private void updateVisibility() {
        Tab tab = mTabSupplier.get();
        if (tab != null && tab.isNativePage() && tab.isDisplayingBackForwardAnimation()) {
            // TODO(crbug.com/365818512): Add a screenshot capture test to cover this case.
            mModel.set(TopToolbarOverlayProperties.VISIBLE, false);
        } else if (mIsVisibilityManuallyControlled) {
            mModel.set(TopToolbarOverlayProperties.VISIBLE, mManualVisibility && mIsOnValidLayout);
        } else {
            boolean visibility =
                    !BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsStateProvider)
                            && mIsOnValidLayout;
            mModel.set(TopToolbarOverlayProperties.VISIBLE, visibility);
        }
    }

    private void updateAnonymize(Tab tab) {
        if (!mIsVisibilityManuallyControlled && ToolbarFeatures.shouldSuppressCaptures()) {
            boolean isNativePage = tab.isNativePage();
            mModel.set(TopToolbarOverlayProperties.ANONYMIZE, isNativePage);
        }
    }

    /** @return Whether this overlay should be attached to the tree. */
    boolean shouldBeAttachedToTree() {
        return true;
    }

    /** @param xOffset The x offset of the toolbar. */
    void setXOffset(float xOffset) {
        mModel.set(TopToolbarOverlayProperties.X_OFFSET, xOffset);
    }

    /** @param anonymize Whether the URL should be hidden when the layer is rendered. */
    void setAnonymize(boolean anonymize) {
        mModel.set(TopToolbarOverlayProperties.ANONYMIZE, anonymize);
    }

    /** @param visible Whether the overlay and shadow should be visible despite other signals. */
    void setManualVisibility(boolean visible) {
        assert mIsVisibilityManuallyControlled
                : "Manual visibility control was not set for this overlay.";
        mManualVisibility = visible;
        updateShadowState();
        updateVisibility();
    }

    void setVisibilityManuallyControlledForTesting(boolean manuallyControlled) {
        mIsVisibilityManuallyControlled = manuallyControlled;
        updateShadowState();
        updateVisibility();
    }

    void setViewportHeight(float viewportHeight) {
        if (viewportHeight == mViewportHeight) return;
        mViewportHeight = viewportHeight;
        updateContentOffset();
    }

    private void updateContentOffset() {
        // When top-anchored, the content offset is used to position the toolbar
        // layer instead of the top controls offset because the top controls can
        // have a different height than that of just the toolbar, (e.g. when status
        // indicator is visible or tab strip is hidden), and the toolbar should be
        // positioned at the bottom of the top controls regardless of the overall
        // height.
        // When the toolbar is bottom-anchored, the situation is even more ambiguous
        // because the bottom-anchored toolbar can't be assumed to sit at the top or
        // bottom of the bottom controls stack. Instead, we rely on an offset
        // provided to us indirectly via BottomControlsStacker, which controls the
        // position of bottom controls layers.
        int contentOffset = mBrowserControlsStateProvider.getContentOffset();
        if (mBrowserControlsStateProvider.getControlsPosition() == ControlsPosition.BOTTOM) {
            contentOffset = (int) (mBottomToolbarControlsOffsetSupplier.get() + mViewportHeight);
        }

        mModel.set(TopToolbarOverlayProperties.CONTENT_OFFSET, contentOffset);
    }

    static void setIsTabletForTesting(Boolean isTablet) {
        sIsTabletForTesting = isTablet;
        ResettersForTesting.register(() -> sIsTabletForTesting = null);
    }

    static void setToolbarBackgroundColorForTesting(@ColorInt int color) {
        sToolbarBackgroundColorForTesting = color;
        ResettersForTesting.register(() -> sToolbarBackgroundColorForTesting = null);
    }

    static void setUrlBarColorForTesting(@ColorInt int color) {
        sUrlBarColorForTesting = color;
        ResettersForTesting.register(() -> sUrlBarColorForTesting = null);
    }
}
