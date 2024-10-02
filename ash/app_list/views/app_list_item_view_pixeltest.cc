// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class AppListItemViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple</*use_folder_icon_refresh=*/bool,
                     /*use_tablet_mode=*/bool,
                     /*use_dense_ui=*/bool,
                     /*use_rtl=*/bool,
                     /*is_new_install=*/bool,
                     /*has_notification=*/bool>> {
 public:
  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = use_rtl();
    return init_params;
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // As per `app_list_config_provider.cc`, dense values are used for screens
    // with width OR height <= 675.
    UpdateDisplay(use_dense_ui() ? "800x600" : "1200x800");
    if (use_folder_icon_refresh()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kAppCollectionFolderRefresh);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAppCollectionFolderRefresh);
    }
  }

  // Creates multiple folders that contain from 1 app to `max_items` apps
  // respectively.
  void CreateFoldersContainingDifferentNumOfItems(int max_items) {
    AppListFolderItem* folder_item;
    for (int i = 1; i <= max_items; ++i) {
      if (i == 1) {
        folder_item = GetAppListTestHelper()->model()->CreateSingleItemFolder(
            "folder_id", "item_id");
      } else {
        folder_item =
            GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(i);
      }
      // Update the notification state of the first app in the folder to
      // simulate that there exists an app with notifications in the folder.
      folder_item->item_list()->item_at(0)->UpdateNotificationBadge(
          has_notification());
    }
  }

  void CreateAppListItem(const std::string& name) {
    AppListItem* item =
        GetAppListTestHelper()->model()->CreateAndAddItem(name + "_id");
    item->SetName(name);
    item->SetIsNewInstall(is_new_install());
    item->UpdateNotificationBadge(has_notification());
  }

  void ShowAppList() {
    if (use_tablet_mode()) {
      Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    } else {
      GetAppListTestHelper()->ShowAppList();
    }
  }

  AppsGridView* GetAppsGridView() {
    if (use_tablet_mode()) {
      return GetAppListTestHelper()->GetRootPagedAppsGridView();
    }

    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  AppListItemView* GetItemViewAt(size_t index) {
    return GetAppsGridView()->GetItemViewAt(index);
  }

  std::string GenerateScreenshotName() {
    std::string stringified_params = base::JoinString(
        {use_tablet_mode() ? "tablet_mode" : "clamshell_mode",
         use_dense_ui() ? "dense_ui" : "regular_ui", use_rtl() ? "rtl" : "ltr",
         is_new_install() ? "new_install=true" : "new_install=false",
         has_notification() ? "has_notification=true"
                            : "has_notification=false"},
        "|");
    return base::JoinString({"app_list_item_view", stringified_params}, ".");
  }

  bool use_folder_icon_refresh() const { return std::get<0>(GetParam()); }
  bool use_tablet_mode() const { return std::get<1>(GetParam()); }
  bool use_dense_ui() const { return std::get<2>(GetParam()); }
  bool use_rtl() const { return std::get<3>(GetParam()); }
  bool is_new_install() const { return std::get<4>(GetParam()); }
  bool has_notification() const { return std::get<5>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AppListItemViewPixelTest,
    testing::Combine(/*use_folder_icon_refresh=*/testing::Bool(),
                     /*use_tablet_mode=*/testing::Bool(),
                     /*use_dense_ui=*/testing::Bool(),
                     /*use_rtl=*/testing::Bool(),
                     /*is_new_install=*/testing::Bool(),
                     /*has_notification=*/testing::Bool()));

TEST_P(AppListItemViewPixelTest, AppListItemView) {
  // Folder icon refresh doesn't change the app list item view.
  if (use_folder_icon_refresh()) {
    return;
  }
  CreateAppListItem("App");
  CreateAppListItem("App with a loooooooong name");

  ShowAppList();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName(), /*revision_number=*/0, GetItemViewAt(0),
      GetItemViewAt(1)));
}

// Verifies the layout of the item icons inside a folder.
TEST_P(AppListItemViewPixelTest, AppListFolderItemsLayoutInIcon) {
  // Skip the case where the apps are newly installed as it doesn't change the
  // folder icons.
  if (is_new_install()) {
    return;
  }

  // Reset any configs set by previous tests so that
  // ItemIconInFolderIconMargin() in app_list_config.cc is correctly
  // initialized. Can be removed if folder icon refresh is set as default.
  AppListConfigProvider::Get().ResetForTesting();

  // To test the item counter on folder icons, set the maximum number of the
  // items in a folder to 5. For legacy folder icons, set the max items to 4 to
  // reduce the revisions.
  const int max_items_in_folder = use_folder_icon_refresh() ? 5 : 4;
  CreateFoldersContainingDifferentNumOfItems(max_items_in_folder);
  ShowAppList();

  if (use_folder_icon_refresh()) {
    EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
        GenerateScreenshotName(), /*revision_number=*/1, GetItemViewAt(0),
        GetItemViewAt(1), GetItemViewAt(2), GetItemViewAt(3),
        GetItemViewAt(4)));
  } else {
    EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
        GenerateScreenshotName(), /*revision_number=*/0, GetItemViewAt(0),
        GetItemViewAt(1), GetItemViewAt(2), GetItemViewAt(3)));
  }
}

