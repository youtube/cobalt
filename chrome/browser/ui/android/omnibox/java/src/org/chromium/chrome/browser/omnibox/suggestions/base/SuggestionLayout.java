// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Rect;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Px;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
/**
 * SuggestionLayout is a container aiming to quickly and correctly arrange encompassed items.
 * The operation is similar to that of ConstraintLayout, with the exception that the purpose of
 * every item is known ahead of time.
 * This layout is highly optimized around view types, and bypasses certain measurement calls, where
 * the size of the view is known ahead of time.
 */
class SuggestionLayout extends ViewGroup {
    private final @Px int mDecorationIconWidthPx;
    private final @Px int mActionButtonWidthPx;
    private final @Px int mCompactContentHeightPx;
    private final @Px int mContentHeightPx;
    private final @Px int mContentPaddingPx;

    /**
     * SuggestionLayout's LayoutParams.
     *
     * Additional parameters define the role of an element..
     */
    public static class LayoutParams extends ViewGroup.LayoutParams {
        @IntDef({SuggestionViewType.CONTENT, SuggestionViewType.DECORATION,
                SuggestionViewType.ACTION_BUTTON, SuggestionViewType.FOOTER})
        @Retention(RetentionPolicy.SOURCE)
        /// Defines suggestion building blocks.
        /// The placement of every SuggestionViewType element is as follows:
        ///
        /// +---+------------------+---+---+---+
        /// | 0 | 1                | 2 | 2 | 2 |
        /// +---+------------------+---+---+---+
        /// | 3                                |
        /// +----------------------------------+
        /// | 3                                |
        /// +----------------------------------+
        ///
        /// The lines on the diagram above are for reference purpose to outline the boundaries
        /// of individual suggestion view types.
        ///
        /// Unless otherwise specified, focus ripples through all suggestion views.
        /// To override this behavior, individual views need to override their background.
        public @interface SuggestionViewType {
            /// Main content, encompassing one or more lines of text. Must be horizontally and
            /// vertically resizable.
            /// Only a single CONTENT view is permitted right now.
            int CONTENT = 0;
            /// An image presented on the left hand side of the CONTENT, such as an icon
            /// (magnifying glass, globe), site favicon, solid color or image.
            /// Multiple DECORATION buttons are presently not permitted.
            int DECORATION = 1;
            /// An action button presented on the right hand side of the CONTENT. Buttons are
            /// aligned to the top/end edge of the suggestion, and build towards the beginning
            /// of the content view (left in LTR, or right in RTL layout directions).
            /// Multiple ACTION_BUTTON elements are permitted.
            int ACTION_BUTTON = 2;
            /// FOOTER element is always added at the bottom of the suggestion, and stretches from
            /// the beginning to the end of the entire suggestion view.
            /// Multiple FOOTER elements will be stacked one on top of another.
            int FOOTER = 3;
        }

        /// The role of the associated view in the SuggestionView.
        private final @SuggestionViewType int mSuggestionViewType;
        private final @NonNull Rect mPlacement;

        public LayoutParams(int width, int height, @SuggestionViewType int type) {
            super(width, height);
            mPlacement = new Rect();
            mSuggestionViewType = type;
        }

        /**
         * Create LayoutParams for particular SuggestionViewType.
         */
        public static LayoutParams forViewType(@SuggestionViewType int type) {
            return new LayoutParams(WRAP_CONTENT, WRAP_CONTENT, type);
        }

        /** @return The role of the view. */
        private @SuggestionViewType int getViewType() {
            return mSuggestionViewType;
        }

        /** @return The placement of the view, relative to Suggestion area start. */
        private @NonNull Rect getPlacement() {
            return mPlacement;
        }

