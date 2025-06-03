// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

/**
 * Chrome implementation of dropdown context menu which leverages {@link BasicListMenu}
 * and {@link AnchoredPopupWindow}.
 */
public class ChromeSelectionDropdownMenuDelegate implements SelectionDropdownMenuDelegate {
    @Nullable
    private AnchoredPopupWindow mPopupWindow;

    @Override
    public void show(Context context, View rootView, MVCListAdapter.ModelList items,
            ItemClickListener clickListener, int x, int y) {
        assert mPopupWindow == null : "Dismiss previous popup window before calling show()";

        Rect dropdownRect = new Rect(x, y, x + 1, y + 1);
        BasicListMenu menu = new BasicListMenu(context, items, clickListener::onItemClick);

        mPopupWindow =
                new AnchoredPopupWindow(context, rootView, new ColorDrawable(Color.TRANSPARENT),
                        menu.getContentView(), new RectProvider(dropdownRect), null);
        AnchoredPopupWindow.LayoutObserver layoutObserver =
                (positionBelow, x2, y2, width, height, anchorRect)
                -> mPopupWindow.setAnimationStyle(positionBelow ? R.style.StartIconMenuAnim
                                                                : R.style.StartIconMenuAnimBottom);
        mPopupWindow.setLayoutObserver(layoutObserver);
        mPopupWindow.setVerticalOverlapAnchor(true);
        mPopupWindow.setHorizontalOverlapAnchor(true);
        mPopupWindow.setMaxWidth(
                context.getResources().getDimensionPixelSize(R.dimen.home_button_list_menu_width));
        mPopupWindow.setFocusable(true);
        mPopupWindow.setOutsideTouchable(true);
        mPopupWindow.addOnDismissListener(() -> mPopupWindow = null);
        mPopupWindow.show();
    }

    @Override
    public void dismiss() {
        if (mPopupWindow != null) {
            mPopupWindow.dismiss();
        }
        mPopupWindow = null;
    }

    @Override
    public int getGroupId(PropertyModel itemModel) {
        // We need to check first because PropertyModel#get throws an exception if a key
        // is not present in the Map.
        if (itemModel.containsKey(ListMenuItemProperties.GROUP_ID)) {
            return itemModel.get(ListMenuItemProperties.GROUP_ID);
        }
        return 0;
    }

    @Override
    public int getItemId(PropertyModel itemModel) {
        // We need to check first because PropertyModel#get throws an exception if a key
        // is not present in the Map.
        if (itemModel.containsKey(ListMenuItemProperties.MENU_ITEM_ID)) {
            return itemModel.get(ListMenuItemProperties.MENU_ITEM_ID);
        }
        return 0;
    }

    @Nullable
    @Override
    public Intent getItemIntent(PropertyModel itemModel) {
        // We need to check first because PropertyModel#get throws an exception if a key
        // is not present in the Map.
        if (itemModel.containsKey(ListMenuItemProperties.INTENT)) {
            return itemModel.get(ListMenuItemProperties.INTENT);
        }
        return null;
    }

    @Nullable
    @Override
    public View.OnClickListener getClickListener(PropertyModel itemModel) {
        // We need to check first because PropertyModel#get throws an exception if a key
        // is not present in the Map.
        if (itemModel.containsKey(ListMenuItemProperties.CLICK_LISTENER)) {
            return itemModel.get(ListMenuItemProperties.CLICK_LISTENER);
        }
        return null;
    }

    @Override
    public ListItem getDivider() {
        return BasicListMenu.buildMenuDivider();
    }

    @Override
    public ListItem getMenuItem(String title, @Nullable String contentDescription, int groupId,
            int id, @Nullable Drawable startIcon, boolean isIconTintable, boolean groupContainsIcon,
            boolean enabled, @Nullable View.OnClickListener clickListener,
            @Nullable Intent intent) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.CONTENT_DESCRIPTION, contentDescription)
                        .with(ListMenuItemProperties.GROUP_ID, groupId)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, id)
                        .with(ListMenuItemProperties.START_ICON_DRAWABLE, startIcon)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.CLICK_LISTENER, clickListener)
                        .with(ListMenuItemProperties.INTENT, intent)
                        .with(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN,
                                groupContainsIcon);
        if (isIconTintable) {
            modelBuilder.with(ListMenuItemProperties.TINT_COLOR_ID,
                    R.color.default_icon_color_secondary_tint_list);
        }
        return new ListItem(ListMenuItemType.MENU_ITEM, modelBuilder.build());
    }
}
