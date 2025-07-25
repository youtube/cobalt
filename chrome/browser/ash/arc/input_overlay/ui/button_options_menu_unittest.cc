// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"

#include <memory>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/style/icon_button.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class ButtonOptionsMenuTest : public OverlayViewTestBase {
 public:
  ButtonOptionsMenuTest() = default;
  ~ButtonOptionsMenuTest() override = default;

  size_t GetActionListItemsSize() {
    DCHECK(editing_list_);

    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    if (editing_list_->HasControls()) {
      return scroll_content->children().size();
    }
    return 0;
  }

  // Return -1 if there is no list item for `action`. Otherwise, return the
  // index of list item in `EditingList` for `action`.
  int GetIndexInEditingList(Action* action) {
    if (IsEditingListInZeroState()) {
      return -1;
    }

    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    for (size_t i = 0; i < scroll_content->children().size(); i++) {
      auto* list_item =
          static_cast<ActionViewListItem*>(scroll_content->children()[i]);
      if (list_item->action() == action) {
        return i;
      }
    }
    return -1;
  }

  size_t GetActionViewSize() { return input_mapping_view_->children().size(); }

  bool IsEditingListInZeroState() { return editing_list_->is_zero_state_; }

  void PressTrashButton(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    menu->OnTrashButtonPressed();
  }

  ActionType GetActionType(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    return menu->action()->GetType();
  }

  void PressActionMoveButton(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    ActionTypeButtonGroup* button_group = menu->button_group_;
    DCHECK(button_group);
    button_group->OnActionMoveButtonPressed();
  }

  void PressTapButton(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    ActionTypeButtonGroup* button_group = menu->button_group_;
    DCHECK(button_group);
    button_group->OnActionTapButtonPressed();
  }

  void PressActionEdit(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    ActionEditView* action_edit = menu->action_edit_;
    DCHECK(action_edit);
    action_edit->OnClicked();
  }

  void PressDoneButton(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    LeftClickOn(menu->done_button_);
  }

  void MouseDragActionBy(Action* action, int x, int y) {
    auto* event_generator = GetEventGenerator();
    auto* touch_point = action->action_view()->touch_point();
    DCHECK(touch_point);
    event_generator->MoveMouseTo(
        touch_point->GetBoundsInScreen().CenterPoint());
    event_generator->PressLeftButton();
    event_generator->MoveMouseBy(x, y);
    event_generator->ReleaseLeftButton();
  }

  bool IsActionInTouchInjector(Action* action) {
    const auto& actions = touch_injector_->actions();
    return std::find_if(actions.begin(), actions.end(),
                        [&](const std::unique_ptr<Action>& p) {
                          return action == p.get();
                        }) != actions.end();
  }

  bool IsActionInEditingList(Action* action) {
    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    for (auto* child : scroll_content->children()) {
      auto* list_item = static_cast<ActionViewListItem*>(child);
      DCHECK(list_item);
      if (list_item->action() == action) {
        return true;
      }
    }
    return false;
  }

  bool IsEditLabelFocused(ButtonOptionsMenu* menu, int index) {
    DCHECK(menu);
    ActionEditView* action_edit = menu->action_edit_;
    DCHECK(action_edit);
    EditLabels* edit_labels = action_edit->labels_view_;
    DCHECK(edit_labels);
    return edit_labels->labels_[index]->HasFocus();
  }
};

TEST_F(ButtonOptionsMenuTest, TestRemoveAction) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_EQ(3u, GetActionListItemsSize());
  EXPECT_EQ(3u, GetActionViewSize());
  EXPECT_FALSE(tap_action_->IsDeleted());
  EXPECT_FALSE(tap_action_two_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());

  // Remove Action Tap.
  auto* menu = ShowButtonOptionsMenu(tap_action_);
  PressTrashButton(menu);
  // Default action is still in the list even it is deleted and it is marked as
  // deleted. But it doesn't show up visually.
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_TRUE(tap_action_->IsDeleted());
  EXPECT_FALSE(tap_action_two_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());
  EXPECT_EQ(2u, GetActionListItemsSize());
  EXPECT_EQ(2u, GetActionViewSize());

  // Remove Action Move.
  menu = ShowButtonOptionsMenu(move_action_);
  PressTrashButton(menu);
  // Default action is still in the list even it is deleted and it is marked as
  // deleted. But it doesn't show up visually.
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_TRUE(tap_action_->IsDeleted());
  EXPECT_FALSE(tap_action_two_->IsDeleted());
  EXPECT_TRUE(move_action_->IsDeleted());
  EXPECT_FALSE(IsEditingListInZeroState());
  EXPECT_EQ(1u, GetActionViewSize());

  // Remove Action Move.
  menu = ShowButtonOptionsMenu(tap_action_two_);
  PressTrashButton(menu);
  // Default action is still in the list even it is deleted and it is marked as
  // deleted. But it doesn't show up visually.
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_TRUE(tap_action_->IsDeleted());
  EXPECT_TRUE(tap_action_two_->IsDeleted());
  EXPECT_TRUE(move_action_->IsDeleted());
  EXPECT_TRUE(IsEditingListInZeroState());
  EXPECT_EQ(0u, GetActionViewSize());
}

