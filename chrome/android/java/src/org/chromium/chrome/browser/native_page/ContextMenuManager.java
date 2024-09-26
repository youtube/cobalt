// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.content.Context;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.ui.base.WindowAndroid.OnCloseContextMenuListener;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Takes care of creating, closing a context menu and triaging the item clicks.
 *
 * Menus created contains options for opening in new window, new tab, new incognito tab,
 * download, remove, and learn more. Clients control which items are shown using
 * {@link Delegate#isItemSupported(int)}.
 */
public class ContextMenuManager implements OnCloseContextMenuListener {
    @IntDef({ContextMenuItemId.SEARCH, ContextMenuItemId.OPEN_IN_NEW_TAB,
            ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP, ContextMenuItemId.OPEN_IN_INCOGNITO_TAB,
            ContextMenuItemId.OPEN_IN_NEW_WINDOW, ContextMenuItemId.SAVE_FOR_OFFLINE,
            ContextMenuItemId.ADD_TO_MY_APPS, ContextMenuItemId.REMOVE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextMenuItemId {
        // The order of the items will be based on the value of their ID. So if new items are added,
        // the value of the existing ones should be modified so they stay in order.
        // Values are also used for indexing - should start from 0 and can't have gaps.
        int SEARCH = 0;
        int OPEN_IN_NEW_TAB = 1;
        int OPEN_IN_NEW_TAB_IN_GROUP = 2;
        int OPEN_IN_INCOGNITO_TAB = 3;
        int OPEN_IN_NEW_WINDOW = 4;
        int SAVE_FOR_OFFLINE = 5;
        int ADD_TO_MY_APPS = 6;
        int REMOVE = 7;

        int NUM_ENTRIES = 8;
    }

    private final NativePageNavigationDelegate mNavigationDelegate;
    private final TouchEnabledDelegate mTouchEnabledDelegate;
    private final Runnable mCloseContextMenuCallback;
    private final String mUserActionPrefix;
    private View mAnchorView;

    /** Defines callback to configure the context menu and respond to user interaction. */
    public interface Delegate {
        /** Opens the current item the way specified by {@code windowDisposition}. */
        void openItem(int windowDisposition);

        /**
         * Opens the current item the way specified by {@code windowDisposition} in a group.
         */
        void openItemInGroup(int windowDisposition);

        /** Remove the current item. */
        void removeItem();

        /**
         * @return the URL of the current item for saving offline, or null if the item can't be
         *         saved offline.
         */
        GURL getUrl();

        /**
         * @return Title to be displayed in the context menu when applicable, or null if no title
         *         should be displayed.
         */
        String getContextMenuTitle();

        /** @return whether the given menu item is supported. */
        boolean isItemSupported(@ContextMenuItemId int menuItemId);

        /** Called when a context menu has been created. */
        void onContextMenuCreated();
    }

    /**
     * Empty implementation of Delegate to allow derived classes to only implement methods they
     * need.
     */
    public static class EmptyDelegate implements Delegate {
        @Override
        public void openItem(int windowDisposition) {}

        @Override
        public void openItemInGroup(int windowDisposition) {}

        @Override
        public void removeItem() {}

        @Override
        public GURL getUrl() {
            return null;
        }

        @Override
        public String getContextMenuTitle() {
            return null;
        }

        @Override
        public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
            return false;
        }

        @Override
        public void onContextMenuCreated() {}
    }

    /**
     * @param navigationDelegate The {@link NativePageNavigationDelegate} for handling navigation
     *                           events.
     * @param touchEnabledDelegate The {@link TouchEnabledDelegate} for handling whether touch
     *                             events are allowed.
     * @param closeContextMenuCallback The callback for closing the context menu.
     * @param userActionPrefix Prefix used to record user actions.
     */
    public ContextMenuManager(NativePageNavigationDelegate navigationDelegate,
            TouchEnabledDelegate touchEnabledDelegate, Runnable closeContextMenuCallback,
            String userActionPrefix) {
        mNavigationDelegate = navigationDelegate;
        mTouchEnabledDelegate = touchEnabledDelegate;
        mCloseContextMenuCallback = closeContextMenuCallback;
        mUserActionPrefix = userActionPrefix;
    }

    /**
     * Populates the context menu.
     *
     * @param menu The menu to populate.
     * @param associatedView The view that requested a context menu.
     * @param delegate Delegate that defines the configuration of the menu and what to do when items
     *            are tapped.
     */
    public void createContextMenu(ContextMenu menu, View associatedView, Delegate delegate) {
        OnMenuItemClickListener listener = new ItemClickListener(delegate);
        boolean hasItems = false;

        for (@ContextMenuItemId int itemId = 0; itemId < ContextMenuItemId.NUM_ENTRIES; itemId++) {
            if (!shouldShowItem(itemId, delegate)
                    || itemId == ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP) {
                continue;
            }

            if (itemId == ContextMenuItemId.OPEN_IN_NEW_TAB
                    && shouldShowItem(ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP, delegate)) {
                if (TabUiFeatureUtilities.showContextMenuOpenNewTabInGroupItemFirst()) {
                    menu.add(Menu.NONE, ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP, Menu.NONE,
                                getResourceIdForMenuItem(associatedView.getContext(),
                                        ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP))
                            .setOnMenuItemClickListener(listener);
                    menu.add(Menu.NONE, itemId, Menu.NONE,
                                getResourceIdForMenuItem(associatedView.getContext(), itemId))
                            .setOnMenuItemClickListener(listener);
                } else {
                    menu.add(Menu.NONE, itemId, Menu.NONE,
                                getResourceIdForMenuItem(associatedView.getContext(), itemId))
                            .setOnMenuItemClickListener(listener);
                    menu.add(Menu.NONE, ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP, Menu.NONE,
                                getResourceIdForMenuItem(associatedView.getContext(),
                                        ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP))
                            .setOnMenuItemClickListener(listener);
                }
                hasItems = true;
                continue;
            }

            menu.add(Menu.NONE, itemId, Menu.NONE,
                        getResourceIdForMenuItem(associatedView.getContext(), itemId))
                    .setOnMenuItemClickListener(listener);
            hasItems = true;
        }

        // No item added. We won't show the menu, so we can skip the rest.
        if (!hasItems) return;

        // Touch events must be disabled on the outer view while the context menu is open. This is
        // to prevent the user long pressing to get the context menu then on the same press
        // scrolling or swiping to dismiss an item (eg. https://crbug.com/638854,
        // https://crbug.com/638555, https://crbug.com/636296).
        mTouchEnabledDelegate.setTouchEnabled(false);
        mAnchorView = associatedView;
        mAnchorView.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {}

            @Override
            public void onViewDetachedFromWindow(View view) {
                if (view == mAnchorView) {
                    mCloseContextMenuCallback.run();
                    view.removeOnAttachStateChangeListener(this);
                }
            }
        });

        notifyContextMenuShown(delegate);
    }

    @Override
    public void onContextMenuClosed() {
        if (mAnchorView == null) return;
        mAnchorView = null;
        mTouchEnabledDelegate.setTouchEnabled(true);
    }

    /**
     * Given currently focused view this function retrieves associated Delegate.
     */
    public static Delegate getDelegateFromFocusedView(View view) {
        return (Delegate) view.getTag(R.id.context_menu_delegate);
    }

    /**
     * notifyContextMenuShown is called right before context menu is shown. It allows delegate to
     * record statistics about user action.
     *
     * @param delegate Delegate for which context menu is shown.
     */
    protected void notifyContextMenuShown(Delegate delegate) {
        delegate.onContextMenuCreated();
        RecordUserAction.record(mUserActionPrefix + ".ContextMenu.Shown");
    }

    /**
     * Given context menu item id and delegate of an element for which context menu is shown,
     * decides if the menu item should be displayed. Takes other features' state into consideration
     * (multiwindow, offline).
     */
    protected boolean shouldShowItem(@ContextMenuItemId int itemId, Delegate delegate) {
        if (!delegate.isItemSupported(itemId)) return false;

        switch (itemId) {
            case ContextMenuItemId.SEARCH:
                return false;
            case ContextMenuItemId.OPEN_IN_NEW_TAB:
                return true;
            case ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP:
                return !TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.getValue()
                        && mNavigationDelegate.isOpenInNewTabInGroupEnabled();
            case ContextMenuItemId.OPEN_IN_INCOGNITO_TAB:
                return mNavigationDelegate.isOpenInIncognitoEnabled();
            case ContextMenuItemId.OPEN_IN_NEW_WINDOW:
                return mNavigationDelegate.isOpenInNewWindowEnabled();
            case ContextMenuItemId.SAVE_FOR_OFFLINE: {
                GURL itemUrl = delegate.getUrl();
                return itemUrl != null && OfflinePageBridge.canSavePage(itemUrl);
            }
            case ContextMenuItemId.REMOVE:
                return true;
            case ContextMenuItemId.ADD_TO_MY_APPS:
                return false;
            default:
                assert false;
                return false;
        }
    }

    /**
     * Returns resource id of a string that should be displayed for menu item with given item id.
     * @param context The activity context.
     */
    protected @StringRes int getResourceIdForMenuItem(Context context, @ContextMenuItemId int id) {
        switch (id) {
            case ContextMenuItemId.OPEN_IN_NEW_TAB:
                return (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(context)
                               && TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.getValue()
                               && mNavigationDelegate.isOpenInNewTabInGroupEnabled())
                        ? R.string.contextmenu_open_in_new_tab_group
                        : R.string.contextmenu_open_in_new_tab;
            case ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP:
                return R.string.contextmenu_open_in_new_tab_group;
            case ContextMenuItemId.OPEN_IN_INCOGNITO_TAB:
                return R.string.contextmenu_open_in_incognito_tab;
            case ContextMenuItemId.OPEN_IN_NEW_WINDOW:
                return R.string.contextmenu_open_in_other_window;
            case ContextMenuItemId.SAVE_FOR_OFFLINE:
                return R.string.contextmenu_save_link;
            case ContextMenuItemId.REMOVE:
                return R.string.remove;
        }
        assert false;
        return 0;
    }

    /**
     * Performs an action corresponding to menu item selected by user.
     * @param itemId Id of menu item selected by user.
     * @param delegate Delegate of an element, for which context menu was shown.
     * @return true if user selection was handled.
     */
    protected boolean handleMenuItemClick(@ContextMenuItemId int itemId, Delegate delegate) {
        switch (itemId) {
            case ContextMenuItemId.OPEN_IN_NEW_TAB:
                delegate.openItem(WindowOpenDisposition.NEW_BACKGROUND_TAB);
                RecordUserAction.record(mUserActionPrefix + ".ContextMenu.OpenItemInNewTab");
                return true;
            case ContextMenuItemId.OPEN_IN_NEW_TAB_IN_GROUP:
                delegate.openItemInGroup(WindowOpenDisposition.NEW_BACKGROUND_TAB);
                RecordUserAction.record(mUserActionPrefix + ".ContextMenu.OpenItemInNewTabInGroup");
                return true;
            case ContextMenuItemId.OPEN_IN_INCOGNITO_TAB:
                delegate.openItem(WindowOpenDisposition.OFF_THE_RECORD);
                RecordUserAction.record(mUserActionPrefix + ".ContextMenu.OpenItemInIncognitoTab");
                return true;
            case ContextMenuItemId.OPEN_IN_NEW_WINDOW:
                delegate.openItem(WindowOpenDisposition.NEW_WINDOW);
                RecordUserAction.record(mUserActionPrefix + ".ContextMenu.OpenItemInNewWindow");
                return true;
            case ContextMenuItemId.SAVE_FOR_OFFLINE:
                delegate.openItem(WindowOpenDisposition.SAVE_TO_DISK);
                RecordUserAction.record(mUserActionPrefix + ".ContextMenu.DownloadItem");
                return true;
            case ContextMenuItemId.REMOVE:
                delegate.removeItem();
                RecordUserAction.record(mUserActionPrefix + ".ContextMenu.RemoveItem");
                return true;
            default:
                return false;
        }
    }

    private class ItemClickListener implements OnMenuItemClickListener {
        private final Delegate mDelegate;

        ItemClickListener(Delegate delegate) {
            mDelegate = delegate;
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            return handleMenuItemClick(item.getItemId(), mDelegate);
        }
    }
}
