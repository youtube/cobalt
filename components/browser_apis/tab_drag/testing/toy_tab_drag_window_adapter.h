// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "ui/gfx/geometry/rect.h"

namespace tabs_api {

class TabDragWindowRegistry;

class ToyTabDragWindowAdapter : public TabDragWindowAdapter {
 public:
  ToyTabDragWindowAdapter(const gfx::Rect& bounds,
                          TabDragWindowRegistry* registry);
  ToyTabDragWindowAdapter(const ToyTabDragWindowAdapter&) = delete;
  ToyTabDragWindowAdapter& operator=(const ToyTabDragWindowAdapter&) = delete;
  ~ToyTabDragWindowAdapter() override;

  // TabDragWindowAdapter:
  TabDragWindowId GetWindowId() const override { return id_; }
  gfx::Rect GetBoundsInScreen() const override;
  gfx::Point ConvertScreenPointToLocal(
      gfx::NativeView target_view,
      const gfx::Point& screen_point) const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;

 private:
  gfx::Rect bounds_;
  bool has_capture_ = false;
  TabDragWindowId id_;
  raw_ptr<TabDragWindowRegistry> registry_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_
