// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_SHUTDOWN_CONFIRMATION_BUBBLE_H_
#define ASH_SHELF_SHELF_SHUTDOWN_CONFIRMATION_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_bubble.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace views {
class BubbleDialogDelegateView;
class View;
}  // namespace views

namespace ash {

// The implementation of tooltip bubbles for the shelf.
class ASH_EXPORT ShelfShutdownConfirmationBubble : public ShelfBubble {
 public:
  enum class ButtonId {
    // We start from 1 because 0 is the default view ID.
    kShutdown = 1,  // Shut down the device.
    kCancel,        // Cancel shutdown.
  };
  // Enum used for UMA. Do NOT reorder or remove entry. Don't forget to
  // update ShutdownConfirmationBubbleAction enum in enums.xml when adding new
  // entries.
  enum class BubbleAction {
    kOpened = 0,
    kCancelled = 1,
    kConfirmed = 2,
    kDismissed = 3,
    kMaxValue = kDismissed
  };

  ShelfShutdownConfirmationBubble(views::View* anchor,
                                  ShelfAlignment alignment,
                                  base::OnceClosure on_confirm_callback,
                                  base::OnceClosure on_cancel_callback);

  ShelfShutdownConfirmationBubble(const ShelfShutdownConfirmationBubble&) =
      delete;
  ShelfShutdownConfirmationBubble& operator=(
      const ShelfShutdownConfirmationBubble&) = delete;
  ~ShelfShutdownConfirmationBubble() override;

  // views::View:
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::u16string GetAccessibleWindowTitle() const override;

 protected:
  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

 private:
  // Callback functions of cancel and confirm buttons
  void OnCancelled();
  void OnConfirmed();
  void OnClosed();
  base::OnceClosure confirm_callback_;
  base::OnceClosure cancel_callback_;

  // Report bubble action metrics
  void ReportBubbleAction(BubbleAction action);

  raw_ptr<views::ImageView, ExperimentalAsh> icon_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> title_ = nullptr;
  raw_ptr<views::LabelButton, ExperimentalAsh> cancel_ = nullptr;
  raw_ptr<views::LabelButton, ExperimentalAsh> confirm_ = nullptr;

  enum class DialogResult { kNone, kCancelled, kConfirmed };

  // A simple state machine to keep track of the dialog result.
  DialogResult dialog_result_{DialogResult::kNone};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_SHUTDOWN_CONFIRMATION_BUBBLE_H_
