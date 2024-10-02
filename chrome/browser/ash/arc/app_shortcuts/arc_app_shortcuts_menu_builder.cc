// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/app_shortcuts/arc_app_shortcuts_menu_builder.h"

#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/arc/app_shortcuts/arc_app_shortcuts_request.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"

namespace arc {

namespace {

// Convert an app_id and a shortcut_id, eg. manifest_new_note_shortcut, into a
// full URL for an Arc app shortcut, of the form:
// appshortcutsearch://[app_id]/[shortcut_id].
std::string ConstructArcAppShortcutUrl(const std::string& app_id,
                                       const std::string& shortcut_id) {
  return "appshortcutsearch://" + app_id + "/" + shortcut_id;
}

}  // namespace

ArcAppShortcutsMenuBuilder::ArcAppShortcutsMenuBuilder(
    Profile* profile,
    const std::string& app_id,
    int64_t display_id,
    int command_id_first,
    int command_id_last)
    : profile_(profile),
      app_id_(app_id),
      display_id_(display_id),
      command_id_first_(command_id_first),
      command_id_last_(command_id_last) {}

ArcAppShortcutsMenuBuilder::~ArcAppShortcutsMenuBuilder() = default;

void ArcAppShortcutsMenuBuilder::BuildMenu(
    const std::string& package_name,
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback) {
  DCHECK(!arc_app_shortcuts_request_);
  // Using base::Unretained(this) here is safe because |this| owns
  // |arc_app_shortcuts_request_|. When |this| is deleted,
  // |arc_app_shortcuts_request_| is also deleted, and once that happens,
  // |arc_app_shortcuts_request_| will never run the callback.
  arc_app_shortcuts_request_ = std::make_unique<ArcAppShortcutsRequest>(
      base::BindOnce(&ArcAppShortcutsMenuBuilder::OnGetAppShortcutItems,
                     base::Unretained(this), base::TimeTicks::Now(),
                     std::move(menu_model), std::move(callback)));
  arc_app_shortcuts_request_->StartForPackage(package_name);
}

void ArcAppShortcutsMenuBuilder::ExecuteCommand(int command_id) {
  DCHECK(command_id >= command_id_first_ && command_id <= command_id_last_);
  size_t index = command_id - command_id_first_;
  DCHECK(app_shortcut_items_);
  DCHECK_LT(index, app_shortcut_items_->size());
  LaunchAppShortcutItem(profile_, app_id_,
                        app_shortcut_items_->at(index).shortcut_id,
                        display_id_);

  // Send a training signal to the search controller.
  AppListClientImpl* app_list_client_impl = AppListClientImpl::GetInstance();
  if (!app_list_client_impl)
    return;
  app_list::LaunchData launch_data;
  // TODO(crbug.com/1199206): This should set launch_data.launched_from.
  launch_data.id = ConstructArcAppShortcutUrl(
      app_id_, app_shortcut_items_->at(index).shortcut_id),
  launch_data.result_type = ash::AppListSearchResultType::kArcAppShortcut;
  launch_data.category = app_list::Category::kAppShortcuts;
  app_list_client_impl->search_controller()->Train(std::move(launch_data));
}

void ArcAppShortcutsMenuBuilder::OnGetAppShortcutItems(
    const base::TimeTicks& start_time,
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback,
    std::unique_ptr<apps::AppShortcutItems> app_shortcut_items) {
  app_shortcut_items_ = std::move(app_shortcut_items);
  if (app_shortcut_items_ && !app_shortcut_items_->empty()) {
    apps::AppShortcutItems& items = *app_shortcut_items_;
    // Sort the shortcuts based on two rules: (1) Static (declared in manifest)
    // shortcuts and then dynamic shortcuts; (2) Within each shortcut type
    // (static and dynamic), shortcuts are sorted in order of increasing rank.
    std::sort(items.begin(), items.end(),
              [](const apps::AppShortcutItem& item1,
                 const apps::AppShortcutItem& item2) {
                return std::tie(item1.type, item1.rank) <
                       std::tie(item2.type, item2.rank);
              });

    menu_model->AddSeparator(ui::DOUBLE_SEPARATOR);
    int command_id = command_id_first_;
    for (const auto& item : items) {
      if (command_id != command_id_first_)
        menu_model->AddSeparator(ui::PADDED_SEPARATOR);
      menu_model->AddItemWithIcon(command_id++,
                                  base::UTF8ToUTF16(item.short_label),
                                  ui::ImageModel::FromImageSkia(item.icon));
    }
  }
  std::move(callback).Run(std::move(menu_model));
  arc_app_shortcuts_request_.reset();

  // Record user metrics.
  UMA_HISTOGRAM_TIMES("Arc.AppShortcuts.BuildMenuTime",
                      base::TimeTicks::Now() - start_time);
}

}  // namespace arc
