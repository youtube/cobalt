// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.graphics.Color;
import android.graphics.RectF;
import android.util.FloatProperty;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.CompositorOnClickHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.util.ColorUtils;

import java.util.List;

/**
 * {@link StripLayoutTab} is used to keep track of the strip position and rendering information for
 * a particular tab so it can draw itself onto the GL canvas.
 */
public class StripLayoutTab implements VirtualView {
    private static final String TAG = "StripLayoutTab";

    /** An observer interface for StripLayoutTab. */
    public interface Observer {
        /** @param visible Whether the StripLayoutTab is visible. */
        void onVisibilityChanged(boolean visible);
    }

    /**
     * Delegate for additional tab functionality.
     */
    public interface StripLayoutTabDelegate {
        /**
         * Handles tab click actions.
         * @param tab The tab clicked.
         */
        void handleTabClick(StripLayoutTab tab);

        /**
         * Handles close button click actions.
         * @param tab  The tab whose close button was clicked.
         * @param time The time the close button was clicked.
         */
        void handleCloseButtonClick(StripLayoutTab tab, long time);
    }

    /** A property for animations to use for changing the X offset of the tab. */
    public static final FloatProperty<StripLayoutTab> X_OFFSET =
            new FloatProperty<StripLayoutTab>("offsetX") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setOffsetX(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getOffsetX();
                }
            };

    /** A property for animations to use for changing the Y offset of the tab. */
    public static final FloatProperty<StripLayoutTab> Y_OFFSET =
            new FloatProperty<StripLayoutTab>("offsetY") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setOffsetY(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getOffsetY();
                }
            };

    /** A property for animations to use for changing the width of the tab. */
    public static final FloatProperty<StripLayoutTab> WIDTH =
            new FloatProperty<StripLayoutTab>("width") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setWidth(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getHeight();
                }
            };

    /** A property for animations to use for changing the drawX of the tab. */
    public static final FloatProperty<StripLayoutTab> DRAW_X =
            new FloatProperty<StripLayoutTab>("drawX") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setDrawX(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getDrawX();
                }
            };

    /** A property for animations to use for changing the drawX of the tab. */
    public static final FloatProperty<StripLayoutTab> BOTTOM_MARGIN =
            new FloatProperty<StripLayoutTab>("bottomMargin") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setBottomMargin(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getBottomMargin();
                }
            };

    /** A property for animations to use for changing the trailingMargin of the tab. */
    public static final FloatProperty<StripLayoutTab> TRAILING_MARGIN =
            new FloatProperty<StripLayoutTab>("trailingMargin") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setTrailingMargin(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getTrailingMargin();
                }
            };

    /** A property for animations to use for changing the brightness of the tab. */
    public static final FloatProperty<StripLayoutTab> BRIGHTNESS =
            new FloatProperty<StripLayoutTab>("brightness") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setBrightness(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getBrightness();
                }
            };

    /** A property for animations to use for changing the opacity of the tab. */
    public static final FloatProperty<StripLayoutTab> OPACITY =
            new FloatProperty<StripLayoutTab>("opacity") {
                @Override
                public void setValue(StripLayoutTab object, float value) {
                    object.setContainerOpacity(value);
                }

                @Override
                public Float get(StripLayoutTab object) {
                    return object.getContainerOpacity();
                }
            };

    // Animation/Timer Constants
    private static final int ANIM_TAB_CLOSE_BUTTON_FADE_MS = 150;

    // Close Button Constants
    // Close button padding value comes from the built-in padding in the source png.
    private static final int CLOSE_BUTTON_PADDING_DP = 7;
    private static final int CLOSE_BUTTON_OFFSET_X = 12;
    private static final int CLOSE_BUTTON_WIDTH_DP = 48;

    // Strip Tab Offset Constants
    private static final float TOP_MARGIN_DP = 2.f;
    private static final float FOLIO_CONTENT_OFFSET_Y = 8.f;
    private static final float DETACHED_CONTENT_OFFSET_Y = 10.f;
    private static final float TOUCH_TARGET_INSET = 16.f;

    // Divider Constants
    private static final int DIVIDER_OFFSET_X = 13;

    // Close button hover highlight alpha
    private static final float CLOSE_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.12f;
    private static final float CLOSE_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.08f;
    @VisibleForTesting
    static final float DIVIDER_FOLIO_LIGHT_OPACITY = 0.2f;

    private int mId = Tab.INVALID_TAB_ID;

    private final Context mContext;
    private final StripLayoutTabDelegate mDelegate;
    private final TabLoadTracker mLoadTracker;
    private final LayoutUpdateHost mUpdateHost;
    private TintedCompositorButton mCloseButton;

    private boolean mVisible = true;
    private boolean mIsDying;
    private boolean mIsReordering;
    private boolean mCanShowCloseButton = true;
    private boolean mFolioAttached = true;
    private boolean mStartDividerVisible;
    private boolean mEndDividerVisible;
    private final boolean mIncognito;
    private float mBottomMargin;
    private float mContainerOpacity;
    private float mLeftInset;
    private float mRightInset;
    private String mAccessibilityDescription = "";

    // Ideal intermediate parameters
    private float mIdealX;
    private float mTabOffsetX;
    private float mTabOffsetY;
    private float mTrailingMargin;

    // Actual draw parameters
    private float mDrawX;
    private float mDrawY;
    private float mWidth;
    private float mHeight;
    private final RectF mTouchTarget = new RectF();

    // Startup parameters
    private boolean mIsPlaceholder;

    private boolean mShowingCloseButton = true;

    // Content Animations
    private CompositorAnimator mButtonOpacityAnimation;

    private float mLoadingSpinnerRotationDegrees;
    private float mBrightness = 1.f;

    // Preallocated
    private final RectF mClosePlacement = new RectF();

    private ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Create a {@link StripLayoutTab} that represents the {@link Tab} with an id of
     * {@code id}.
     *
     * @param context An Android context for accessing system resources.
     * @param id The id of the {@link Tab} to visually represent.
     * @param delegate The delegate for additional strip tab functionality.
     * @param loadTrackerCallback The {@link TabLoadTrackerCallback} to be notified of loading state
     *                            changes.
     * @param renderHost The {@link LayoutRenderHost}.
     * @param incognito Whether or not this layout tab is incognito.
     */
    public StripLayoutTab(Context context, int id, StripLayoutTabDelegate delegate,
            TabLoadTrackerCallback loadTrackerCallback, LayoutUpdateHost updateHost,
            boolean incognito) {
        mId = id;
        mContext = context;
        mDelegate = delegate;
        mLoadTracker = new TabLoadTracker(id, loadTrackerCallback);
        mUpdateHost = updateHost;
        mIncognito = incognito;
        CompositorOnClickHandler closeClickAction = new CompositorOnClickHandler() {
            @Override
            public void onClick(long time) {
                mDelegate.handleCloseButtonClick(StripLayoutTab.this, time);
            }
        };
        mCloseButton = new TintedCompositorButton(
                context, 0, 0, closeClickAction, R.drawable.btn_tab_close_normal);
        mCloseButton.setTintResources(
                R.color.default_icon_color_tint_list,
                R.color.default_icon_color_tint_list,
                R.color.default_icon_color_light,
                R.color.default_icon_color_light);

        mCloseButton.setBackgroundResourceId(R.drawable.tab_close_button_bg);
        int apsBackgroundHoveredTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(context),
                        (int) (CLOSE_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY * 255));
        int apsBackgroundPressedTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(context),
                        (int) (CLOSE_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY * 255));

        int apsBackgroundIncognitoHoveredTint =
                ColorUtils.setAlphaComponent(
                        context.getResources().getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (CLOSE_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY * 255));
        int apsBackgroundIncognitoPressedTint =
                ColorUtils.setAlphaComponent(
                        context.getResources().getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (CLOSE_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY * 255));

        // Only set color for hover bg.
        mCloseButton.setBackgroundTint(
                Color.TRANSPARENT,
                Color.TRANSPARENT,
                Color.TRANSPARENT,
                Color.TRANSPARENT,
                apsBackgroundHoveredTint,
                apsBackgroundPressedTint,
                apsBackgroundIncognitoHoveredTint,
                apsBackgroundIncognitoPressedTint);

        mCloseButton.setIncognito(mIncognito);
        mCloseButton.setBounds(getCloseRect());
        mCloseButton.setClickSlop(0.f);
        if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
            if (LocalizationUtils.isLayoutRtl()) {
                mLeftInset = getCloseButtonOffsetX();
                mRightInset = TOUCH_TARGET_INSET;
            } else {
                mLeftInset = TOUCH_TARGET_INSET;
                mRightInset = getCloseButtonOffsetX();
            }
        }
    }

    /** @param observer The observer to add. */
    @VisibleForTesting
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer The observer to remove. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Get a list of virtual views for accessibility events.
     *
     * @param views     A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        views.add(this);
        if (mShowingCloseButton) views.add(mCloseButton);
    }

    /**
     * Set strip tab and close button accessibility description.
     * @param description   A description for accessibility events.
     * @param title The title of the tab.
     */
    public void setAccessibilityDescription(String description, String title) {
        mAccessibilityDescription = description;
        String closeButtonDescription = ContextUtils.getApplicationContext().getString(
                R.string.accessibility_tabstrip_btn_close_tab, title);
        mCloseButton.setAccessibilityDescription(closeButtonDescription, closeButtonDescription);
    }

    @Override
    public String getAccessibilityDescription() {
        return mAccessibilityDescription;
    }

    @Override
    public void getTouchTarget(RectF target) {
        target.set(mTouchTarget);
    }

    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        // Since both the close button as well as the tab inhabit the same coordinates, the tab
        // should not consider itself hit if the close button is also hit, since it is on top.
        if (checkCloseHitTest(x, y)) return false;
        return mTouchTarget.contains(x, y);
    }

    @Override
    public void handleClick(long time) {
        mDelegate.handleTabClick(this);
    }

    /**
     * Marks if we are currently reordering this tab.
     * @param isReordering Whether the tab is reordering.
     */
    public void setIsReordering(boolean isReordering) {
        mIsReordering = isReordering;
    }

    /**
     * Marks if tab container is attached to the toolbar for the Tab Strip Redesign folio treatment.
     * @param folioAttached Whether the tab should be attached or not.
     */
    public void setFolioAttached(boolean folioAttached) {
        mFolioAttached = folioAttached;
    }

    boolean getFolioAttachedForTesting() {
        return mFolioAttached;
    }

    void setCloseButtonForTesting(TintedCompositorButton closeButton) {
        mCloseButton = closeButton;
    }

    void setShowingCloseButtonForTesting(boolean showingCloseButton) {
        mShowingCloseButton = showingCloseButton;
    }

    /**
     * Sets the id of the {@link Tab} this {@link StripLayoutTab} represents.
     */
    public void setId(int id) {
        mId = id;
    }

    /**
     * @return The id of the {@link Tab} this {@link StripLayoutTab} represents.
     */
    public int getId() {
        return mId;
    }

    /**
     * @return The Android resource that represents the tab background.
     */
    public int getResourceId() {
        if (!ChromeFeatureList.sTabStripRedesign.isEnabled()) return R.drawable.bg_tabstrip_tab;

        if (TabManagementFieldTrial.isTabStripDetachedEnabled() || !mFolioAttached
                || mIsPlaceholder) {
            return TabUiThemeUtil.getTSRDetachedResource();
        } else {
            return TabUiThemeUtil.getTSRFolioResource();
        }
    }

    /**
     * @return The Android resource that represents the tab outline.
     */
    public int getOutlineResourceId() {
        return R.drawable.bg_tabstrip_background_tab_outline;
    }

    /**
     * @return The Android resource that represents the tab divider.
     */
    public int getDividerResourceId() {
        return R.drawable.bg_tabstrip_tab_divider;
    }

    /**
     * @param foreground Whether or not this tab is a foreground tab.
     * @param hovered Whether or not this tab is hovered on.
     * @return The tint color resource that represents the tab background. A foreground tab will
     *         have the same tint irrespective of its hover state.
     */
    public int getTint(boolean foreground, boolean hovered) {
        hovered = ChromeFeatureList.isEnabled(
                          ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP)
                && hovered;
        // TODO(https://crbug.com/1408276): Avoid calculating every time. Instead, store the tab's
        //  color and only re-determine when the color could have changed (i.e. on selection).
        if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
            return TabUiThemeUtil.getTabStripContainerColor(
                    mContext, mIncognito, foreground, mIsReordering, mIsPlaceholder, hovered);
        }

        if (foreground) {
            return ChromeColors.getDefaultThemeColor(mContext, mIncognito);
        }

        // |defaultColor| represents the color of a background/inactive tab.
        int defaultColor;
        int overlayColor;
        if (mIncognito) {
            defaultColor = mContext.getResources().getColor(
                    R.color.baseline_neutral_10_with_neutral_0_alpha_30);
            overlayColor = mContext.getColor(R.color.baseline_neutral_90);
        } else {
            final int baseColor = ChromeColors.getSurfaceColor(
                    mContext, R.dimen.compositor_background_tab_elevation);
            final float overlayAlpha = ResourcesCompat.getFloat(
                    mContext.getResources(), R.dimen.compositor_background_tab_overlay_alpha);
            defaultColor = ColorUtils.getColorWithOverlay(baseColor, Color.BLACK, overlayAlpha);
            overlayColor = MaterialColors.getColor(mContext, R.attr.colorOnSurface, TAG);
        }

        if (hovered) {
            return ColorUtils.getColorWithOverlay(defaultColor, overlayColor,
                    ResourcesCompat.getFloat(
                            mContext.getResources(), R.dimen.gm2_tab_inactive_hover_alpha));
        }

        return defaultColor;
    }

    /**
     * @param foreground Whether or not this tab is a foreground tab.
     * @return The tint color resource that represents the tab outline.
     */
    public int getOutlineTint(boolean foreground) {
        if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
            // Tabs have no outline in TSR. Return arbitrary color to avoid calculation.
            return Color.TRANSPARENT;
        }

        if (foreground) {
            return getTint(true, false);
        }

        if (mIncognito) {
            return mContext.getResources().getColor(
                    R.color.baseline_neutral_10_with_neutral_0_alpha_30_with_neutral_variant_60_alpha_15);
        }

        final int baseColor = getTint(false, false);
        final int overlayColor = MaterialColors.getColor(mContext, R.attr.colorOutline, TAG);
        final float overlayAlpha = ResourcesCompat.getFloat(
                mContext.getResources(), R.dimen.compositor_background_tab_outline_alpha);
        return ColorUtils.getColorWithOverlay(baseColor, overlayColor, overlayAlpha);
    }

    /**
     * @return The tint color resource for the tab divider.
     */
    public @ColorInt int getDividerTint() {
        if (!ChromeFeatureList.sTabStripRedesign.isEnabled()) {
            // Dividers are only present in TSR. Return arbitrary color to avoid calculation.
            return Color.TRANSPARENT;
        }

        if (mIncognito) {
            return mContext.getColor(R.color.divider_line_bg_color_light);
        }

        if (TabManagementFieldTrial.isTabStripFolioEnabled() && !ColorUtils.inNightMode(mContext)
                && !mIncognito) {
            // This color will not be used at full opacity. We can't set this using the alpha
            // component of the {@code @ColorInt}, since it is ignored when loading resources
            // with a specified tint in the CC layer (instead retaining the alpha of the original
            // image). Instead, this is reflected by setting the opacity of the divider itself.
            // See https://crbug.com/1373634.
            return androidx.core.graphics.ColorUtils.setAlphaComponent(
                    SemanticColorUtils.getDefaultIconColorAccent1(mContext),
                    (int) (DIVIDER_FOLIO_LIGHT_OPACITY * 255));
        }

        return SemanticColorUtils.getDividerLineBgColor(mContext);
    }

    /**
     * @param visible Visibility of tab's start divider.
     */
    public void setStartDividerVisible(boolean visible) {
        mStartDividerVisible = visible;
    }

    /**
     * @return Visibility of tab's start divider.
     */
    public boolean isStartDividerVisible() {
        return mStartDividerVisible;
    }

    /**
     * @param visible Visibility of end divider.
     */
    public void setEndDividerVisible(boolean visible) {
        mEndDividerVisible = visible;
    }

    /**
     * @return Visibility of tab's end divider.
     */
    public boolean isEndDividerVisible() {
        return mEndDividerVisible;
    }

    /**
     * @param visible Whether or not this {@link StripLayoutTab} should be drawn.
     */
    public void setVisible(boolean visible) {
        mVisible = visible;
        if (!visible) {
            mUpdateHost.releaseResourcesForTab(mId);
        }
        for (Observer observer : mObservers) {
            observer.onVisibilityChanged(mVisible);
        }
    }

    /**
     * @return Whether or not this {@link StripLayoutTab} should be drawn.
     */
    public boolean isVisible() {
        return mVisible;
    }

    /**
     * Mark this tab as in the process of dying.  This lets us track which tabs are dead after
     * animations.
     * @param isDying Whether or not the tab is dying.
     */
    public void setIsDying(boolean isDying) {
        mIsDying = isDying;
    }

    /**
     * @return Whether or not the tab is dying.
     */
    public boolean isDying() {
        return mIsDying;
    }

    /**
     * @return Whether or not this tab should be visually represented as loading.
     */
    public boolean isLoading() {
        return mLoadTracker.isLoading();
    }

    /**
     * @return The rotation of the loading spinner in degrees.
     */
    public float getLoadingSpinnerRotation() {
        return mLoadingSpinnerRotationDegrees;
    }

    /**
     * Additive spinner rotation update.
     * @param rotation The amount to rotate the spinner by in degrees.
     */
    public void addLoadingSpinnerRotation(float rotation) {
        mLoadingSpinnerRotationDegrees = (mLoadingSpinnerRotationDegrees + rotation) % 1080;
    }

    /**
     * Called when this tab has started loading.
     */
    public void pageLoadingStarted() {
        mLoadTracker.pageLoadingStarted();
    }

    /**
     * Called when this tab has finished loading.
     */
    public void pageLoadingFinished() {
        mLoadTracker.pageLoadingFinished();
    }

    /**
     * Called when this tab has started loading resources.
     */
    public void loadingStarted() {
        mLoadTracker.loadingStarted();
    }

    /**
     * Called when this tab has finished loading resources.
     */
    public void loadingFinished() {
        mLoadTracker.loadingFinished();
    }

    /**
     * @param brightness The fraction (from 0.f to 1.f) of how bright the tab should be.
     */
    public void setBrightness(float brightness) {
        mBrightness = brightness;
    }

    /**
     * @return The fraction (from 0.f to 1.f) of how bright the tab should be.
     */
    public float getBrightness() {
        return mBrightness;
    }

    /**
     * @param opacity The fraction (from 0.f to 1.f) of how opaque the tab container should be.
     */
    public void setContainerOpacity(float opacity) {
        mContainerOpacity = opacity;
    }

    /**
     * @return The fraction (from 0.f to 1.f) of how opaque the tab container should be.
     */
    public float getContainerOpacity() {
        if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
            return mContainerOpacity;
        } else {
            return 1.f;
        }
    }

    /**
     * @return How far to vertically offset the tab content.
     */
    public float getContentOffsetY() {
        if (TabManagementFieldTrial.isTabStripDetachedEnabled()) {
            return DETACHED_CONTENT_OFFSET_Y - (TOP_MARGIN_DP / 2);
        } else if (TabManagementFieldTrial.isTabStripFolioEnabled()) {
            return FOLIO_CONTENT_OFFSET_Y - (TOP_MARGIN_DP / 2);
        } else {
            // If TSR is disabled, contentOffsetY will not be used. Default to 0.
            return 0.f;
        }
    }

    /**
     * @return The trailing offset for the tab divider.
     */
    public float getDividerOffsetX() {
        return DIVIDER_OFFSET_X;
    }

    /**
     * @param bottomMargin How far to offset the bottom of the tab container from the toolbar.
     */
    public void setBottomMargin(float bottomMargin) {
        mBottomMargin = bottomMargin;
    }

    /**
     * @return How far to offset the bottom of the tab container from the toolbar.
     */
    public float getBottomMargin() {
        return mBottomMargin;
    }

    /**
     * @return How far to offset the top of the tab container from the top of the tab strip.
     */
    public float getTopMargin() {
        return ChromeFeatureList.sTabStripRedesign.isEnabled() ? TOP_MARGIN_DP : 0;
    }

    /**
     * @param show Whether or not the close button is allowed to be shown.
     * @param animate Whether or not to animate the close button showing/hiding.
     */
    public void setCanShowCloseButton(boolean show, boolean animate) {
        mCanShowCloseButton = show;
        checkCloseButtonVisibility(animate);
    }

    /**
     * @param x The actual position in the strip, taking into account stacking, scrolling, etc.
     */
    public void setDrawX(float x) {
        mCloseButton.setX(mCloseButton.getX() + (x - mDrawX));
        mDrawX = x;
        mTouchTarget.left = mDrawX + mLeftInset;
        mTouchTarget.right = mDrawX + mWidth - mRightInset;
    }

    /**
     * @return The actual position in the strip, taking into account stacking, scrolling, etc.
     */
    public float getDrawX() {
        return mDrawX;
    }

    /**
     * @param y The vertical position for the tab.
     */
    public void setDrawY(float y) {
        mCloseButton.setY(mCloseButton.getY() + (y - mDrawY));
        mDrawY = y;
        mTouchTarget.top = mDrawY;
        mTouchTarget.bottom = mDrawY + mHeight;
    }

    /**
     * @return The vertical position for the tab.
     */
    public float getDrawY() {
        return mDrawY;
    }

    /**
     * @param width The width of the tab.
     */
    public void setWidth(float width) {
        mWidth = width;
        resetCloseRect();
        mTouchTarget.right = mDrawX + mWidth - mRightInset;
    }

    /**
     * @return The width of the tab.
     */
    public float getWidth() {
        return mWidth;
    }

    /**
     * @param height The height of the tab.
     */
    public void setHeight(float height) {
        mHeight = height;
        resetCloseRect();
        mTouchTarget.bottom = mDrawY + mHeight;
    }

    /**
     * @return The height of the tab.
     */
    public float getHeight() {
        return mHeight;
    }

    /**
     * @param closePressed The current pressed state of the attached button.
     */
    public void setClosePressed(boolean closePressed, boolean isPressedFromMouse) {
        mCloseButton.setPressed(closePressed, isPressedFromMouse);
    }

    /**
     * @param closeHovered The current hovered state of the attached button.
     */
    public void setCloseHovered(boolean closeHovered) {
        mCloseButton.setHovered(closeHovered);
    }

    /**
     * @return The current hovered state of the close button.
     */
    public boolean isCloseHovered() {
        return mCloseButton.isHovered();
    }

    /**
     * @return The current pressed state of the close button.
     */
    public boolean getClosePressed() {
        return mCloseButton.isPressed();
    }

    /**
     * @return The close button for this tab.
     */
    public TintedCompositorButton getCloseButton() {
        return mCloseButton;
    }

    /**
     * This represents how much this tab's width should be counted when positioning tabs in the
     * stack.  As tabs close or open, their width weight is increased.  They visually take up
     * the same amount of space but the other tabs will smoothly move out of the way to make room.
     * @return The weight from 0 to 1 that the width of this tab should have on the stack.
     */
    public float getWidthWeight() {
        return MathUtils.clamp(1.f - mDrawY / mHeight, 0.f, 1.f);
    }

    /**
     * @param x The x position of the position to test.
     * @param y The y position of the position to test.
     * @return Whether or not {@code x} and {@code y} is over the close button for this tab and
     *         if the button can be clicked.
     */
    public boolean checkCloseHitTest(float x, float y) {
        return mShowingCloseButton
                ? ((CompositorButton) mCloseButton).checkClickedOrHovered(x, y)
                : false;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @param offsetX The offset of the tab (used for drag and drop, slide animating, etc).
     */
    public void setOffsetX(float offsetX) {
        mTabOffsetX = offsetX;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @return The offset of the tab (used for drag and drop, slide animating, etc).
     */
    public float getOffsetX() {
        return mTabOffsetX;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @param x The ideal position, in an infinitely long strip, of this tab.
     */
    public void setIdealX(float x) {
        mIdealX = x;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @return The ideal position, in an infinitely long strip, of this tab.
     */
    public float getIdealX() {
        return mIdealX;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @param offsetY The vertical offset of the tab.
     */
    public void setOffsetY(float offsetY) {
        mTabOffsetY = offsetY;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @return The vertical offset of the tab.
     */
    public float getOffsetY() {
        return mTabOffsetY;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @param trailingMargin The trailing margin of the tab (used for margins around tab groups
     *                       when reordering, etc.).
     */
    public void setTrailingMargin(float trailingMargin) {
        mTrailingMargin = trailingMargin;
    }

    /**
     * This is used to help calculate the tab's position and is not used for rendering.
     * @return The trailing margin of the tab.
     */
    public float getTrailingMargin() {
        return mTrailingMargin;
    }

    /**
     * This is used to determine if the tab is a placeholder or not. If it is a placeholder, it will
     * show as an empty tab on the tab strip (without tab contents, such as title & favicon,
     * generated).
     * @param isPlaceholder Whether or not the tab is a placeholder used on startup.
     */
    public void setIsPlaceholder(boolean isPlaceholder) {
        mIsPlaceholder = isPlaceholder;
        checkCloseButtonVisibility(false);
    }

    /**
     * This is used to determine if the tab is a placeholder or not. If it is a placeholder, it will
     * show as an empty tab on the tab strip (without tab contents, such as title & favicon,
     * generated).
     * @return Whether or not the tab is a placeholder used on startup.
     */
    public boolean getIsPlaceholder() {
        return mIsPlaceholder;
    }

    /**
     * @return The left-side of the tab's touch target.
     */
    public float getTouchTargetLeft() {
        return mTouchTarget.left;
    }

    /**
     * @return The right-side of the tab's touch target.
     */
    public float getTouchTargetRight() {
        return mTouchTarget.right;
    }

    private void resetCloseRect() {
        RectF closeRect = getCloseRect();
        mCloseButton.setWidth(closeRect.width());
        mCloseButton.setHeight(closeRect.height());
        mCloseButton.setX(closeRect.left);
        mCloseButton.setY(closeRect.top);
    }

    private RectF getCloseRect() {
        int closeButtonWidth = CLOSE_BUTTON_WIDTH_DP;
        int closeButtonOffsetX = getCloseButtonOffsetX();
        if (!LocalizationUtils.isLayoutRtl()) {
            mClosePlacement.left = getWidth() - closeButtonWidth - closeButtonOffsetX;
            mClosePlacement.right = mClosePlacement.left + closeButtonWidth;
        } else {
            mClosePlacement.left = closeButtonOffsetX;
            mClosePlacement.right = closeButtonWidth + closeButtonOffsetX;
        }

        mClosePlacement.top = 0;
        mClosePlacement.bottom = getHeight();

        mClosePlacement.offset(getDrawX(), getDrawY());
        return mClosePlacement;
    }

    public int getCloseButtonPadding() {
        return ChromeFeatureList.sTabStripRedesign.isEnabled() ? CLOSE_BUTTON_PADDING_DP : 0;
    }

    public int getCloseButtonOffsetX() {
        return ChromeFeatureList.sTabStripRedesign.isEnabled() ? CLOSE_BUTTON_OFFSET_X : 0;
    }

    // TODO(dtrainor): Don't animate this if we're selecting or deselecting this tab.
    private void checkCloseButtonVisibility(boolean animate) {
        boolean shouldShow = mCanShowCloseButton && !mIsPlaceholder;

        if (shouldShow != mShowingCloseButton) {
            float opacity = shouldShow ? 1.f : 0.f;
            if (animate) {
                if (mButtonOpacityAnimation != null) mButtonOpacityAnimation.end();
                mButtonOpacityAnimation = CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(), mCloseButton, CompositorButton.OPACITY,
                        mCloseButton.getOpacity(), opacity, ANIM_TAB_CLOSE_BUTTON_FADE_MS);
                mButtonOpacityAnimation.addListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mButtonOpacityAnimation = null;
                    }
                });
                mButtonOpacityAnimation.start();
            } else {
                mCloseButton.setOpacity(opacity);
            }
            mShowingCloseButton = shouldShow;
            if (!mShowingCloseButton) mCloseButton.setPressed(false);
        }
    }
}
