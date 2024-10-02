// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_
#define ASH_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace ash {

class AppListFolderView;

// An invisible background view of the folder in fullscreen app list. It is used
// to close folder when the user clicks/taps outside the opened folder.
class FolderBackgroundView : public views::View {
 public:
  explicit FolderBackgroundView(AppListFolderView* folder_view);

  FolderBackgroundView(const FolderBackgroundView&) = delete;
  FolderBackgroundView& operator=(const FolderBackgroundView&) = delete;

  ~FolderBackgroundView() override;

  void set_folder_view(AppListFolderView* folder_view) {
    folder_view_ = folder_view;
  }

  // views::View:
  const char* GetClassName() const override;

 private:
  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Handles mouse click event or gesture tap event.
  void HandleClickOrTap();

  raw_ptr<AppListFolderView, DanglingUntriaged | ExperimentalAsh> folder_view_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_
