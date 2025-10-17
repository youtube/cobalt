// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PSEUDO_FOCUS_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PSEUDO_FOCUS_H_

#include "ash/ash_export.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// Direction to traverse pseudo focusable elements.
enum class QuickInsertPseudoFocusDirection {
  kForward,
  kBackward,
};

void ASH_EXPORT ApplyQuickInsertPseudoFocusToView(views::View* view);

void ASH_EXPORT RemoveQuickInsertPseudoFocusFromView(views::View* view);

bool ASH_EXPORT DoQuickInsertPseudoFocusedActionOnView(views::View* view);

// Gets the next pseudo focusable view in `direction`. The order of pseudo focus
// traversal is similar to usual view focus traversal, except the textfield is
// skipped (since the textfield keeps actual focus while Quick Insert is
// open). If `should_loop` is true, the next pseudo focusable view loops (i.e.
// returns to the start/end). If `should_loop` is false, then nullptr is instead
// returned at the start/end.
views::View* ASH_EXPORT
GetNextQuickInsertPseudoFocusableView(views::View* view,
                                      QuickInsertPseudoFocusDirection direction,
                                      bool should_loop);

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PSEUDO_FOCUS_H_
