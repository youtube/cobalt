// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GRID_TEST_API_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GRID_TEST_API_H_

#include "base/memory/raw_ptr.h"

#include "ash/wm/overview/overview_grid.h"

namespace aura {
class Window;
}

namespace ash {
class BirchChipButton;
class SavedDeskSaveDeskButton;
class SavedDeskSaveDeskButtonContainer;

class OverviewGridTestApi {
 public:
  explicit OverviewGridTestApi(OverviewGrid* overview_grid);
  explicit OverviewGridTestApi(aura::Window* root);
  OverviewGridTestApi(const OverviewGridTestApi&) = delete;
  OverviewGridTestApi& operator=(const OverviewGridTestApi&) = delete;
  ~OverviewGridTestApi();

  const gfx::Rect bounds() const { return overview_grid_->bounds_; }
  float scroll_offset() const { return overview_grid_->scroll_offset_; }
  views::Widget* informed_restore_widget() {
    return overview_grid_->informed_restore_widget_.get();
  }
  const views::Widget* birch_bar_widget() const {
    return overview_grid_->birch_bar_widget_.get();
  }
  BirchBarView* birch_bar_view() { return overview_grid_->birch_bar_view_; }

  int num_incognito_windows() const {
    return overview_grid_->num_incognito_windows_;
  }
  int num_unsupported_windows() const {
    return overview_grid_->num_unsupported_windows_;
  }

  bool IsSaveDeskAsTemplateButtonVisible() const;
  bool IsSaveDeskForLaterButtonVisible() const;

  // Returns the save desk as template button or save desk for later button if
  // available, otherwise null.
  SavedDeskSaveDeskButton* GetSaveDeskAsTemplateButton();
  SavedDeskSaveDeskButton* GetSaveDeskForLaterButton();

  // Returns the save button container if available, otherwise null.
  SavedDeskSaveDeskButtonContainer* GetSaveDeskButtonContainer();
  const SavedDeskSaveDeskButtonContainer* GetSaveDeskButtonContainer() const;

  const std::vector<raw_ptr<BirchChipButtonBase>>& GetBirchChips() const;

 private:
  const raw_ptr<OverviewGrid> overview_grid_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GRID_TEST_API_H_
