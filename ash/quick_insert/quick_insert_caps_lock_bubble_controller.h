// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_CAPS_LOCK_BUBBLE_CONTROLLER_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_CAPS_LOCK_BUBBLE_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/events/event_handler.h"

namespace ui {
class Event;
}

namespace ash {

class QuickInsertCapsLockStateView;

// TODO: b/358248370 - This class is not related to Quick Insert. We should move
// this code to a different directory.
class ASH_EXPORT QuickInsertCapsLockBubbleController
    : public input_method::ImeKeyboard::Observer,
      public ui::EventHandler {
 public:
  explicit QuickInsertCapsLockBubbleController(
      input_method::ImeKeyboard* keyboard);
  QuickInsertCapsLockBubbleController(
      const QuickInsertCapsLockBubbleController&) = delete;
  QuickInsertCapsLockBubbleController& operator=(
      const QuickInsertCapsLockBubbleController&) = delete;
  ~QuickInsertCapsLockBubbleController() override;

  void CloseBubble();

  // input_method::ImeKeyboard::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnLayoutChanging(const std::string& layout_name) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  QuickInsertCapsLockStateView* bubble_view_for_testing() {
    return bubble_view_.get();
  }

 private:
  // Closes the bubble due to an input event.
  void MaybeCloseBubbleByEvent(ui::Event* event);

  raw_ptr<QuickInsertCapsLockStateView> bubble_view_ = nullptr;

  // Timer to close the bubble after a delay.
  base::OneShotTimer bubble_close_timer_;

  // Time that the bubble was last shown.
  base::TimeTicks last_shown_;

  base::ScopedObservation<input_method::ImeKeyboard,
                          input_method::ImeKeyboard::Observer>
      ime_keyboard_observation_{this};

  base::WeakPtrFactory<QuickInsertCapsLockBubbleController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_CAPS_LOCK_BUBBLE_CONTROLLER_H_
