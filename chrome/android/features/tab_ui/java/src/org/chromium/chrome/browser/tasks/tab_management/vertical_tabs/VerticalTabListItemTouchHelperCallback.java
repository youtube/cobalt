// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListItemTouchHelperCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

import java.util.function.Supplier;

/**
 * A {@link TabListItemTouchHelperCallback} implementation to host the logic for swipe and drag
 * related actions in vertical tab list layout.
 */
@NullMarked
public class VerticalTabListItemTouchHelperCallback extends TabListItemTouchHelperCallback {
    /**
     * @param context The Android context.
     * @param model The {@link TabListModel} for the tab list.
     * @param currentTabModelSupplier Supplier for the current {@link TabModel}.
     */
    public VerticalTabListItemTouchHelperCallback(
            Context context, TabListModel model, Supplier<TabModel> currentTabModelSupplier) {
        super(context, model, currentTabModelSupplier);
    }

    /**
     * Returns the movement flags for the given view holder. Regular and child tabs can move
     * vertically, while pinned tabs can move horizontally as well.
     */
    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        if (!hasTabPropertiesModel(viewHolder)) return 0;

        // Regular and child tabs can move vertically.
        int dragFlags = ItemTouchHelper.UP | ItemTouchHelper.DOWN;
        // Pinned tabs can also move horizontally.
        if (isPinnedRegularTab(viewHolder)) {
            dragFlags |= ItemTouchHelper.LEFT | ItemTouchHelper.RIGHT;
        }
        return makeMovementFlags(dragFlags, 0);
    }

    /**
     * Checks whether a dragged tab can be dropped over a target tab. Prevents drops across pinned
     * and unpinned boundaries.
     */
    @Override
    public boolean canDropOver(
            RecyclerView recyclerView,
            RecyclerView.ViewHolder current,
            RecyclerView.ViewHolder target) {
        if (!hasTabPropertiesModel(target)) {
            return false;
        }

        // A pinned tab cannot be dropped in the unpinned section and vice versa.
        if (isPinnedRegularTab(current) != isPinnedRegularTab(target)) {
            return false;
        }
        return super.canDropOver(recyclerView, current, target);
    }

    /**
     * Called when a tab is moved. Updates the underlying {@link TabModel} to reflect the
     * reordering.
     */
    @Override
    public boolean onMove(
            RecyclerView recyclerView,
            RecyclerView.ViewHolder fromViewHolder,
            RecyclerView.ViewHolder toViewHolder) {
        if (!hasTabPropertiesModel(fromViewHolder) || !hasTabPropertiesModel(toViewHolder)) {
            return false;
        }

        int currentTabId = getTabId(fromViewHolder);
        int destinationTabId = getTabId(toViewHolder);

        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabModel == null) return false;

        // Calculate destination index and restrict it based on pinning boundaries.
        int destinationIndex = tabModel.indexOf(tabModel.getTabById(destinationTabId));
        destinationIndex = adjustIndexBasedOnPinning(tabModel, currentTabId, destinationIndex);

        // Track the current UI position to correctly clean up visual selection on drop
        mSelectedTabIndex = toViewHolder.getBindingAdapterPosition();

        // Perform basic list reordering by updating the TabModel immediately.
        // - Standalone tabs & group headers use moveRelatedTabs() to fire didMoveTabGroup(),
        //   which TabListMediator observes to update top-level UI rows.
        // - Child tabs use moveTab() because they move within their group, firing
        //   didMoveWithinGroup() which TabListMediator observes.
        boolean isGroupHeader = fromViewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP;
        Tab currentTab = tabModel.getTabById(currentTabId);
        boolean isStandaloneTab = currentTab != null && currentTab.getTabGroupId() == null;

        if (isGroupHeader || isStandaloneTab) {
            // TODO(crbug.com/518307037): Needs to handle grouping when a standalone tab is dragged
            // into the index span
            // of an expanded group.
            tabModel.moveRelatedTabs(currentTabId, destinationIndex);
        } else {
            // TODO(crbug.com/518307037): Needs to handle ungrouping when a child tab is dragged
            // outside
            // its parent group's boundaries.
            tabModel.moveTab(currentTabId, destinationIndex);
        }
        return true;
    }

    /** Called when a tab is swiped. Swiping is not supported for vertical tabs. */
    @Override
    public void onSwiped(RecyclerView.ViewHolder viewHolder, int direction) {
        // Empty/default
    }

    /**
     * Returns whether long press to drag is enabled. Disabled for mouse input to allow instant
     * dragging.
     */
    @Override
    public boolean isLongPressDragEnabled() {
        return !mIsMouseInputSource;
    }

    /**
     * Called when the selected state of a tab changes, such as when dragging starts or stops.
     * Updates the visual state and tab model selection.
     */
    @Override
    public void onSelectedChanged(RecyclerView.@Nullable ViewHolder viewHolder, int actionState) {
        super.onSelectedChanged(viewHolder, actionState);

        // TODO(crbug.com/518307037): Check for a TAB_GROUP header, cache its IS_COLLAPSED state
        // and:
        // (a) Trigger a collapse if needed when a drag starts.
        // (b) Trigger an expand if needed when dropped.

        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
            if (!hasTabPropertiesModel(viewHolder)) return;
            assumeNonNull(viewHolder);
            mSelectedTabIndex = viewHolder.getBindingAdapterPosition();
            mModel.updateSelectedCardForSelection(mSelectedTabIndex, true);

            TabModel tabModel = mCurrentTabModelSupplier.get();
            if (tabModel != null) {
                int tabId = getTabId(viewHolder);
                Tab tab = tabModel.getTabById(tabId);
                if (tab != null) {
                    int index = tabModel.indexOf(tab);
                    if (index != TabModel.INVALID_TAB_INDEX && index != tabModel.index()) {
                        tabModel.setIndex(index, TabSelectionType.FROM_USER);
                    }
                }
            }
        } else if (actionState == ItemTouchHelper.ACTION_STATE_IDLE) {
            if (mSelectedTabIndex != TabModel.INVALID_TAB_INDEX) {
                mModel.updateSelectedCardForSelection(mSelectedTabIndex, false);
                mSelectedTabIndex = TabModel.INVALID_TAB_INDEX;
            }
        }
    }

    private int getTabId(RecyclerView.ViewHolder viewHolder) {
        return assumeNonNull(((ViewHolder) viewHolder).model).get(TabProperties.TAB_ID);
    }
}
