// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_

#include "base/memory/weak_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "ui/gfx/geometry/rect.h"

namespace tabs_api {

class ToyTabDragWindowAdapter : public TabDragWindowAdapter {
 public:
  explicit ToyTabDragWindowAdapter(const gfx::Rect& bounds);
  ToyTabDragWindowAdapter(const ToyTabDragWindowAdapter&) = delete;
  ToyTabDragWindowAdapter& operator=(const ToyTabDragWindowAdapter&) = delete;
  ~ToyTabDragWindowAdapter() override;

  // TabDragWindowAdapter:
  gfx::Rect GetBoundsInScreen() const override;
  gfx::Point ConvertScreenPointToLocal(
      const gfx::Point& screen_point) const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  base::WeakPtr<TabDragWindowAdapter> AsWeakPtr() override;

 private:
  gfx::Rect bounds_;
  bool has_capture_ = false;
  base::WeakPtrFactory<ToyTabDragWindowAdapter> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_
