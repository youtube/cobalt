// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_ROW_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_ROW_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class View;
}  // namespace views

namespace ash {

class QuickInsertImageItemView;

// Container view for a single row of image items in a section.
class ASH_EXPORT QuickInsertImageItemRowView
    : public views::BoxLayoutView,
      public QuickInsertTraversableItemContainer {
  METADATA_HEADER(QuickInsertImageItemRowView, views::BoxLayoutView)

 public:
  explicit QuickInsertImageItemRowView(
      base::RepeatingClosure more_items_button = {},
      std::u16string more_items_accessible_name = u"");
  QuickInsertImageItemRowView(const QuickInsertImageItemRowView&) = delete;
  QuickInsertImageItemRowView& operator=(const QuickInsertImageItemRowView&) =
      delete;
  ~QuickInsertImageItemRowView() override;

  void SetLeadingIcon(const ui::ImageModel& icon);
  QuickInsertImageItemView* AddImageItem(
      std::unique_ptr<QuickInsertImageItemView> image_item);

  // views::BoxLayoutView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // QuickInsertTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  views::View::Views GetItems() const;
  [[nodiscard]] base::CallbackListSubscription AddItemsChangedCallback(
      views::PropertyChangedCallback callback);

  views::ImageButton* GetMoreItemsButtonForTesting() {
    return more_items_button_;
  }

 private:
  views::View* GetLeftmostItem();

  raw_ptr<views::ImageView> leading_icon_view_ = nullptr;
  raw_ptr<views::View> items_container_ = nullptr;
  raw_ptr<views::ImageButton> more_items_button_ = nullptr;
  base::RepeatingClosureList on_items_changed_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT,
                   QuickInsertImageItemRowView,
                   views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::QuickInsertImageItemRowView)

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_ROW_VIEW_H_