// Verifies the folder icon is extended when an app is dragged upon it.
TEST_P(AppListItemViewPixelTest, AppListFolderIconExtendedState) {
  // Skip the case where the apps are newly installed as it doesn't change the
  // folder icons.
  if (is_new_install()) {
    return;
  }

  // Reset any configs set by previous tests so that
  // ItemIconInFolderIconMargin() in app_list_config.cc is correctly
  // initialized. Can be removed if folder icon refresh is set as default.
  AppListConfigProvider::Get().ResetForTesting();

  // To test the item counter on folder icons, set the maximum number of the
  // items in a folder to 5. For legacy folder icons, set the max items to 4 to
  // reduce the revisions.
  const int max_items_in_folder = use_folder_icon_refresh() ? 5 : 4;
  CreateFoldersContainingDifferentNumOfItems(max_items_in_folder);
  CreateAppListItem("App");
  ShowAppList();

  // For tablet mode, simulate that a drag starts and enter the cardified state.
  if (use_tablet_mode()) {
    GetAppListTestHelper()
        ->GetRootPagedAppsGridView()
        ->MaybeStartCardifiedView();
  }

  // Simulate that there is an app dragged onto it for each folder.
  for (int i = 0; i < max_items_in_folder; ++i) {
    GetItemViewAt(i)->OnDraggedViewEnter();
  }

  if (use_folder_icon_refresh()) {
    EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
        GenerateScreenshotName(), /*revision_number=*/1, GetItemViewAt(0),
        GetItemViewAt(1), GetItemViewAt(2), GetItemViewAt(3),
        GetItemViewAt(4)));
  } else {
    EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
        GenerateScreenshotName(), /*revision_number=*/0, GetItemViewAt(0),
        GetItemViewAt(1), GetItemViewAt(2), GetItemViewAt(3)));
  }

  // Reset the states.
  for (int i = 0; i < max_items_in_folder; ++i) {
    GetItemViewAt(i)->OnDraggedViewExit();
  }
  if (use_tablet_mode()) {
    GetAppListTestHelper()->GetRootPagedAppsGridView()->MaybeEndCardifiedView();
  }
}

// Vefifies the dragged folder icon proxy is correctly created.
TEST_P(AppListItemViewPixelTest, DraggedAppListFolderIcon) {
  // Skip the case where the apps are newly installed or have notifications as
  // they don't change the folder icons.
  if (is_new_install() || has_notification()) {
    return;
  }

  // Reset any configs set by previous tests so that
  // ItemIconInFolderIconMargin() in app_list_config.cc is correctly
  // initialized. Can be removed if folder icon refresh is set as default.
  AppListConfigProvider::Get().ResetForTesting();

  // Set the maximum number of the items in a folder that we want to test to 4.
  const int max_items_in_folder = 4;
  CreateFoldersContainingDifferentNumOfItems(max_items_in_folder);
  ShowAppList();

  auto* event_generator = GetEventGenerator();
  AppsGridView* apps_grid_view = GetAppsGridView();
  gfx::Point grid_center = apps_grid_view->GetBoundsInScreen().CenterPoint();

  // Create a folder item view list for folders with different number of items.
  // This is used instead of GetItemViewAt() to prevent reordering while
  // dragging each folder.
  std::vector<AppListItemView*> folder_list;
  for (int i = 0; i < max_items_in_folder; ++i) {
    folder_list.push_back(GetItemViewAt(i));
  }

  for (int i = 0; i < max_items_in_folder; ++i) {
    gfx::Point folder_icon_center =
        folder_list[i]->GetIconBoundsInScreen().CenterPoint();

    // Start dragging the folder icon.
    if (use_tablet_mode()) {
      event_generator->PressTouch(folder_icon_center);
      folder_list[i]->FireTouchDragTimerForTest();
      event_generator->MoveTouch(grid_center);
      std::unique_ptr<test::AppsGridViewTestApi> test_api =
          std::make_unique<test::AppsGridViewTestApi>(apps_grid_view);
      test_api->WaitForItemMoveAnimationDone();
    } else {
      event_generator->MoveMouseTo(folder_icon_center);
      event_generator->PressLeftButton();
      folder_list[i]->FireMouseDragTimerForTest();
      event_generator->MoveMouseTo(grid_center);
    }

    const int revision_number = use_folder_icon_refresh() ? 3 : 2;

    std::string filename = base::NumberToString(i + 1) + "_items_folder";
    EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
        base::JoinString({GenerateScreenshotName(), filename}, "."),
        revision_number,
        apps_grid_view->app_drag_icon_proxy_for_test()->GetWidgetForTesting()));

    // Release the drag.
    if (use_tablet_mode()) {
      event_generator->ReleaseTouch();
    } else {
      event_generator->ReleaseLeftButton();
    }
  }
}

}  // namespace ash
