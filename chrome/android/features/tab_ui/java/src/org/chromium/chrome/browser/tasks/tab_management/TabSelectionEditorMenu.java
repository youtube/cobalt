// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorActionViewLayout.ActionViewLayoutDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * A {@link ListMenu} for the {@link TabSelectionEditorToolbar} that helps manage a
 * {@link TabSelectionEditorActionViewLayout} for Action views. The menu contains a list of
 * {@link TabSelectionEditorMenuItem}s which hold optional action views if room is available.
 */
public class TabSelectionEditorMenu implements ListMenu, OnItemClickListener,
                                               SelectionDelegate.SelectionObserver<Integer>,
                                               ActionViewLayoutDelegate {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.MENU_ITEM})
    public static @interface ListItemType {
        int MENU_ITEM = 0;
    }

    private Context mContext;
    // Insertion ordering is important and for performance it is ok as size is very small.
    private Map<Integer, TabSelectionEditorMenuItem> mMenuItems = new LinkedHashMap<>();

    private View mContentView;
    private ListView mListView;
    private TabSelectionEditorActionViewLayout mActionViewLayout;
    private ModelList mModelList;
    private ModelListAdapter mAdapter;

    /**
     * @param context to use for accessing resources.
     * @param actionViewLayout the actionViewLayout to use.
     * @param anchorView the {@link View} to anchor on.
     */
    public TabSelectionEditorMenu(
            Context context, TabSelectionEditorActionViewLayout actionViewLayout) {
        mContext = context;
        mActionViewLayout = actionViewLayout;

        mModelList = new ModelList();
        mAdapter = new ModelListAdapter(mModelList) {
            @Override
            public boolean isEnabled(int position) {
                // For accessibility on Android Q and earlier even if the View for the item is
                // disabled the list item may behave as though it is enabled. Pass back the model
                // state for isEnabled() queries. This is also necessary in some testing frameworks
                // such as Espresso.
                return mModelList.get(position).model.get(
                        TabSelectionEditorActionProperties.ENABLED);
            }
        };
        registerItemTypes();
        mContentView = LayoutInflater.from(mContext).inflate(R.layout.app_menu_layout, null);
        mListView = mContentView.findViewById(R.id.app_menu_list);
        mListView.setAdapter(mAdapter);
        mListView.setDivider(null);
        mListView.setOnItemClickListener(this);

        mActionViewLayout.setListMenuButtonDelegate(() -> this);
        mActionViewLayout.setActionViewLayoutDelegate(this);
    }

    private void registerItemTypes() {
        // clang-format off
        mAdapter.registerType(ListItemType.MENU_ITEM,
            new LayoutViewBuilder(R.layout.list_menu_item),
            TabSelectionEditorMenuAdapter::bindMenuItem);
        // clang-format on
    }

    private ListItem buildListItem(int menuItemId) {
        // Model values are populated while configuring the TabSelectionEditorMenuItem.
        return new ListItem(ListItemType.MENU_ITEM,
                new PropertyModel.Builder(TabSelectionEditorActionProperties.MENU_ITEM_KEYS)
                        .with(TabSelectionEditorActionProperties.MENU_ITEM_ID, menuItemId)
                        .build());
    }

    /**
     * Create a {@link TabSelectionEditorMenuItem} for this menu.
     * @param menuItemId the ID to use for the new TabSelectionEditorMenuItem.
     */
    public void add(int menuItemId) {
        ListItem listItem = buildListItem(menuItemId);
        mMenuItems.put(menuItemId, new TabSelectionEditorMenuItem(mContext, listItem));
        mModelList.add(listItem);
    }

    /**
     * Signal that the action view and property model for {@link TabSelecetionEditorMenuItem} are
     * initialized.
     * @param menuItemId the ID of the TabSelectionEditorMenuItem that finished initialization.
     */
    public void menuItemInitialized(int menuItemId) {
        final TabSelectionEditorMenuItem menuItem = mMenuItems.get(menuItemId);
        if (menuItem.getActionView() == null) {
            mActionViewLayout.setHasMenuOnlyItems(true);
        } else {
            mActionViewLayout.add(menuItem);
        }
    }

    /**
     * @param menuItemId the id of the item to get.
     * @return a {@link} TabSelectionEditorMenuItem or null if the key isn't present.
     */
    public TabSelectionEditorMenuItem getMenuItem(int menuItemId) {
        return mMenuItems.get(menuItemId);
    }

    /**
     * Clears all items in the menu.
     */
    public void clear() {
        mMenuItems.clear();
        mModelList.clear();
        mActionViewLayout.clear();
    }

    /**
     * Delegates selection updates to each menu item.
     * @param selectedItems the currently selected items.
     */
    @Override
    public void onSelectionStateChange(List<Integer> selectedItems) {
        for (TabSelectionEditorMenuItem menuItem : mMenuItems.values()) {
            menuItem.onSelectionStateChange(selectedItems);
        }
    }

    /**
     * {@link OnItemClickListener} implementation.
     */
    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        TabSelectionEditorMenuItem item =
                mMenuItems.get(((ListItem) mAdapter.getItem(position))
                                       .model.get(TabSelectionEditorActionProperties.MENU_ITEM_ID));

        if (!item.onClick()) return;

        if (item.shouldDismissMenu()) mActionViewLayout.dismissMenu();
    }

    /**
     * {@link ActionViewLayoutDelegate} implementation.
     */
    @Override
    public void setVisibleActionViews(Set<TabSelectionEditorMenuItem> visibleActions) {
        if (mModelList.size() == visibleActions.size()) {
            boolean unchanged = true;
            for (TabSelectionEditorMenuItem item : visibleActions) {
                if (mModelList.indexOf(item.getListItem()) == -1) {
                    unchanged = false;
                    break;
                }
            }
            if (unchanged) return;
        }

        // Reset the entire list to maintain the correct ordering.
        mModelList.clear();
        for (TabSelectionEditorMenuItem item : mMenuItems.values()) {
            if (visibleActions.contains(item)) {
                item.setActionViewShowing(true);
                continue;
            }

            item.setActionViewShowing(false);
            mModelList.add(item.getListItem());
        }
        // Resize the list which is necessary if elements are removed.
        mListView.invalidateViews();
    }

    /**
     * {@link ListMenu} implementation.
     */
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public void addContentViewClickRunnable(Runnable runnable) {}

    @Override
    public int getMaxItemWidth() {
        return mContext.getResources().getDimensionPixelSize(R.dimen.menu_width);
    }
}
