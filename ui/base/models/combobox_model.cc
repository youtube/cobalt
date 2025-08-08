// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/combobox_model.h"

#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/image_model.h"

namespace ui {

ComboboxModel::ComboboxModel() = default;

ComboboxModel::~ComboboxModel() {
  for (auto& observer : observers_)
    observer.OnComboboxModelDestroying(this);
}

std::u16string ComboboxModel::GetDropDownTextAt(size_t index) const {
  return GetItemAt(index);
}

std::u16string ComboboxModel::GetDropDownSecondaryTextAt(size_t index) const {
  return std::u16string();
}

ImageModel ComboboxModel::GetIconAt(size_t index) const {
  return ui::ImageModel();
}

ImageModel ComboboxModel::GetDropDownIconAt(size_t index) const {
  return GetIconAt(index);
}

bool ComboboxModel::IsItemSeparatorAt(size_t index) const {
  return false;
}

absl::optional<size_t> ComboboxModel::GetDefaultIndex() const {
  return size_t{0};
}

bool ComboboxModel::IsItemEnabledAt(size_t index) const {
  return true;
}

void ComboboxModel::AddObserver(ComboboxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ComboboxModel::RemoveObserver(ComboboxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

absl::optional<ui::ColorId> ComboboxModel::GetDropdownForegroundColorIdAt(
    size_t index) const {
  return absl::nullopt;
}

absl::optional<ui::ColorId> ComboboxModel::GetDropdownBackgroundColorIdAt(
    size_t index) const {
  return absl::nullopt;
}

absl::optional<ui::ColorId>
ComboboxModel::GetDropdownSelectedBackgroundColorIdAt(size_t index) const {
  return absl::nullopt;
}

}  // namespace ui
