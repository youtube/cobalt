// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.database.DataSetObserver;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuItemViewBinder;
import org.chromium.ui.listmenu.ListSectionDividerViewBinder;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * A coordinator for the overflow menu for tabs and tab groups. This applies to both the
 * TabGridDialog toolbar and tab group cards on GTS. It is responsible for creating a list of menu
 * items, setting up the menu, and displaying the menu.
 *
 * @param <T> The type of the ID of the overflow menu's origin. For individual tabs, this is a tab
 *     ID. For tab groups, it's the tab group ID.
 */
public abstract class TabOverflowMenuCoordinator<T> {

    /**
     * Helper interface for handling menu item clicks.
     *
     * @param <T> The type of the ID of the overflow menu's origin. For individual tabs, this is a
     *     tab ID. For tab groups, it's the tab group ID.
     */
    @FunctionalInterface
    public interface OnItemClickedCallback<T> {
        void onClick(@IdRes int menuId, T id, @Nullable String collaborationId);
    }

    private static class OverflowMenuHolder<T> {
        private static final int INVALID_ITEM_ID = -1;
        private final Context mContext;
        private final View mContentView;
        private final ModelList mModelList = new ModelList();
        private final ComponentCallbacks mComponentCallbacks;
        private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
        private AnchoredPopupWindow mMenuWindow;

        OverflowMenuHolder(
                RectProvider anchorViewRectProvider,
                boolean horizontalOverlapAnchor,
                boolean verticalOverlapAnchor,
                @StyleRes int animStyle,
                @HorizontalOrientation int horizontalOrientation,
                @LayoutRes int menuLayout,
                Drawable menuBackground,
                OnItemClickedCallback<T> onItemClickedCallback,
                T id,
                @Nullable String collaborationId,
                @DimenRes int popupWidthRes,
                @Nullable Callback<OverflowMenuHolder<T>> onDismiss,
                Activity activity) {
            mContext = activity;
            mComponentCallbacks =
                    new ComponentCallbacks() {
                        @Override
                        public void onConfigurationChanged(Configuration newConfig) {
                            if (mMenuWindow == null || !mMenuWindow.isShowing()) return;
                            mMenuWindow.dismiss();
                        }

                        @Override
                        public void onLowMemory() {}
                    };
            mContext.registerComponentCallbacks(mComponentCallbacks);

            mContentView = LayoutInflater.from(mContext).inflate(menuLayout, null);

            ListView listView = mContentView.findViewById(R.id.tab_group_action_menu_list);
            ModelListAdapter adapter =
                    new ModelListAdapter(mModelList) {
                        @Override
                        public long getItemId(int position) {
                            ListItem item = (ListItem) getItem(position);
                            if (getItemViewType(position) == ListMenuItemType.MENU_ITEM) {
                                return item.model.get(ListMenuItemProperties.MENU_ITEM_ID);
                            } else {
                                return INVALID_ITEM_ID;
                            }
                        }
                    };
            adapter.registerType(
                    ListMenuItemType.MENU_ITEM,
                    new LayoutViewBuilder(R.layout.list_menu_item),
                    ListMenuItemViewBinder::binder);
            adapter.registerType(
                    ListMenuItemType.DIVIDER,
                    new LayoutViewBuilder(R.layout.list_section_divider),
                    ListSectionDividerViewBinder::bind);
            listView.setAdapter(adapter);
            listView.setOnItemClickListener(
                    (p, v, pos, menuId) -> {
                        onItemClickedCallback.onClick((int) menuId, id, collaborationId);
                        mMenuWindow.dismiss();
                    });

            View decorView = activity.getWindow().getDecorView();

            mMenuWindow =
                    new AnchoredPopupWindow(
                            mContext,
                            decorView,
                            menuBackground,
                            mContentView,
                            anchorViewRectProvider);
            mMenuWindow.setFocusable(true);
            mMenuWindow.setHorizontalOverlapAnchor(horizontalOverlapAnchor);
            mMenuWindow.setVerticalOverlapAnchor(verticalOverlapAnchor);
            mMenuWindow.setPreferredHorizontalOrientation(horizontalOrientation);
            // Override animation style or animate from anchor as default.
            if (animStyle == ResourcesCompat.ID_NULL) {
                mMenuWindow.setAnimationStyle(animStyle);
            } else {
                mMenuWindow.setAnimateFromAnchor(true);
            }
            int popupWidth = mContext.getResources().getDimensionPixelSize(popupWidthRes);
            mMenuWindow.setMaxWidth(popupWidth);

            // Resize if any new elements are added.
            adapter.registerDataSetObserver(
                    new DataSetObserver() {
                        @Override
                        public void onChanged() {
                            resize();
                        }
                    });

            // When the menu is dismissed, call destroy to unregister the orientation listener.
            mMenuWindow.addOnDismissListener(
                    () -> {
                        if (onDismiss != null) {
                            onDismiss.onResult(this);
                        }
                        destroy();
                    });
        }

