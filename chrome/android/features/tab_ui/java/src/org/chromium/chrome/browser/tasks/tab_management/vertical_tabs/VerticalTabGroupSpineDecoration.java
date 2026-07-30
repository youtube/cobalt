// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListItemAnimator;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Draws a vertical line to the side of tab items that belong to a tab group. */
@NullMarked
class VerticalTabGroupSpineDecoration extends RecyclerView.ItemDecoration {
    // TabListItemAnimator sequences removals before moves, so the spine's collapse duration
    // must account for both.
    private static final long COLLAPSE_DURATION_MS =
            TabListItemAnimator.DEFAULT_REMOVE_DURATION + TabListItemAnimator.DEFAULT_MOVE_DURATION;

    private final TabListModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final Runnable mInvalidationTrigger;
    private final Set<Token> mDrawnGroupIds = new HashSet<>();
    private final Set<Token> mCollapsingGroupIds = new HashSet<>();
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF mRectF = new RectF();
    private final int mSpineRadius;
    private final float mSpineWidth;
    private final int mMarginBottom;
    private final List<View> mSortedChildren = new ArrayList<>();
    private final ChildPositionComparator mChildPositionComparator = new ChildPositionComparator();

    private @Nullable TabModel mCurrentTabModel;

    private final TabGroupObserver mTabGroupObserver =
            new TabGroupObserver() {
                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    mInvalidationTrigger.run();
                }

