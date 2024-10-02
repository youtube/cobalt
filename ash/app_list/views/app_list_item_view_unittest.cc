// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/label.h"

namespace ash {

class AppListItemViewTest : public AshTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  AppListItemViewTest() = default;
  ~AppListItemViewTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        app_list_features::kDragAndDropRefactor, IsUsingDragDropController());

    AshTestBase::SetUp();

    ShellTestApi().drag_drop_controller()->SetLoopClosureForTesting(
        base::BindLambdaForTesting([&]() {
          drag_started_on_controller_++;
          ASSERT_TRUE(drag_view_);
          EXPECT_EQ(GetDragState(drag_view_),
                    AppListItemView::DragState::kStarted);
          GetEventGenerator()->ReleaseTouch();
          EXPECT_EQ(GetDragState(drag_view_),
                    AppListItemView::DragState::kNone);
        }),
        base::DoNothing());
  }

  static views::View* GetNewInstallDot(AppListItemView* view) {
    return view->new_install_dot_;
  }

  AppListItem* CreateAppListItem(const std::string& name) {
    AppListItem* item =
        GetAppListTestHelper()->model()->CreateAndAddItem(name + "_id");
    item->SetName(name);
    return item;
  }

  AppListItem* CreateFolderItem(const int num_apps) {
    AppListItem* item =
        GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(
            num_apps);
    return item;
  }

  AppListItemView::DragState GetDragState(AppListItemView* view) {
    return view->drag_state_;
  }

  bool IsIconScaled(AppListItemView* view) { return view->icon_scale_ != 1.0f; }

  void SetAppListItemViewForTest(AppListItemView* view) { drag_view_ = view; }

  void MaybeCheckDragStartedOnControllerCount(int count) {
    if (IsUsingDragDropController()) {
      EXPECT_EQ(count, drag_started_on_controller_);
    }
  }

  bool IsUsingDragDropController() { return GetParam(); }

  int drag_started_on_controller_ = 0;
  raw_ptr<AppListItemView, ExperimentalAsh> drag_view_;
  base::test::ScopedFeatureList scoped_feature_list_;
};
INSTANTIATE_TEST_SUITE_P(All, AppListItemViewTest, testing::Bool());

using AppListItemViewTestWithDragDropController = AppListItemViewTest;

INSTANTIATE_TEST_SUITE_P(All,
                         AppListItemViewTestWithDragDropController,
                         testing::Values(true));

TEST_P(AppListItemViewTest, NewInstallDot) {
  AppListItem* item = CreateAppListItem("Google Buzz");
  ASSERT_FALSE(item->is_new_install());

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  ui::AXNodeData node_data;

  // By default, the new install dot is not visible.
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* item_view = apps_grid_view->GetItemViewAt(0);
  views::View* new_install_dot = GetNewInstallDot(item_view);
  ASSERT_TRUE(new_install_dot);
  EXPECT_FALSE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz");
  item_view->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      node_data.GetStringAttribute(ax::mojom::StringAttribute::kDescription),
      "");

  // When the app is a new install the dot is visible and the tooltip changes.
  item->SetIsNewInstall(true);
  EXPECT_TRUE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz\nNew install");
  item_view->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      node_data.GetStringAttribute(ax::mojom::StringAttribute::kDescription),
      "New install");
}

TEST_P(AppListItemViewTest, LabelInsetWithNewInstallDot) {
  AppListItem* long_item = CreateAppListItem("Very very very very long name");
  long_item->SetIsNewInstall(true);
  AppListItem* short_item = CreateAppListItem("Short");
  short_item->SetIsNewInstall(true);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* long_item_view = apps_grid_view->GetItemViewAt(0);
  AppListItemView* short_item_view = apps_grid_view->GetItemViewAt(1);

  // The item with the long name has its title bounds left edge inset to make
  // space for the blue dot.
  EXPECT_LT(long_item_view->GetDefaultTitleBoundsForTest().x(),
            long_item_view->title()->x());

  // The item with the short name does not have the title bounds inset, because
  // there is enough space for the blue dot as-is.
  EXPECT_EQ(short_item_view->GetDefaultTitleBoundsForTest(),
            short_item_view->title()->bounds());
}

TEST_P(AppListItemViewTest, AppItemReleaseTouchBeforeTimerFires) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  generator->ReleaseTouch();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(0);
}

TEST_P(AppListItemViewTest, AppItemDragStateChange) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  generator->MoveTouchBy(10, 10);

  if (!IsUsingDragDropController()) {
    EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kStarted);
    generator->ReleaseTouch();
  }

  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(1);
}

TEST_P(AppListItemViewTest, AppItemDragStateAfterLongPress) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  // Verify that actual drag state is not started until the item is moved.
  ui::GestureEvent long_press(
      from.x(), from.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  generator->Dispatch(&long_press);
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  // After a long press, the first event type is ET_GESTURE_SCROLL_BEGIN
  // but drag does not start until ET_GESTURE_SCROLL_UPDATE, so do the
  // movement in two steps.
  generator->MoveTouchBy(5, 5);
  generator->MoveTouchBy(5, 5);

  if (!IsUsingDragDropController()) {
    EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kStarted);
    generator->ReleaseTouch();
  }

  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(1);
}

TEST_P(AppListItemViewTest, AppItemReleaseTouchBeforeDragStart) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();

  generator->ReleaseTouch();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(0);
}

TEST_P(AppListItemViewTest, AppItemReleaseTouchBeforeDragStartWithLongPress) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  ui::GestureEvent long_press(
      from.x(), from.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  generator->Dispatch(&long_press);

  generator->ReleaseTouch();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_EQ(drag_started_on_controller_, 0);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(0);
}

TEST_P(AppListItemViewTestWithDragDropController,
       TouchDragAppRemovedDoesNotCrash) {
  CreateAppListItem("TestItem 1");
  CreateAppListItem("TestItem 2");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  const std::string kDraggedItemId = view->item()->id();
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view->view_model();
  EXPECT_EQ(view_model->view_size(), 2u);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  // Make sure that the item view is deleted after releasing the drag. This
  // should occur within the same thread as the events to force the crash.
  ShellTestApi().drag_drop_controller()->SetLoopClosureForTesting(
      base::BindLambdaForTesting([&]() {
        drag_started_on_controller_++;
        GetEventGenerator()->ReleaseTouch();
        GetAppListTestHelper()->model()->DeleteItem(kDraggedItemId);
      }),
      base::DoNothing());

  generator->MoveTouch(
      apps_grid_view->GetItemViewAt(1)->GetIconBoundsInScreen().CenterPoint());

  EXPECT_EQ(view_model->view_size(), 1u);
  EXPECT_FALSE(GetAppListTestHelper()->model()->FindItem(kDraggedItemId));
  MaybeCheckDragStartedOnControllerCount(1);
}

TEST_P(AppListItemViewTestWithDragDropController,
       AppListFolderLabelShowsAfterMouseClick) {
  CreateFolderItem(2);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(view->title()->GetVisible());

  auto* generator = GetEventGenerator();

  // Press folder icon to open.
  gfx::Point folder_center = view->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(folder_center);
  generator->ClickLeftButton();
  EXPECT_TRUE(helper->IsInFolderView());

  // Attempt to fire the mouse drag timer. The view should've stopped after
  // releasing the click.
  EXPECT_FALSE(view->FireMouseDragTimerForTest());

  // Press ESC key to close the folder grid.
  generator->PressKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(helper->IsInFolderView());

  EXPECT_TRUE(view->title()->GetVisible());
}

}  // namespace ash