        ModelList getModelList() {
            return mModelList;
        }

        View getContentView() {
            return mContentView;
        }

        void show() {
            mMenuWindow.show();
        }

        void resize() {
            mMenuWindow.onRectChanged();
        }

        void dismiss() {
            mMenuWindow.dismiss();
        }

        void destroy() {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
            // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
            // with a stack trace showing the stack during LifetimeAssert.create().
            LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        }
    }

    protected final @NonNull CollaborationService mCollaborationService;

    private final @LayoutRes int mMenuLayout;
    private final OnItemClickedCallback<T> mOnItemClickedCallback;
    private final Supplier<TabModel> mTabModelSupplier;
    protected final @Nullable TabGroupSyncService mTabGroupSyncService;
    private @Nullable OverflowMenuHolder<T> mMenuHolder;

    /**
     * @param menuLayout The menu layout to use.
     * @param onItemClickedCallback A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param collaborationService Used for checking the user is the owner of a group.
     */
    protected TabOverflowMenuCoordinator(
            @LayoutRes int menuLayout,
            OnItemClickedCallback<T> onItemClickedCallback,
            Supplier<TabModel> tabModelSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @NonNull CollaborationService collaborationService) {
        mMenuLayout = menuLayout;
        mOnItemClickedCallback = onItemClickedCallback;
        mTabModelSupplier = tabModelSupplier;
        mTabGroupSyncService = tabGroupSyncService;
        assert collaborationService != null;
        mCollaborationService = collaborationService;
    }

    /**
     * Implemented in {@link TabGroupContextMenuCoordinator} to initialize the custom view for the
     * tab group context menu. This method inflates necessary components, including the color picker
     * and group title text.
     *
     * @param contentView The root view of the content where the custom view will be initialized.
     * @param isIncognito Whether the current tab model is incognito or not.
     */
    protected void buildCustomView(View contentView, boolean isIncognito) {}

    /**
     * Concrete class required to define what the ModelList for the menu contains.
     *
     * @param itemList The {@link ModelList} to populate.
     * @param isIncognito Whether the current tab model is incognito or not.
     * @param isTabGroupSyncEnabled Whether to tab group sync is enabled.
     * @param hasCollaborationData Whether the menu will call buildCollaborationMenuItems after.
     */
    protected abstract void buildMenuActionItems(
            ModelList itemList,
            boolean isIncognito,
            boolean isTabGroupSyncEnabled,
            boolean hasCollaborationData);

    /**
     * Concrete class required to define what to add for collaborations.
     *
     * @param itemList The {@link ModelList} to populate.
     * @param memberRole The role of the current user in the group.
     */
    protected abstract void buildCollaborationMenuItems(
            ModelList itemList, @MemberRole int memberRole);

    /** Concrete class required to get a specific menu width for the menu pop up window. */
    protected abstract @DimenRes int getMenuWidth();

    /** Returns the collaborationId relevant for the object with ID {@code id} */
    protected abstract @Nullable String getCollaborationIdOrNull(T id);

