// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_ITEM_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_ITEM_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

namespace apps {
class ShortcutUpdate;
}  // namespace apps

// An app shortcut list item provided by the App Service.
class AppServiceShortcutItem : public ChromeAppListItem,
                               public app_list::AppContextMenuDelegate {
 public:
  static const char kItemType[];

  AppServiceShortcutItem(
      Profile* profile,
      AppListModelUpdater* model_updater,
      const apps::ShortcutUpdate& shortcut_update,
      const app_list::AppListSyncableService::SyncItem* sync_item);
  AppServiceShortcutItem(
      Profile* profile,
      AppListModelUpdater* model_updater,
      const apps::ShortcutView& shortcut_view,
      const app_list::AppListSyncableService::SyncItem* sync_item);
  AppServiceShortcutItem(const AppServiceShortcutItem&) = delete;
  AppServiceShortcutItem& operator=(const AppServiceShortcutItem&) = delete;
  ~AppServiceShortcutItem() override;

  // Update the shortcut item with the new  shortcut info from the Shortcut
  // Registry Cache.
  void OnShortcutUpdate(const apps::ShortcutUpdate& update);

 private:
  AppServiceShortcutItem(
      Profile* profile,
      AppListModelUpdater* model_updater,
      const apps::ShortcutId& shortcut_id,
      const std::string& shortcut_name,
      const app_list::AppListSyncableService::SyncItem* sync_item);

  // ChromeAppListItem overrides:
  const char* GetItemType() const override;
  void LoadIcon() override;
  void Activate(int event_flags) override;
  void GetContextMenuModel(ash::AppListItemContext item_context,
                           GetMenuModelCallback callback) override;
  app_list::AppContextMenu* GetAppContextMenu() override;

  // app_list::AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  void OnLoadIcon(apps::IconValuePtr icon_value,
                  apps::IconValuePtr badge_value);

  std::unique_ptr<app_list::AppContextMenu> context_menu_;

  apps::ShortcutId shortcut_id_;

  base::WeakPtrFactory<AppServiceShortcutItem> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_ITEM_H_