                @Override
                public void didChangeTabGroupCollapsed(
                        Token tabGroupId, boolean isCollapsed, boolean animate) {
                    if (isCollapsed) {
                        mCollapsingGroupIds.add(tabGroupId);
                        mHandler.postDelayed(
                                () -> {
                                    mCollapsingGroupIds.remove(tabGroupId);
                                    mInvalidationTrigger.run();
                                },
                                COLLAPSE_DURATION_MS);
                    }
                }
            };

    /**
     * Constructor for {@link VerticalTabGroupSpineDecoration}.
     *
     * @param context The {@link Context} used to retrieve dimension resources.
     * @param invalidationTrigger A {@link Runnable} triggered to invalidate the recycler view.
     * @param model The {@link TabListModel} containing tab properties.
     * @param tabModelSelector The {@link TabModelSelector} used to fetch tab models.
     */
    public VerticalTabGroupSpineDecoration(
            Context context,
            Runnable invalidationTrigger,
            TabListModel model,
            TabModelSelector tabModelSelector) {
        mInvalidationTrigger = invalidationTrigger;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        var res = context.getResources();
        mSpineWidth = res.getDimensionPixelSize(R.dimen.vertical_tab_spine_width);
        mSpineRadius = res.getDimensionPixelSize(R.dimen.vertical_tab_spine_radius);
        mMarginBottom = res.getDimensionPixelSize(R.dimen.vertical_tab_item_margin_bottom);

        mCurrentTabModelObserver = this::onCurrentTabModelChanged;
        mTabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndCallIfNonNull(mCurrentTabModelObserver);
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel == null || mModel.isEmpty()) return;

        boolean isIncognito = tabModel.isIncognitoBranded();
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        float left;
        float right;
        if (isRtl) {
            right = parent.getWidth() - parent.getPaddingRight();
            left = right - mSpineWidth;
        } else {
            left = parent.getPaddingLeft();
            right = left + mSpineWidth;
        }

        mDrawnGroupIds.clear();
        boolean anyTabBeingDragged = sortAndCheckDragging(parent);

        float deferredTop = 0;
        float deferredBottom = 0;
        int deferredColor = 0;
        boolean hasDeferredSpine = false;

        int sortedChildCount = mSortedChildren.size();
        for (int i = 0; i < sortedChildCount; i++) {
            View child = mSortedChildren.get(i);
            int pos = parent.getChildAdapterPosition(child);

            PropertyModel childModel = mModel.get(pos).model;
            Token headerId = childModel.get(TabProperties.TAB_GROUP_HEADER_ID);
            boolean isHeader = headerId != null;
            Token groupId = isHeader ? headerId : childModel.get(TabProperties.TAB_GROUP_ID);
            if (groupId == null || !mDrawnGroupIds.add(groupId)) {
                continue;
            }

            boolean isCollapsed = isHeader && childModel.get(TabProperties.IS_COLLAPSED);
            float top = calculateTop(parent, sortedChildCount, i, groupId, isHeader);
            float bottom =
                    calculateBottom(
                            parent, sortedChildCount, i, groupId, isCollapsed, anyTabBeingDragged);
            if (bottom <= top) continue;

            @Nullable Integer cardColorId = childModel.get(TabProperties.TAB_GROUP_CARD_COLOR);
            int colorId =
                    cardColorId != null
                            ? cardColorId
                            : tabModel.getTabGroupColorWithFallback(groupId);
            int color =
                    TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                            parent.getContext(), colorId, isIncognito);

            boolean isDragging = isHeader && childModel.get(TabProperties.IS_BEING_DRAGGED);
            if (isDragging) {
                deferredTop = top;
                deferredBottom = bottom;
                deferredColor = color;
                hasDeferredSpine = true;
                continue;
            }

            drawSpine(c, left, top, right, bottom, color);
        }

        if (hasDeferredSpine) {
            drawSpine(c, left, deferredTop, right, deferredBottom, deferredColor);
        }
    }

    public void destroy() {
        mHandler.removeCallbacksAndMessages(null);
        mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeTabGroupObserver(mTabGroupObserver);
            mCurrentTabModel = null;
        }
    }

    private void drawSpine(Canvas c, float left, float top, float right, float bottom, int color) {
        mPaint.setColor(color);
        mRectF.set(left, top, right, bottom);
        c.drawRoundRect(mRectF, mSpineRadius, mSpineRadius, mPaint);
    }

    private void onCurrentTabModelChanged(@Nullable TabModel tabModel) {
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeTabGroupObserver(mTabGroupObserver);
        }
        mCurrentTabModel = tabModel;
        if (mCurrentTabModel != null) {
            mCurrentTabModel.addTabGroupObserver(mTabGroupObserver);
        }
    }

    /**
     * Collects and sorts the visible child views of the recycler view.
     *
     * @param parent The recycler view containing child views.
     * @return True if any of the collected visible tabs are currently being dragged.
     */
    private boolean sortAndCheckDragging(RecyclerView parent) {
        boolean anyVisibleTabDragging = false;
        mSortedChildren.clear();
        int childCount = parent.getChildCount();
        for (int i = 0; i < childCount; i++) {
            View child = parent.getChildAt(i);
            int pos = parent.getChildAdapterPosition(child);
            if (pos != RecyclerView.NO_POSITION && pos < mModel.size()) {
                mSortedChildren.add(child);

                // Check if this child is being dragged
                PropertyModel model = mModel.get(pos).model;
                if (model != null && model.get(TabProperties.IS_BEING_DRAGGED)) {
                    anyVisibleTabDragging = true;
                }
            }
        }

        // RecyclerView heavily recycles and reorders views during fast scrolling and
        // group expand/collapse animations. Because of this, `parent.getChildAt(i)`
        // often returns views out of their logical adapter order (e.g. newly bound
        // child tabs are appended to the end of the ViewGroup). We must sort the
        // attached views by their adapter position to guarantee we can accurately
        // identify the "first" and "last" visible tabs for each group and properly
        // connect the spine to subsequent items.
        mChildPositionComparator.setParent(parent);
        mSortedChildren.sort(mChildPositionComparator);
        mChildPositionComparator.setParent(null);
        return anyVisibleTabDragging;
    }

    private float calculateTop(
            RecyclerView parent,
            int sortedChildCount,
            int childIndex,
            Token groupId,
            boolean isHeader) {
        View child = mSortedChildren.get(childIndex);
        if (isHeader) {
            PropertyModel model = mModel.get(parent.getChildAdapterPosition(child)).model;

            boolean isGroupBeingDragged =
                    model != null && model.get(TabProperties.IS_BEING_DRAGGED);
            View firstGroupView = null;
            int nextIndex = childIndex + 1;
            if (nextIndex < sortedChildCount) {
                View v = mSortedChildren.get(nextIndex);
                int pos = parent.getChildAdapterPosition(v);
                Token nextTabId = mModel.get(pos).model.get(TabProperties.TAB_GROUP_ID);
                if (groupId.equals(nextTabId)) {
                    firstGroupView = v;
                }
            }

            if (isGroupBeingDragged && firstGroupView != null) {
                // TODO(b/521987032): Remove this - check why header's translationY is not stable
                // when dragging the whole group, so that we have to use firstGroupView's. Maybe due
                // to early return in VerticalTabListItemTouchHelperCallback#onChildDraw, the
                // viewHolder's end animation is not ended?
                // TODO(b/521987032): check why firstGroupView's translationY is not stable when
                // dragging an internal tab in this tab group
                return firstGroupView.getTop() + firstGroupView.getTranslationY();
            } else {
                // For a group header, start the line right below the header card (plus trailing
                // margin).
                return child.getBottom() + child.getTranslationY() + mMarginBottom;
            }
        }
        // For a normal child tab (meaning the group header scrolled off-screen),
        // start the vertical line directly from the top edge of this card.
        return child.getTop() + child.getTranslationY();
    }

    private float calculateBottom(
            RecyclerView parent,
            int sortedChildCount,
            int childIndex,
            Token groupId,
            boolean isCollapsed,
            boolean anyTabBeingDragged) {
        View child = mSortedChildren.get(childIndex);
        View lastGroupView = child;
        View lastStableGroupView = child;
        View nextSiblingView = null;
        boolean isChildBeingDragged = false;

        // Scan subsequent visible items in the sorted array to find:
        // 1. lastGroupView: The last visible child tab belonging to this group.
        // 2. lastStableGroupView: The last fully visible tab, for expanding/adding tab animation if
        // the tab group is the last one on the screen
        // 3. isChildBeingDragged: Whether any of the child is being dragged
        // 4. nextSiblingView: The first visible tab belonging to a DIFFERENT group.
        for (int j = childIndex + 1; j < sortedChildCount; j++) {
            View v = mSortedChildren.get(j);
            PropertyModel nextModel = mModel.get(parent.getChildAdapterPosition(v)).model;
            Token nextTabId =
                    (nextModel == null) ? null : nextModel.get(TabProperties.TAB_GROUP_ID);
            if (groupId.equals(nextTabId)) {
                if (v.getAlpha() >= 1f) {
                    lastStableGroupView = v;
                }
                lastGroupView = v;
                isChildBeingDragged =
                        isChildBeingDragged || nextModel.get(TabProperties.IS_BEING_DRAGGED);
            } else {
                nextSiblingView = v;
                break;
            }
        }

        // Case 1: Fully collapsed or actively collapsing group.
        if (isCollapsed) {
            // If this group is in mCollapsingGroupIds, it is actively collapsing. Connect its spine
            // bottom to the next group.
            if (mCollapsingGroupIds.contains(groupId) && nextSiblingView != null) {
                return nextSiblingView.getTop() + nextSiblingView.getTranslationY() - mMarginBottom;
            }
            // If it's fully collapsed, do not draw spine
            return Integer.MIN_VALUE;
        }

        // Case 2: Middle group with items below it.
        // When static (no tabs are being dragged, or only the internal tab is being dragged),
        // connect the spine bottom to the top of the next group to eliminate sub-pixel gaps.
        if ((!anyTabBeingDragged || isChildBeingDragged) && nextSiblingView != null) {
            return nextSiblingView.getTop() + nextSiblingView.getTranslationY() - mMarginBottom;
        }

        // Case 3: The tab group is being dragged, or if there are more items in the adapter that
        // are being pushed off-screen,  the spine should stick to the bottom without shrinking/any
        // animation.
        float targetBottom = lastGroupView.getBottom() + lastGroupView.getTranslationY();

        int lastPos = parent.getChildAdapterPosition(lastGroupView);
        boolean hasSubsequentItems = lastPos < mModel.size() - 1;
        if (anyTabBeingDragged || hasSubsequentItems) {
            return targetBottom;
        }

        // Case 4: The last group shown on the list.
        // TODO(b/521987032): check why lastGroupView's translationY is not stable when dragging an
        // internal tab in this tab group
        float startBottom = lastStableGroupView.getBottom() + lastStableGroupView.getTranslationY();
        float t = lastGroupView.getAlpha();
        return startBottom + ((targetBottom - startBottom) * t);
    }

    private static class ChildPositionComparator implements Comparator<View> {
        private @Nullable RecyclerView mParent;

        public void setParent(@Nullable RecyclerView parent) {
            mParent = parent;
        }

        @Override
        public int compare(View v1, View v2) {
            assert mParent != null;
            return Integer.compare(
                    mParent.getChildAdapterPosition(v1), mParent.getChildAdapterPosition(v2));
        }
    }
}
