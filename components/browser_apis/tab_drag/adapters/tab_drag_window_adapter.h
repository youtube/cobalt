// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_

#include "base/types/id_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace tabs_api {

using TabDragWindowId = base::IdType64<class TabDragWindowTag>;

// Represents a browser window for the TabDragAPI.
class TabDragWindowAdapter {
 public:
  virtual ~TabDragWindowAdapter() = default;

  virtual TabDragWindowId GetWindowId() const = 0;

  // Returns the window bounds in screen coordinates.
  virtual gfx::Rect GetBoundsInScreen() const = 0;

  // Converts a point in screen coordinates to local coordinates relative to the
  // given `target_view`.
  virtual gfx::Point ConvertScreenPointToLocal(
      gfx::NativeView target_view,
      const gfx::Point& screen_point) const = 0;

  // Acquires input capture for this window.
  virtual void SetCapture() = 0;

  // Releases input capture from this window.
  virtual void ReleaseCapture() = 0;

  // Returns true if this window has capture.
  virtual bool HasCapture() const = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_
