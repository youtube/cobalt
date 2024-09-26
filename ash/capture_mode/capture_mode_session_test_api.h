// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "capture_mode_session_focus_cycler.h"

namespace ash {

class CaptureLabelView;
class CaptureModeBarView;
class CaptureModeSession;
class CaptureModeSettingsView;
class MagnifierGlass;
class RecordingTypeMenuView;
class UserNudgeController;

// Wrapper for CaptureModeSession that exposes internal state to test functions.
class CaptureModeSessionTestApi {
 public:
  CaptureModeSessionTestApi();
  explicit CaptureModeSessionTestApi(CaptureModeSession* session);

  CaptureModeSessionTestApi(CaptureModeSessionTestApi&) = delete;
  CaptureModeSessionTestApi& operator=(CaptureModeSessionTestApi&) = delete;
  ~CaptureModeSessionTestApi() = default;

  CaptureModeBarView* GetCaptureModeBarView();

  CaptureModeSettingsView* GetCaptureModeSettingsView();

  CaptureLabelView* GetCaptureLabelView();

  RecordingTypeMenuView* GetRecordingTypeMenuView();

  views::Widget* GetCaptureModeSettingsWidget();

  views::Widget* GetCaptureLabelWidget();

  views::Widget* GetRecordingTypeMenuWidget();

  views::Widget* GetDimensionsLabelWidget();

  UserNudgeController* GetUserNudgeController();

  MagnifierGlass& GetMagnifierGlass();

  bool IsUsingCustomCursor(CaptureModeType type);

  CaptureModeSessionFocusCycler::FocusGroup GetCurrentFocusGroup();

  size_t GetCurrentFocusIndex();

  CaptureModeSessionFocusCycler::HighlightableWindow* GetHighlightableWindow(
      aura::Window* window);

  CaptureModeSessionFocusCycler::HighlightableView* GetCurrentFocusedView();

  // Returns false if `current_focus_group_` equals to `kNone` which means
  // there's no focus on any focus group for now. Otherwise, returns true;
  bool HasFocus();

  bool IsFolderSelectionDialogShown();

  // Returns true if all UIs (cursors, widgets, and paintings on the layer) of
  // the capture mode session is visible.
  bool IsAllUisVisible();

 private:
  const raw_ptr<CaptureModeSession, ExperimentalAsh> session_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_
