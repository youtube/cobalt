// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_LIST_ITEM_CONTAINER_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_LIST_ITEM_CONTAINER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

class QuickInsertItemWithSubmenuView;
class QuickInsertListItemView;

// Container view for the list items in a section. The list items are displayed
// in a vertical list, each spanning the width of the container.
class ASH_EXPORT QuickInsertListItemContainerView
    : public views::View,
      public QuickInsertTraversableItemContainer {
  METADATA_HEADER(QuickInsertListItemContainerView, views::View)

 public:
  QuickInsertListItemContainerView();
  QuickInsertListItemContainerView(const QuickInsertListItemContainerView&) =
      delete;
  QuickInsertListItemContainerView& operator=(
      const QuickInsertListItemContainerView&) = delete;
  ~QuickInsertListItemContainerView() override;

  // QuickInsertTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  QuickInsertListItemView* AddListItem(
      std::unique_ptr<QuickInsertListItemView> list_item);
  QuickInsertItemWithSubmenuView* AddItemWithSubmenu(
      std::unique_ptr<QuickInsertItemWithSubmenuView> item_with_submenu);

 private:
  views::ViewModelT<views::View> items_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_LIST_ITEM_CONTAINER_VIEW_H_
