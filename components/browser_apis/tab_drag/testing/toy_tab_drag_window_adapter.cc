// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"

namespace tabs_api {

ToyTabDragWindowAdapter::ToyTabDragWindowAdapter(const gfx::Rect& bounds)
    : bounds_(bounds) {}

ToyTabDragWindowAdapter::~ToyTabDragWindowAdapter() = default;

gfx::Rect ToyTabDragWindowAdapter::GetBoundsInScreen() const {
  return bounds_;
}

gfx::Point ToyTabDragWindowAdapter::ConvertScreenPointToLocal(
    const gfx::Point& screen_point) const {
  return screen_point - bounds_.OffsetFromOrigin();
}

void ToyTabDragWindowAdapter::SetCapture() {
  has_capture_ = true;
}

void ToyTabDragWindowAdapter::ReleaseCapture() {
  has_capture_ = false;
}

bool ToyTabDragWindowAdapter::HasCapture() const {
  return has_capture_;
}

base::WeakPtr<TabDragWindowAdapter> ToyTabDragWindowAdapter::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace tabs_api
