// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_listener.h"

#include <utility>

#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"

namespace tabs_api {

ToyTabDragSessionListener::ToyTabDragSessionListener() = default;
ToyTabDragSessionListener::~ToyTabDragSessionListener() = default;

void ToyTabDragSessionListener::OnSessionStarted(
    std::vector<tabs_api::NodeId> dragged_tabs,
    TabDragWindowAdapter* source_window) {
  events_.push_back({.type = Event::Type::kStarted,
                     .window = source_window,
                     .dragged_tabs = std::move(dragged_tabs)});
}

void ToyTabDragSessionListener::OnTargetWindowChanged(
    TabDragWindowAdapter* new_target,
    const gfx::Point& screen_point) {
  events_.push_back({.type = Event::Type::kTargetChanged,
                     .window = new_target,
                     .point = screen_point});
}

void ToyTabDragSessionListener::OnDragMoved(const gfx::Point& screen_point) {
  events_.push_back({.type = Event::Type::kMoved, .point = screen_point});
}

void ToyTabDragSessionListener::OnSessionDropped(
    const gfx::Point& screen_point) {
  events_.push_back({.type = Event::Type::kDropped, .point = screen_point});
}

void ToyTabDragSessionListener::OnSessionCancelled() {
  events_.push_back({.type = Event::Type::kCancelled});
}

}  // namespace tabs_api