        /**
         * Specify the position of the view relative to the SuggestionLayout's Top/Start corner.
         *
         * Placement is LayoutDirection agnostic. Callers should assume that the offsets are
         * relative to the start position of the view, and sizes expand in the direction appropriate
         * for the currently used LayoutDirection.
         *
         * @param start The offset from the start edge (left for LTR layouts, right for RTL
         *         layouts) of the SuggestionLayout.
         * @param top The offset from the top edge of the SuggestionLayout.
         * @param width The width of the view (expands to the right for LTR layouts, and to
         *         the left for RTL layouts).
         * @param height The height of the view.
         */
        private void setPlacement(int start, int top, int width, int height) {
            mPlacement.set(start, top, start + width, top + height);
        }
    }

    public SuggestionLayout(Context context) {
        super(context);

        boolean useModernUI = OmniboxFeatures.shouldShowModernizeVisualUpdate(getContext());

        if (!useModernUI) {
            // Align the Action Buttons with the Omnibox icons (clear, mic etc).
            int startSpace =
                    getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_start_padding);
            int endSpace =
                    getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_end_padding);
            setPaddingRelative(startSpace, 0, endSpace, 0);
        }

        mDecorationIconWidthPx = getResources().getDimensionPixelSize(useModernUI
                        ? R.dimen.omnibox_suggestion_icon_area_size_modern
                        : R.dimen.omnibox_suggestion_icon_area_size);

        mActionButtonWidthPx = getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_action_button_width);

        mCompactContentHeightPx = getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_compact_content_height);
        mContentHeightPx =
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_content_height);
        mContentPaddingPx =
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_content_padding);
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        // The only measure spec we know is the WIDTH of the suggestion and padding around the
        // content.
        var suggestionWidthPx =
                MeasureSpec.getSize(widthSpec) - getPaddingLeft() - getPaddingRight();

        // First, compute the width of the content area.
        // We know the size of every DECORATION and ACTION_BUTTON, which surround the CONTENT.
        var measuredContentWidthPx = measureContentViewsWidthPx(suggestionWidthPx);

        // Next, compute the height of the CONTENT area. CONTENT may hold multiple lines of wrapped
        // text, such as dictionary entries (type: "define dictionary" for an example entry).
        // As such, these views may have varying heights.
        var measuredContentHeightPx = measureContentViewHeightPx(measuredContentWidthPx);

        // Now that we know how tall the CONTENT area is, apply that measurement to the views
        // surrounding the CONTENT area. This will permit proper content scaling and placement.
        measureDecorationIconAndActionButtons(measuredContentWidthPx, measuredContentHeightPx);

        // Compute the height of all FOOTER views, such as Action Chips (type: "play dino game"
        // to see example action chip).
        // Multiple footers are permitted and will stack on top of each other.
        // Note that, unlike CONTENT, FOOTER views are not surrounded by DECORATION or ACTION
        // BUTTON views.
        var measuredFooterHeightPx = measureFooterViewsHeightPx(suggestionWidthPx);

        // Finally, compute the placements of every view relative to the START of the view,
        // including our own.
        // Note that while START means different things for RTL and LTR layout directions, these
        // offsets are RELATIVE, and will expand in appropriate direction during Layout phase.
        applySuggestionViewPlacements(
                suggestionWidthPx, measuredContentWidthPx, measuredContentHeightPx);
        setMeasuredDimension(widthSpec,
                MeasureSpec.makeMeasureSpec(measuredContentHeightPx + measuredFooterHeightPx
                                + getPaddingTop() + getPaddingBottom(),
                        MeasureSpec.EXACTLY));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        var topPx = getPaddingTop();
        var layoutDirectionRTL = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        var startPx = layoutDirectionRTL ? right - left - getPaddingRight() : getPaddingLeft();

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();

            // All our placements are relative to the view START:
            // - the LEFT edge, when the layout direction is LTR,
            // - the RIGHT edge, when the layout direction is RTL.
            // and grow in the direction appropriate for the layout direction.
            // - to the RIGHT, when the layout direction is LTR,
            // - to the LEFT, when the layout direction is RTL.
            var placement = params.getPlacement();
            if (layoutDirectionRTL) {
                view.layout(startPx - placement.right, topPx + placement.top,
                        startPx - placement.left, topPx + placement.bottom);
            } else {
                view.layout(startPx + placement.left, topPx + placement.top,
                        startPx + placement.right, topPx + placement.bottom);
            }
        }
    }

    /**
     * Given the SuggestionView width, compute the width available to the CONTENT views.
     * CONTENT views are surrounded by DECORATION and ACTION_BUTTON view types.
     *
     * NOTE: this is invoked directly by the measure cycle. Do not allocate memory, and avoid any
     * calls that trigger measurement invalidation and restart.
     *
     * @param suggestionWidthPx The width of the suggestion content area
     * @return The computed width of the CONTENT views.
     */
    private @Px int measureContentViewsWidthPx(@Px int suggestionWidthPx) {
        // Always reserve space for the decoration view.
        // Even if we don't have one, it's better to leave a gap, than render misaligned
        // suggestions.
        var contentWidthPx = suggestionWidthPx - mDecorationIconWidthPx;

        // Measure all other views surrounding the CONTENT area. Currently these are only
        // ACTION_BUTTONs.
        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == LayoutParams.SuggestionViewType.ACTION_BUTTON) {
                contentWidthPx -= mActionButtonWidthPx;
            }
        }
        return contentWidthPx;
    }

    /**
     * Given the CONTENT width, compute the height of the CONTENT view.
     *
     * NOTE: this is invoked directly by the measure cycle. Do not allocate memory, and avoid any
     * calls that trigger measurement invalidation and restart.
     *
     * @param contentWidthPx The width of the CONTENT view.
     * @return The measured height of the CONTENT view, no less than the minimum size.
     */
    private @Px int measureContentViewHeightPx(@Px int contentWidthPx) {
        int contentHeightPx = 0;
        boolean hasFooter = false;

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == LayoutParams.SuggestionViewType.CONTENT) {
                assert contentHeightPx == 0 : "Content view already defined";
                // Content views' width is constrained by how much space the decoration views
                // allocate. These views may, as a result, wrap around to one or more extra lines of
                // text.
                view.measure(MeasureSpec.makeMeasureSpec(contentWidthPx, MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                contentHeightPx = view.getMeasuredHeight();
            } else if (params.getViewType() == LayoutParams.SuggestionViewType.FOOTER) {
                hasFooter = true;
            }
        }

        // It is possible for the measured CONTENT area to be smaller than our minimum
        // suggestion height. Apply necessary corrections here. We currently expect the
        // CONTENT view to properly utilize the LAYOUT GRAVITY to position its content
        // around the center, if its measured height is smaller than our minimum.
        assert contentHeightPx != 0 : "No content views, or content view is empty";

        // Pad suggestion around to guarantee appropriate spacing around suggestions.
        if (!hasFooter) contentHeightPx += mContentPaddingPx;

        // Guarantee that the suggestion height meets our required minimum tap target size.
        return Math.max(contentHeightPx, hasFooter ? mCompactContentHeightPx : mContentHeightPx);
    }

    /**
     * Given the SuggestionView width, compute the height of all FOOTER views.
     *
     * NOTE: this is invoked directly by the measure cycle. Do not allocate memory, and avoid any
     * calls that trigger measurement invalidation and restart.
     *
     * @param suggestionWidthPx The width of the suggestion content area
     * @return The cumulative height of the FOOTER views.
     */
    private @Px int measureFooterViewsHeightPx(@Px int suggestionWidthPx) {
        int footerHeightPx = 0;

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            if (params.getViewType() == LayoutParams.SuggestionViewType.FOOTER) {
                // FOOTER views stretch from one end of the Suggestion view to the other.
                // Unlike CONTENT views, FOOTERs are not surrounded by DECORATION or ACTION_BUTTON
                // views.
                view.measure(MeasureSpec.makeMeasureSpec(suggestionWidthPx, MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
                footerHeightPx += view.getMeasuredHeight();
            }
        }

        return footerHeightPx;
    }

    /**
     * Given the CONTENT area dimensions, apply measurements and placement of all elements
     * surrounding the CONTENT view (currently: DECORATION and ACTION_BUTTONs).
     *
     * NOTE: this is invoked directly by the measure cycle. Do not allocate memory, and avoid any
     * calls that trigger measurement invalidation and restart.
     *
     * @param contentWidthPx The width of the CONTENT area.
     * @param contentHeightPx The height of the CONTENT area.
     */
    private void measureDecorationIconAndActionButtons(
            @Px int contentWidthPx, @Px int contentHeightPx) {
        var contentHeightSpec = MeasureSpec.makeMeasureSpec(contentHeightPx, MeasureSpec.AT_MOST);
        var computedWidthPx = contentWidthPx;
        var nextActionButtonStartPx = contentWidthPx + mDecorationIconWidthPx;

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();

            // Capture the measure spec of the area available to DECORATION and ACTION_BUTTONs.
            // Note that at this stage everything else has already been measured.
            var viewWidthSpec = 0;
            if (params.getViewType() == LayoutParams.SuggestionViewType.DECORATION) {
                viewWidthSpec =
                        MeasureSpec.makeMeasureSpec(mDecorationIconWidthPx, MeasureSpec.EXACTLY);
            } else if (params.getViewType() == LayoutParams.SuggestionViewType.ACTION_BUTTON) {
                viewWidthSpec =
                        MeasureSpec.makeMeasureSpec(mActionButtonWidthPx, MeasureSpec.EXACTLY);
            } else {
                continue;
            }

            view.measure(viewWidthSpec, getChildMeasureSpec(contentHeightSpec, 0, params.height));
        }
    }

    /**
     * Apply placements to all the views.
     *
     * The views are placed linearly, offering offset from the START of the encompassing view's
     * padded area. The concept of LayoutDirection is irrelevant at this point for simplicity, and
     * can be assumed "any": the placements computed here will expand views
     * - to the RIGHT, when the layout direction is LTR, and
     * - to the LEFT, when the layout direction ir RTL.
     *
     * @param suggestionWidthPx The width of the Suggestion area.
     * @param contentWidthPx The width of the CONTENT area.
     * @param contentHeightPx The height of the CONTENT area.
     */
    private void applySuggestionViewPlacements(
            @Px int suggestionWidthPx, @Px int contentViewsWidth, @Px int contentViewHeight) {
        var nextActionButtonStartPx = mDecorationIconWidthPx + contentViewsWidth;
        var nextFooterViewTopPx = contentViewHeight;

        for (int index = 0; index < getChildCount(); ++index) {
            var view = getChildAt(index);
            if (view.getVisibility() == GONE) continue;

            var params = (LayoutParams) view.getLayoutParams();
            switch (params.getViewType()) {
                case LayoutParams.SuggestionViewType.DECORATION:
                    // DECORATION space is square, but the image inside does not have to be.
                    // Retrieve the information about target image height and center it with the
                    // CONTENT.
                    var decorationWidth = view.getMeasuredWidth();
                    var decorationHeight = view.getMeasuredHeight();
                    var decorationLeft = (mDecorationIconWidthPx - decorationWidth) / 2;
                    var decorationTop = (contentViewHeight - decorationHeight) / 2;
                    params.setPlacement(
                            decorationLeft, decorationTop, decorationWidth, decorationHeight);
                    break;

                case LayoutParams.SuggestionViewType.CONTENT:
                    params.setPlacement(
                            mDecorationIconWidthPx, 0, contentViewsWidth, contentViewHeight);
                    break;

                case LayoutParams.SuggestionViewType.ACTION_BUTTON:
                    params.setPlacement(
                            nextActionButtonStartPx, 0, mActionButtonWidthPx, contentViewHeight);
                    // Horizontally line up ACTION_BUTTONs.
                    nextActionButtonStartPx += mActionButtonWidthPx;
                    break;

                case LayoutParams.SuggestionViewType.FOOTER:
                    var footerViewHeight = view.getMeasuredHeight();
                    params.setPlacement(
                            0, nextFooterViewTopPx, suggestionWidthPx, footerViewHeight);
                    // Vertically Stack FOOTERs.
                    nextFooterViewTopPx += footerViewHeight;
                    break;
            }
        }
    }
}