TEST_F(ButtonOptionsMenuTest, TestChangeActionType) {
  // Change Action Tap.
  auto* menu = ShowButtonOptionsMenu(tap_action_);
  int list_index = GetIndexInEditingList(tap_action_);
  EXPECT_EQ(GetActionType(menu), ActionType::TAP);

  // Left click on `ButtonOptionsMenu::labels_view_` will focus on one of the
  // `labels_` which is tested in `TestClickActionEdit`. This is to test
  // `EditLabel::OnBlur()` doesn't crash when changing action type.
  PressActionEdit(menu);

  PressActionMoveButton(menu);
  EXPECT_EQ(GetActionType(menu), ActionType::MOVE);
  EXPECT_TRUE(IsActionInTouchInjector(menu->action()));
  EXPECT_TRUE(IsActionInEditingList(menu->action()));
  EXPECT_EQ(list_index, GetIndexInEditingList(menu->action()));
  // Change Action Move.
  menu = ShowButtonOptionsMenu(move_action_);
  list_index = GetIndexInEditingList(move_action_);
  EXPECT_EQ(GetActionType(menu), ActionType::MOVE);

  // Press Action Edit twice to focus on the second label.
  PressActionEdit(menu);
  PressActionEdit(menu);

  PressTapButton(menu);
  EXPECT_EQ(GetActionType(menu), ActionType::TAP);
  EXPECT_TRUE(IsActionInTouchInjector(menu->action()));
  EXPECT_TRUE(IsActionInEditingList(menu->action()));
  EXPECT_EQ(list_index, GetIndexInEditingList(menu->action()));
}

TEST_F(ButtonOptionsMenuTest, TestClickActionEdit) {
  auto* menu = ShowButtonOptionsMenu(tap_action_);
  PressActionEdit(menu);
  EXPECT_TRUE(IsEditLabelFocused(menu, /*index=*/0));
  menu = ShowButtonOptionsMenu(move_action_);
  PressActionEdit(menu);
  EXPECT_TRUE(IsEditLabelFocused(menu, /*index=*/0));
  PressActionEdit(menu);
  EXPECT_FALSE(IsEditLabelFocused(menu, /*index=*/0));
  EXPECT_TRUE(IsEditLabelFocused(menu, /*index=*/1));
}

TEST_F(ButtonOptionsMenuTest, TestDisplayRelatedToShelf) {
  // Display is 1000*800. Set window position to lower part.
  auto* game_window = widget_->GetNativeWindow();
  game_window->SetBounds(gfx::Rect(310, 200, 300, 500));

  auto* root_window = game_window->GetRootWindow();
  auto* shelf = ash::RootWindowController::ForWindow(root_window)->shelf();
  DCHECK(shelf);
  shelf->SetAlignment(ash::ShelfAlignment::kBottom);
  // Drag action to bottom.
  MouseDragActionBy(tap_action_two_, /*x=*/0, /*y=*/500);
  auto* menu = ShowButtonOptionsMenu(tap_action_two_);
  // Menu should show right on top of the shelf.
  EXPECT_EQ(
      root_window->bounds().bottom() - ash::ShelfConfig::Get()->shelf_size(),
      menu->GetWidget()->GetNativeWindow()->bounds().bottom());
  // Close menu.
  PressDoneButton(menu);
  // Set shelf to auto hide.
  shelf->SetAutoHideBehavior(ash::ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(shelf->IsVisible());
  menu = ShowButtonOptionsMenu(tap_action_two_);
  // Menu should align to the bottom of the root window if the shelf is hidden.
  EXPECT_EQ(root_window->bounds().bottom(),
            menu->GetWidget()->GetNativeWindow()->bounds().bottom());
}

}  // namespace arc::input_overlay
