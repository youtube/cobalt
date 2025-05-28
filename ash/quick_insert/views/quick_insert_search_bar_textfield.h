// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_BAR_TEXTFIELD_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_BAR_TEXTFIELD_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

class QuickInsertSearchFieldView;

// The textfield in the Quick Insert search bar view.
class ASH_EXPORT QuickInsertSearchBarTextfield : public views::Textfield {
  METADATA_HEADER(QuickInsertSearchBarTextfield, views::Textfield)

 public:
  explicit QuickInsertSearchBarTextfield(
      QuickInsertSearchFieldView* search_field_view);
  QuickInsertSearchBarTextfield(const QuickInsertSearchBarTextfield&) = delete;
  QuickInsertSearchBarTextfield& operator=(
      const QuickInsertSearchBarTextfield&) = delete;
  ~QuickInsertSearchBarTextfield() override;

  void SetShouldShowFocusIndicator(bool should_show_focus_indicator);

 private:
  // The search field view that contains and owns this textfield.
  raw_ptr<QuickInsertSearchFieldView> search_field_view_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, QuickInsertSearchBarTextfield, views::Textfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::QuickInsertSearchBarTextfield)

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_SEARCH_BAR_TEXTFIELD_H_
