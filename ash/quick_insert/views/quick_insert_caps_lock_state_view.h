// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_CAPS_LOCK_STATE_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_CAPS_LOCK_STATE_VIEW_H_

#include "ash/ash_export.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class Rect;
}

namespace views {
class ImageView;
}

namespace ash {

// A bubble view to display CapsLock state when it is toggled.
class ASH_EXPORT QuickInsertCapsLockStateView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(QuickInsertCapsLockStateView, views::BubbleDialogDelegateView)

 public:
  explicit QuickInsertCapsLockStateView(
      gfx::NativeView parent,
      bool enabled,
      gfx::Rect caret_bounds,
      base::i18n::TextDirection text_direction =
          base::i18n::TextDirection::UNKNOWN_DIRECTION);
  QuickInsertCapsLockStateView(const QuickInsertCapsLockStateView&) = delete;
  QuickInsertCapsLockStateView& operator=(const QuickInsertCapsLockStateView&) =
      delete;
  ~QuickInsertCapsLockStateView() override;

  void Close();
  void Show();

  views::ImageView& icon_view_for_testing() const { return *icon_view_; }

 private:
  raw_ptr<views::ImageView> icon_view_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_CAPS_LOCK_STATE_VIEW_H_
