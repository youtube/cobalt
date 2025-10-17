// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PSEUDO_FOCUS_HANDLER_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PSEUDO_FOCUS_HANDLER_H_

#include "ash/ash_export.h"

namespace ash {

enum class QuickInsertPseudoFocusDirection;

// Interface for classes that have pseudo focusable elements, which can look and
// behave as if they were focused without having actual focus. We use "pseudo
// focus" since actual view focus generally stays on the Quick Insert search
// field, which just forwards user actions to be handled by pseudo focused
// elements if needed (e.g. to select an item when the user presses the enter
// key).
class ASH_EXPORT QuickInsertPseudoFocusHandler {
 public:
  virtual ~QuickInsertPseudoFocusHandler() = default;

  // Returns true if an action was performed.
  virtual bool DoPseudoFocusedAction() = 0;

  // Moves pseudo focus to the pseudo focusable element in the specified
  // direction, or returns false if there is no such element.
  virtual bool MovePseudoFocusUp() = 0;
  virtual bool MovePseudoFocusDown() = 0;
  virtual bool MovePseudoFocusLeft() = 0;
  virtual bool MovePseudoFocusRight() = 0;

  // Moves pseudo focus to the next (or previous) pseudo focusable element, or
  // returns false if there is no such element.
  virtual bool AdvancePseudoFocus(
      QuickInsertPseudoFocusDirection direction) = 0;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PSEUDO_FOCUS_HANDLER_H_
