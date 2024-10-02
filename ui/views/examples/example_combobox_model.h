// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLE_COMBOBOX_MODEL_H_
#define UI_VIEWS_EXAMPLES_EXAMPLE_COMBOBOX_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/models/combobox_model.h"

namespace views::examples {

class ExampleComboboxModel : public ui::ComboboxModel {
 public:
  ExampleComboboxModel(const char* const* strings, size_t count);

  ExampleComboboxModel(const ExampleComboboxModel&) = delete;
  ExampleComboboxModel& operator=(const ExampleComboboxModel&) = delete;

  ~ExampleComboboxModel() override;

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;

 private:
  const raw_ptr<const char* const> strings_;
  const size_t count_;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_EXAMPLE_COMBOBOX_MODEL_H_
