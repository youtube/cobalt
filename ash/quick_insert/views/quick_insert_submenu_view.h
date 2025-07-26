// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SUBMENU_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SUBMENU_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class QuickInsertListItemView;
class QuickInsertSectionView;

// View for a Quick Insert submenu, which shows a list of results in a bubble
// outside the main Quick Insert container.
class ASH_EXPORT QuickInsertSubmenuView
    : public views::WidgetDelegateView,
      public QuickInsertTraversableItemContainer {
  METADATA_HEADER(QuickInsertSubmenuView, views::WidgetDelegateView)

 public:
  QuickInsertSubmenuView(
      const gfx::Rect& anchor_rect,
      std::vector<std::unique_ptr<QuickInsertListItemView>> items);
  QuickInsertSubmenuView(const QuickInsertSubmenuView&) = delete;
  QuickInsertSubmenuView& operator=(const QuickInsertSubmenuView&) = delete;
  ~QuickInsertSubmenuView() override;

  // views::WidgetDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
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

 private:
  gfx::Rect GetDesiredBounds(gfx::Rect anchor_rect);

  // Section which contains the submenu items.
  raw_ptr<QuickInsertSectionView> section_view_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SUBMENU_VIEW_H_