    /** Returns menu background drawable. */
    public Drawable getMenuBackground(Context context, boolean isIncognito) {
        final @DrawableRes int bgDrawableId =
                isIncognito ? R.drawable.menu_bg_tinted_on_dark_bg : R.drawable.menu_bg_tinted;

        return AppCompatResources.getDrawable(context, bgDrawableId);
    }

    // TODO(crbug.com/357878838): Pass the activity through constructor and setup test to test this
    // method
    /**
     * See {@link #createAndShowMenu(RectProvider, Object, boolean, boolean, int, int, Activity)}}
     */
    protected void createAndShowMenu(View anchorView, T id, @NonNull Activity activity) {
        createAndShowMenu(
                new ViewRectProvider(anchorView),
                id,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ true,
                R.style.EndIconMenuAnim,
                HorizontalOrientation.MAX_AVAILABLE_SPACE,
                activity);
    }

    /**
     * Creates a menu view and renders it within an {@link AnchoredPopupWindow}
     *
     * @param anchorViewRectProvider Rect provider for view to anchor the menu.
     * @param id ID of the object the menu needs to be shown for.
     * @param horizontalOverlapAnchor If true, horizontally overlaps menu with the anchor view.
     * @param verticalOverlapAnchor If true, vertically overlaps menu with the anchor view.
     * @param animStyle Animation style to apply for menu show/hide.
     * @param horizontalOrientation {@link HorizontalOrientation} to use for the menu position.
     * @param activity Activity to get resources and decorView for menu.
     */
    protected void createAndShowMenu(
            RectProvider anchorViewRectProvider,
            T id,
            boolean horizontalOverlapAnchor,
            boolean verticalOverlapAnchor,
            @StyleRes int animStyle,
            @HorizontalOrientation int horizontalOrientation,
            @NonNull Activity activity) {
        assert mMenuHolder == null;
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();
        @Nullable String collaborationId = getCollaborationIdOrNull(id);
        Drawable menuBackground = getMenuBackground(activity, isIncognito);
        mMenuHolder =
                new OverflowMenuHolder<>(
                        anchorViewRectProvider,
                        horizontalOverlapAnchor,
                        verticalOverlapAnchor,
                        animStyle,
                        horizontalOrientation,
                        mMenuLayout,
                        menuBackground,
                        mOnItemClickedCallback,
                        id,
                        collaborationId,
                        getMenuWidth(),
                        this::onDismiss,
                        activity);
        buildCustomView(mMenuHolder.getContentView(), isIncognito);
        configureMenuItems(mMenuHolder.getModelList(), isIncognito, collaborationId);
        mMenuHolder.show();
    }

    /**
     * Resizes the menu if the menu holder is available. This is used to adjust the menu size when
     * adding collaboration items for {@link TabGroupContextMenuCoordinator}.
     */
    protected void resizeMenu() {
        if (mMenuHolder != null) {
            mMenuHolder.resize();
        }
    }

    /**
     * Dismisses the menu. No-op if the menu holder is {@code null}, and therefore the menu is not
     * already showing.
     */
    public void dismiss() {
        if (mMenuHolder != null) {
            mMenuHolder.dismiss();
        }
    }

    protected void onMenuDismissed() {}

    protected @Nullable TabModel getTabModel() {
        return mTabModelSupplier.get();
    }

    private void onDismiss(OverflowMenuHolder<T> menuHolder) {
        assert mMenuHolder == menuHolder;
        mMenuHolder = null;
        onMenuDismissed();
    }

    private void configureMenuItems(
            ModelList modelList, boolean isIncognito, @Nullable String collaborationId) {
        boolean hasCollaborationData =
                TabShareUtils.isCollaborationIdValid(collaborationId)
                        && mCollaborationService.getServiceStatus().isAllowedToJoin();
        buildMenuActionItems(
                modelList, isIncognito, mTabGroupSyncService != null, hasCollaborationData);
        if (hasCollaborationData) {
            buildCollaborationMenuItems(
                    modelList, mCollaborationService.getCurrentUserRoleForGroup(collaborationId));
        }
    }
}
