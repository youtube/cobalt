// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/color_picker_view.h"

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/canvas.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {

static const TabGroupEditorBubbleView::Colors kTestColors({
    {tab_groups::TabGroupColorId::kRed, u"Red"},
    {tab_groups::TabGroupColorId::kGreen, u"Green"},
    {tab_groups::TabGroupColorId::kBlue, u"Blue"},
});

}  // namespace

class ColorPickerViewTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ = CreateTestWidget();
    bubble_view_ = std::make_unique<views::BubbleDialogDelegateView>();

    auto color_picker = std::make_unique<ColorPickerView>(
        bubble_view(), kTestColors, tab_groups::TabGroupColorId::kBlue,
        color_selected_callback_.Get());
    color_picker->SizeToPreferredSize();
    color_picker_ = widget_->SetContentsView(std::move(color_picker));
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  void ClickColorElement(views::Button* element) {
    gfx::Point center = element->GetLocalBounds().CenterPoint();
    gfx::Point root_center = center;
    views::View::ConvertPointToWidget(color_picker_, &root_center);

    ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, center, root_center,
                                 base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                                 0);
    element->OnMousePressed(pressed_event);

    ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, center, root_center,
                                  base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                                  0);
    element->OnMouseReleased(released_event);
  }

  void ClickColorAtIndex(int index) {
    ClickColorElement(color_picker_->GetElementAtIndexForTesting(index));
  }

  const views::BubbleDialogDelegateView* bubble_view() {
    return bubble_view_.get();
  }

  ::testing::NiceMock<
      base::MockCallback<ColorPickerView::ColorSelectedCallback>>
      color_selected_callback_;
  raw_ptr<ColorPickerView> color_picker_;

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<views::BubbleDialogDelegateView> bubble_view_;
};

TEST_F(ColorPickerViewTest, ColorSelectedByDefaultIfMatching) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  ColorPickerView* color_picker =
      widget->SetContentsView(std::make_unique<ColorPickerView>(
          bubble_view(), kTestColors, tab_groups::TabGroupColorId::kRed,
          color_selected_callback_.Get()));

  color_picker->SizeToPreferredSize();

  EXPECT_TRUE(color_picker->GetSelectedElement().has_value());
  // Expect the index to match that of TabGroupId::kRed.
  EXPECT_EQ(color_picker->GetSelectedElement().value(), 0);
}

TEST_F(ColorPickerViewTest, ClickingSelectsColor) {
  ClickColorAtIndex(0);
  EXPECT_EQ(0, color_picker_->GetSelectedElement());

  ClickColorAtIndex(1);
  EXPECT_EQ(1, color_picker_->GetSelectedElement());
}

TEST_F(ColorPickerViewTest, ColorNotDeselected) {
  ClickColorAtIndex(0);
  ClickColorAtIndex(0);
  EXPECT_EQ(0, color_picker_->GetSelectedElement());
}

TEST_F(ColorPickerViewTest, SelectingColorNotifiesCallback) {
  EXPECT_CALL(color_selected_callback_, Run()).Times(2);

  ClickColorAtIndex(0);
  ClickColorAtIndex(1);
}

TEST_F(ColorPickerViewTest, CallbackNotifiedOnce) {
  EXPECT_CALL(color_selected_callback_, Run()).Times(1);

  ClickColorAtIndex(0);
  ClickColorAtIndex(0);
}

TEST_F(ColorPickerViewTest, KeyboardFocusBehavesLikeRadioButtons) {
  views::FocusManager* focus_manager = color_picker_->GetFocusManager();

  // Focus should start at the selected element.
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(color_picker_->GetElementAtIndexForTesting(2),
            focus_manager->GetFocusedView());

  // Pressing arrow keys should cycle through the elements.
  ui::KeyEvent arrow_event(
      ui::EventType::ET_KEY_PRESSED,
      ui::DomCodeToUsLayoutKeyboardCode(ui::DomCode::ARROW_RIGHT),
      ui::DomCode::ARROW_RIGHT, ui::EF_NONE);
  EXPECT_FALSE(focus_manager->OnKeyEvent(arrow_event));
  EXPECT_EQ(color_picker_->GetElementAtIndexForTesting(0),
            focus_manager->GetFocusedView());

  focus_manager->ClearFocus();
  ClickColorAtIndex(1);

  // Re-entering should restore focus to the currently selected color.
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(color_picker_->GetElementAtIndexForTesting(1),
            focus_manager->GetFocusedView());
}
