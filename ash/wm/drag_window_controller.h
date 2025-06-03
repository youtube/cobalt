// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DRAG_WINDOW_CONTROLLER_H_
#define ASH_WM_DRAG_WINDOW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ui {
class Shadow;
}

namespace ash {

// Handles visuals for window dragging as it relates to multi-display support.
// Phantom windows called "drag windows" represent the window on other displays.
class ASH_EXPORT DragWindowController {
 public:
  DragWindowController(
      aura::Window* window,
      bool is_touch_dragging,
      const absl::optional<gfx::Rect>& shadow_bounds = absl::nullopt);
  DragWindowController(const DragWindowController&) = delete;
  DragWindowController& operator=(const DragWindowController&) = delete;
  virtual ~DragWindowController();

  // Updates bounds and opacity for the drag windows, and creates/destroys each
  // drag window so that it only exists when it would have nonzero opacity. Also
  // updates the opacity of the original window.
  void Update();

  float old_opacity_for_testing() const { return old_opacity_; }

 private:
  class DragWindowDetails;
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest, DragWindowController);
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest,
                           DragWindowControllerAcrossThreeDisplays);
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest,
                           DragWindowControllerWithCustomShadowBounds);

  // Returns the currently active drag windows.
  int GetDragWindowsCountForTest() const;

  // Returns the drag window/layer owner for given index of the
  // currently active drag windows list.
  const aura::Window* GetDragWindowForTest(size_t index) const;
  const ui::Shadow* GetDragWindowShadowForTest(size_t index) const;

  // Call Layer::OnPaintLayer on all layers under the drag_windows_.
  void RequestLayerPaintForTest();

  // The original window.
  raw_ptr<aura::Window, ExperimentalAsh> window_;

  // Indicates touch dragging, as opposed to mouse dragging.
  const bool is_touch_dragging_;

  // Used if the drag windows may need their shadows adjusted.
  const absl::optional<gfx::Rect> shadow_bounds_;

  // |window_|'s opacity before the drag. Used to revert opacity after the drag.
  const float old_opacity_;

  // Phantom windows to visually represent |window_| on other displays.
  std::vector<std::unique_ptr<DragWindowDetails>> drag_windows_;
};

}  // namespace ash

#endif  // ASH_WM_DRAG_WINDOW_CONTROLLER_H_
