// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/chrome_app_list_item.h"

#include <utility>

#include "ash/public/cpp/tablet_mode.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/reorder/app_list_reorder_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/app_icon_color_cache.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

AppListControllerDelegate* g_controller_for_test = nullptr;

std::unique_ptr<ash::AppListItemMetadata> CreateDefaultMetadata(
    const std::string& app_id) {
  auto metadata = std::make_unique<ash::AppListItemMetadata>();
  metadata->id = app_id;
  return metadata;
}

}  // namespace

// static
void ChromeAppListItem::OverrideAppListControllerDelegateForTesting(
    AppListControllerDelegate* controller) {
  g_controller_for_test = controller;
}

// ChromeAppListItem::TestApi
ChromeAppListItem::TestApi::TestApi(ChromeAppListItem* item) : item_(item) {}

void ChromeAppListItem::TestApi::SetFolderId(const std::string& folder_id) {
  item_->SetFolderId(folder_id);
}

void ChromeAppListItem::TestApi::SetPosition(
    const syncer::StringOrdinal& position) {
  item_->SetPosition(position);
}

void ChromeAppListItem::TestApi::SetName(const std::string& name) {
  item_->SetChromeName(name);
}

// ChromeAppListItem
ChromeAppListItem::ChromeAppListItem(Profile* profile,
                                     const std::string& app_id)
    : metadata_(CreateDefaultMetadata(app_id)), profile_(profile) {}

ChromeAppListItem::ChromeAppListItem(Profile* profile,
                                     const std::string& app_id,
                                     AppListModelUpdater* model_updater)
    : metadata_(CreateDefaultMetadata(app_id)),
      profile_(profile),
      model_updater_(model_updater) {}

ChromeAppListItem::~ChromeAppListItem() = default;

void ChromeAppListItem::SetMetadata(
    std::unique_ptr<ash::AppListItemMetadata> metadata) {
  metadata_ = std::move(metadata);
}

std::unique_ptr<ash::AppListItemMetadata> ChromeAppListItem::CloneMetadata()
    const {
  return std::make_unique<ash::AppListItemMetadata>(*metadata_);
}

void ChromeAppListItem::PerformActivate(int event_flags) {
  // Handle recording app launch source from the AppList in Demo Mode.
  ash::DemoSession::RecordAppLaunchSourceIfInDemoMode(
      ash::DemoSession::AppLaunchSource::kAppList);
  Activate(event_flags);
  MaybeDismissAppList();
}

syncer::StringOrdinal
ChromeAppListItem::CalculateDefaultPositionIfApplicable() {
  syncer::StringOrdinal page_ordinal;
  syncer::StringOrdinal launch_ordinal;
  extensions::AppSorting* app_sorting = GetAppSorting();
  if (app_sorting->GetDefaultOrdinals(id(), &page_ordinal, &launch_ordinal) &&
      page_ordinal.IsValid() && launch_ordinal.IsValid()) {
    // Set the default position if it exists.
    return syncer::StringOrdinal(page_ordinal.ToInternalValue() +
                                 launch_ordinal.ToInternalValue());
  }

  return syncer::StringOrdinal();
}

void ChromeAppListItem::Activate(int event_flags) {}

const char* ChromeAppListItem::GetItemType() const {
  return "";
}

