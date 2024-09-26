// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_NAME_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_NAME_VIEW_H_

#include <string>

#include "ash/wm/desks/desk_textfield.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Defines a textfield styled to normally look like a label. Allows modifying
// the name of its corresponding saved desk.
class SavedDeskNameView : public DeskTextfield {
 public:
  METADATA_HEADER(SavedDeskNameView);

  SavedDeskNameView();
  SavedDeskNameView(const SavedDeskNameView&) = delete;
  SavedDeskNameView& operator=(const SavedDeskNameView&) = delete;
  ~SavedDeskNameView() override;

  static constexpr int kSavedDeskNameViewHeight = 20;

  const absl::optional<std::u16string> temporary_name() const {
    return temporary_name_;
  }

  void SetViewName(const std::u16string& name);
  void SetTemporaryName(const std::u16string& new_name) {
    temporary_name_ = new_name;
  }
  void ResetTemporaryName() { temporary_name_.reset(); }

  // Called when the contents in the textfield change. Updates the preferred
  // size of `this`, which invalidates the layout.
  void OnContentsChanged();

  // DeskTextfield:
  gfx::Size CalculatePreferredSize() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Store the modified text view name if name nudge is removed.
  absl::optional<std::u16string> temporary_name_;
};

BEGIN_VIEW_BUILDER(/* no export */, SavedDeskNameView, DeskTextfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SavedDeskNameView)

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_NAME_VIEW_H_
