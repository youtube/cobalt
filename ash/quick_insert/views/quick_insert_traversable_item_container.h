// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_TRAVERSABLE_ITEM_CONTAINER_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_TRAVERSABLE_ITEM_CONTAINER_H_

#include "ash/ash_export.h"

namespace views {
class View;
}

namespace ash {

// Interface implemented by Quick Insert item containers that support item
// traversal.
class ASH_EXPORT QuickInsertTraversableItemContainer {
 public:
  virtual ~QuickInsertTraversableItemContainer() = default;

  // Returns the item to highlight to when navigating to this container from the
  // top, or nullptr if the container is empty.
  virtual views::View* GetTopItem() = 0;

  // Returns the item to highlight to when navigating to this container from the
  // bottom, or nullptr if the container is empty.
  virtual views::View* GetBottomItem() = 0;

  // Returns the item directly above `item`, or nullptr if there is no such item
  // in the container.
  virtual views::View* GetItemAbove(views::View* item) = 0;

  // Returns the item directly below `item`, or nullptr if there is no such item
  // in the container.
  virtual views::View* GetItemBelow(views::View* item) = 0;

  // Returns the item directly to the left of `item`, or nullptr if there is no
  // such item in the container.
  virtual views::View* GetItemLeftOf(views::View* item) = 0;

  // Returns the item directly to the right of `item`, or nullptr if there is no
  // such item in the container.
  virtual views::View* GetItemRightOf(views::View* item) = 0;

  virtual bool ContainsItem(views::View* item) = 0;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_TRAVERSABLE_ITEM_CONTAINER_H_
