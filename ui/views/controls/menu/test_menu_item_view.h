// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_TEST_MENU_ITEM_VIEW_H_
#define UI_VIEWS_CONTROLS_MENU_TEST_MENU_ITEM_VIEW_H_

#include "ui/views/controls/menu/menu_item_view.h"

namespace views {

class MenuDelegate;

// A MenuItemView implementation with a public destructor (so we can clean up
// in tests).
class TestMenuItemView : public MenuItemView {
 public:
  TestMenuItemView();
  explicit TestMenuItemView(MenuDelegate* delegate);

  TestMenuItemView(const TestMenuItemView&) = delete;
  TestMenuItemView& operator=(const TestMenuItemView&) = delete;

  ~TestMenuItemView() override;

  using MenuItemView::AddEmptyMenus;

  void set_has_mnemonics(bool has_mnemonics) { has_mnemonics_ = has_mnemonics; }

  bool show_mnemonics() { return show_mnemonics_; }

  static ImageView* submenu_arrow_image_view(MenuItemView* view) {
    return view->submenu_arrow_image_view_;
  }
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_TEST_MENU_ITEM_VIEW_H_
