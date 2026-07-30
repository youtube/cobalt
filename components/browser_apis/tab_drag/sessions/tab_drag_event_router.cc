// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_event_router.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace tabs_api {

TabDragEventRouter::TabDragEventRouter(DropTargetRegistry& registry)
    : registry_(registry) {}

TabDragEventRouter::~TabDragEventRouter() = default;

void TabDragEventRouter::OnSessionStarted(
    std::vector<tabs_api::NodeId> dragged_tabs,
    TabDragWindowAdapter* source_window) {
  dragged_tabs_ = std::move(dragged_tabs);
}

void TabDragEventRouter::OnTargetWindowChanged(TabDragWindowAdapter* new_target,
                                               const gfx::Point& screen_point) {
  TransitionToTargetWindow(new_target, screen_point);
}

void TabDragEventRouter::OnDragMoved(const gfx::Point& screen_point) {
  if (current_drop_target_window_) {
    DispatchEvent(current_drop_target_window_, DropTargetEvent::kDrag,
                  screen_point);
  }
}

void TabDragEventRouter::OnSessionDropped(const gfx::Point& screen_point) {
  if (current_drop_target_window_) {
    DispatchEvent(current_drop_target_window_, DropTargetEvent::kDrop,
                  screen_point);
    current_drop_target_window_ = nullptr;
  }
  dragged_tabs_.clear();
}

void TabDragEventRouter::OnSessionCancelled() {
  if (current_drop_target_window_) {
    DispatchEvent(current_drop_target_window_, DropTargetEvent::kCancelled);
    current_drop_target_window_ = nullptr;
  }
  dragged_tabs_.clear();
}

void TabDragEventRouter::TransitionToTargetWindow(
    TabDragWindowAdapter* new_target,
    const gfx::Point& screen_point) {
  if (current_drop_target_window_) {
    DispatchEvent(current_drop_target_window_, DropTargetEvent::kLeave);
  }
  current_drop_target_window_ = new_target;
  if (current_drop_target_window_) {
    DispatchEvent(current_drop_target_window_, DropTargetEvent::kEntered,
                  screen_point);
  }
}

void TabDragEventRouter::DispatchEvent(TabDragWindowAdapter* window,
                                       DropTargetEvent event,
                                       const gfx::Point& screen_point) {
  auto target_opt = registry_->GetDropTarget(window);
  mojom::DropTarget* target = target_opt ? &target_opt->get() : nullptr;
  if (!target) {
    return;
  }

  switch (event) {
    case DropTargetEvent::kEntered:
      target->OnDragEntered(dragged_tabs_,
                            window->ConvertScreenPointToLocal(screen_point));
      break;
    case DropTargetEvent::kDrag:
      target->OnDrag(window->ConvertScreenPointToLocal(screen_point));
      break;
    case DropTargetEvent::kLeave:
      target->OnDragLeave();
      break;
    case DropTargetEvent::kDrop:
      target->OnDrop(dragged_tabs_,
                     window->ConvertScreenPointToLocal(screen_point));
      break;
    case DropTargetEvent::kCancelled:
      target->OnDragCancelled();
      break;
  }
}

}  // namespace tabs_api