void ChromeAppListItem::GetContextMenuModel(
    ash::AppListItemContext item_context,
    GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

bool ChromeAppListItem::IsBadged() const {
  return false;
}

app_list::AppContextMenu* ChromeAppListItem::GetAppContextMenu() {
  return nullptr;
}

void ChromeAppListItem::MaybeDismissAppList() {
  // Launching apps can take some time. It looks nicer to dismiss the app list.
  // Do not close app list for home launcher.
  if (!ash::TabletMode::Get() || !ash::TabletMode::Get()->InTabletMode()) {
    GetController()->DismissView();
  }
}

extensions::AppSorting* ChromeAppListItem::GetAppSorting() {
  return extensions::ExtensionSystem::Get(profile())->app_sorting();
}

AppListControllerDelegate* ChromeAppListItem::GetController() {
  return g_controller_for_test != nullptr ? g_controller_for_test
                                          : AppListClientImpl::GetInstance();
}

void ChromeAppListItem::InitFromSync(
    const app_list::AppListSyncableService::SyncItem* sync_item) {
  DCHECK(sync_item && sync_item->item_ordinal.IsValid());
  // An existing synced position exists, use that.
  SetPosition(sync_item->item_ordinal);
  // Only set the name from the sync item if it is empty.
  if (name().empty())
    SetName(sync_item->item_name);

  SetChromeFolderId(sync_item->parent_id);
}

void ChromeAppListItem::LoadIcon() {
  NOTIMPLEMENTED();
}

void ChromeAppListItem::IncrementIconVersion() {
  ++metadata_->icon_version;

  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemIconVersion(id(), metadata_->icon_version);
}

void ChromeAppListItem::SetIcon(const gfx::ImageSkia& icon,
                                bool is_place_holder_icon) {
  metadata_->icon = icon;
  metadata_->icon.EnsureRepsForSupportedScales();
  metadata_->badge_color =
      ash::AppIconColorCache::GetInstance().GetLightVibrantColorForApp(id(),
                                                                       icon);
  metadata_->icon_color =
      is_place_holder_icon
          ? ash::IconColor()
          : app_list::reorder::GetSortableIconColorForApp(id(), icon);

  AppListModelUpdater* updater = model_updater();
  if (updater) {
    // NOTE: `metadata_` could be reset during updating the icon and color
    // through `updater`. Therefore, copy the id and the badge color.
    const std::string id_copy = id();
    const SkColor badge_color_copy = metadata_->badge_color;

    updater->SetItemIconAndColor(id_copy, metadata_->icon,
                                 metadata_->icon_color);
    updater->SetNotificationBadgeColor(id_copy, badge_color_copy);
  }
}

void ChromeAppListItem::SetAppStatus(ash::AppStatus app_status) {
  metadata_->app_status = app_status;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetAppStatus(id(), app_status);
}

void ChromeAppListItem::SetFolderId(const std::string& folder_id) {
  metadata_->folder_id = folder_id;
}

void ChromeAppListItem::SetName(const std::string& name) {
  metadata_->name = name;
  if (model_updater())
    model_updater()->SetItemName(id(), name);
}

void ChromeAppListItem::SetPosition(const syncer::StringOrdinal& position) {
  metadata_->position = position;
}

void ChromeAppListItem::SetIsSystemFolder(bool is_system_folder) {
  metadata_->is_system_folder = is_system_folder;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemIsSystemFolder(id(), is_system_folder);
}

void ChromeAppListItem::SetIsNewInstall(bool is_new_install) {
  metadata_->is_new_install = is_new_install;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetIsNewInstall(id(), is_new_install);
}

void ChromeAppListItem::SetChromeFolderId(const std::string& folder_id) {
  metadata_->folder_id = folder_id;
}

void ChromeAppListItem::SetChromeIsFolder(bool is_folder) {
  metadata_->is_folder = is_folder;
}

void ChromeAppListItem::SetChromeName(const std::string& name) {
  metadata_->name = name;
}

void ChromeAppListItem::SetChromePosition(
    const syncer::StringOrdinal& position) {
  metadata_->position = position;
}

void ChromeAppListItem::SetIsEphemeral(bool is_ephemeral) {
  metadata_->is_ephemeral = is_ephemeral;
}

bool ChromeAppListItem::CompareForTest(const ChromeAppListItem* other) const {
  return id() == other->id() && folder_id() == other->folder_id() &&
         name() == other->name() && GetItemType() == other->GetItemType() &&
         position().Equals(other->position());
}

std::string ChromeAppListItem::ToDebugString() const {
  return id().substr(0, 8) + " '" + name() + "' (" + folder_id() + ") [" +
         position().ToDebugString() + "]";
}

syncer::StringOrdinal ChromeAppListItem::CalculateDefaultPositionForTest() {
  syncer::StringOrdinal page_ordinal;
  syncer::StringOrdinal launch_ordinal;
  extensions::AppSorting* app_sorting = GetAppSorting();
  app_sorting->EnsureValidOrdinals(id(), syncer::StringOrdinal());
  page_ordinal = app_sorting->GetPageOrdinal(id());
  launch_ordinal = app_sorting->GetAppLaunchOrdinal(id());
  return syncer::StringOrdinal(page_ordinal.ToInternalValue() +
                               launch_ordinal.ToInternalValue());
}
