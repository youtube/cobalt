// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_ACTIONABLE_VIEW_H_
#define ASH_SYSTEM_TRAY_ACTIONABLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_popup_ink_drop_style.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// A focusable view that performs an action when user clicks on it, or presses
// enter or space when focused. Note that the action is triggered on mouse-up,
// instead of on mouse-down. So if user presses the mouse on the view, then
// moves the mouse out of the view and then releases, then the action will not
// be performed.
// Exported for SystemTray.
//
// TODO(bruthig): Consider removing ActionableView and make clients use
// Buttons instead. (See crbug.com/614453)
class ASH_EXPORT ActionableView : public views::Button {
 public:
  static const char kViewClassName[];

  explicit ActionableView(TrayPopupInkDropStyle ink_drop_style);

  ActionableView(const ActionableView&) = delete;
  ActionableView& operator=(const ActionableView&) = delete;

  ~ActionableView() override;

 protected:
  // Performs an action when user clicks on the view (on mouse-press event), or
  // presses a key when this view is in focus. Returns true if the event has
  // been handled and an action was performed. Returns false otherwise.
  virtual bool PerformAction(const ui::Event& event) = 0;

  // Called after PerformAction() to act upon its result, including showing
  // appropriate ink drop ripple. This will not get called if the view is
  // destroyed during PerformAction(). Default implementation shows triggered
  // ripple if action is performed or hides existing ripple if no action is
  // performed. Subclasses can override to change the default behavior.
  virtual void HandlePerformActionResult(bool action_performed,
                                         const ui::Event& event);

  // Overridden from views::Button.
  const char* GetClassName() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  void ButtonPressed(const ui::Event& event);

  // Used by ButtonPressed() to determine whether |this| has been destroyed as a
  // result of performing the associated action. This is necessary because in
  // the not-destroyed case ButtonPressed() uses member variables.
  raw_ptr<bool, ExperimentalAsh> destroyed_ = nullptr;

  // Defines the flavor of ink drop ripple/highlight that should be constructed.
  const TrayPopupInkDropStyle ink_drop_style_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_ACTIONABLE_VIEW_H_
