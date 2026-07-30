// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.WeakHashMap;

/** Draws a vertical line to the side of tab items that belong to a tab group. */
@NullMarked
class VerticalTabGroupSpineDecoration extends RecyclerView.ItemDecoration {
    private final TabListModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final Runnable mInvalidationTrigger;
    private final Map<View, GroupInfo> mViewToGroupInfo = new WeakHashMap<>();
    private final Set<Token> mDrawnGroupIds = new HashSet<>();
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF mRectF = new RectF();
    private final int mSpineRadius;
    private final float mSpineWidth;
    private final int mMarginBottom;

    private @Nullable TabModel mCurrentTabModel;

    private final TabGroupObserver mTabGroupObserver =
            new TabGroupObserver() {
                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    mInvalidationTrigger.run();
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

        int childCount = parent.getChildCount();
        mDrawnGroupIds.clear();

        // Cache group IDs for live views. When a tab is closed via 'X', RecyclerView detaches it
        // (adapter position becomes NO_POSITION) during its exit animation. This map remembers
        // which group the fading view belonged to so the spine curve wraps it until it vanishes.
        for (int i = 0; i < childCount; i++) {
            View v = parent.getChildAt(i);
            int pos = parent.getChildAdapterPosition(v);
            if (pos != RecyclerView.NO_POSITION && pos < mModel.size()) {
                PropertyModel model = mModel.get(pos).model;
                Token headerId = model.get(TabProperties.TAB_GROUP_HEADER_ID);
                Token groupId = headerId != null ? headerId : model.get(TabProperties.TAB_GROUP_ID);
                if (groupId != null) {
                    mViewToGroupInfo.put(
                            v,
                            new GroupInfo(
                                    groupId,
                                    headerId != null,
                                    model.get(TabProperties.IS_COLLAPSED)));
                } else {
                    mViewToGroupInfo.remove(v);
                }
            }
        }

        for (int i = 0; i < childCount; i++) {
            View child = parent.getChildAt(i);
            int pos = parent.getChildAdapterPosition(child);
            if (pos == RecyclerView.NO_POSITION || pos >= mModel.size()) continue;

            GroupInfo info = mViewToGroupInfo.get(child);
            if (info == null
                    || (info.isHeader && info.isCollapsed)
                    || !mDrawnGroupIds.add(info.groupId)) {
                continue;
            }

            float top = calculateTop(child, info.isHeader);
            float bottom = calculateBottom(parent, childCount, child, i, info.groupId);

            if (bottom <= top) continue;

            PropertyModel model = mModel.get(pos).model;
            @Nullable Integer cardColorId = model.get(TabProperties.TAB_GROUP_CARD_COLOR);
            int colorId =
                    cardColorId != null
                            ? cardColorId
                            : tabModel.getTabGroupColorWithFallback(info.groupId);
            int color =
                    TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                            parent.getContext(), colorId, isIncognito);
            mPaint.setColor(color);
            mPaint.setAlpha(Math.round(child.getAlpha() * 255f));

            mRectF.set(left, top, right, bottom);
            c.drawRoundRect(mRectF, mSpineRadius, mSpineRadius, mPaint);
        }
    }

    public void destroy() {
        mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeTabGroupObserver(mTabGroupObserver);
            mCurrentTabModel = null;
        }
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

    private float calculateTop(View child, boolean isHeader) {
        if (!isHeader) {
            // For a normal child tab (meaning the group header scrolled off-screen),
            // start the vertical line directly from the top edge of this card.
            return child.getTop() + child.getTranslationY();
        }

        // For a group header, start the line right below the header card (plus trailing margin).
        return child.getBottom() + child.getTranslationY() + mMarginBottom;
    }

    private float calculateBottom(
            RecyclerView parent, int childCount, View child, int childIndex, Token groupId) {
        float totalGroupHeight = child.getHeight() + mMarginBottom;
        float renderedBottom = child.getBottom() + child.getTranslationY();

        // We scan downstream visible siblings. For cards belonging to our group, we track:
        // 1. renderedBottom: Deepest physical GPU render boundary (including closing/fading cards),
        // for shrinking smoothly when tabs close.
        // 2. totalGroupHeight: Aggregate un-animated structural layout height of surviving cards,
        // for holding spine steady while dragging cards.
        for (int j = childIndex + 1; j < childCount; j++) {
            View nextView = parent.getChildAt(j);

            // Break early if we hit a different group or unassigned card
            GroupInfo nextInfo = mViewToGroupInfo.get(nextView);
            if (nextInfo == null || !groupId.equals(nextInfo.groupId)) break;

            float itemBottom = nextView.getBottom() + nextView.getTranslationY();
            if (itemBottom > renderedBottom) renderedBottom = itemBottom;

            if (parent.getChildAdapterPosition(nextView) != RecyclerView.NO_POSITION) {
                totalGroupHeight += nextView.getHeight() + mMarginBottom;
            }
        }

        float staticBottom =
                child.getTop() + child.getTranslationY() + totalGroupHeight - mMarginBottom;
        return Math.max(staticBottom, renderedBottom);
    }

    private static class GroupInfo {
        public final Token groupId;
        public final boolean isHeader;
        public final boolean isCollapsed;

        public GroupInfo(Token groupId, boolean isHeader, boolean isCollapsed) {
            this.groupId = groupId;
            this.isHeader = isHeader;
            this.isCollapsed = isCollapsed;
        }
    }
}
