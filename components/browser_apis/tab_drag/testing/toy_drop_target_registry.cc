// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/testing/toy_drop_target_registry.h"

#include <utility>

#include "build/build_config.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/drop_target.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_ui_types.h"

namespace tabs_api {

ToyDropTargetRegistry::ToyDropTargetRegistry() : target_id_(1) {}
ToyDropTargetRegistry::~ToyDropTargetRegistry() = default;

DropTargetId ToyDropTargetRegistry::RegisterDropTarget(
    TabDragWindowAdapter* window,
    gfx::NativeView native_view,
    mojo::PendingAssociatedRemote<mojom::DropTarget> target,
    mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
        registration) {
  return DropTargetId();
}

void ToyDropTargetRegistry::UnregisterDropTarget(DropTargetId target_id) {}

DropTargetId ToyDropTargetRegistry::FindTargetAtPoint(
    const gfx::Point& screen_point,
    DropTargetId exclude_target) const {
  if (target_window_ && exclude_target != target_id_) {
    auto* target = GetDropTarget(target_id_);
    if (target) {
      gfx::Point local_point = target_window_->ConvertScreenPointToLocal(
          target->native_view(), screen_point);
      auto bounds_opt = target->cached_bounds();
      if (bounds_opt) {
        if (bounds_opt->Contains(local_point)) {
          return target_id_;
        }
      } else {
        if (target_window_->GetBoundsInScreen().Contains(screen_point)) {
          return target_id_;
        }
      }
    }
  }
  return DropTargetId();
}

DropTargetId ToyDropTargetRegistry::FindTargetForWindow(
    TabDragWindowId window_id) const {
  if (target_window_ && target_window_->GetWindowId() == window_id) {
    return target_id_;
  }
  if (source_window_ && source_window_->GetWindowId() == window_id) {
    return source_id_;
  }
  return DropTargetId();
}

DropTarget* ToyDropTargetRegistry::GetDropTarget(DropTargetId target_id) const {
  if (target_id == target_id_) {
    return target_object_.get();
  }
  if (target_id == source_id_) {
    return source_object_.get();
  }
  return nullptr;
}

void ToyDropTargetRegistry::set_target_window(ToyTabDragWindowAdapter* window) {
  target_window_ = window;
  if (window) {
    mojo::PendingAssociatedRemote<mojom::DropTarget> null_remote;
    target_object_ = std::make_unique<DropTarget>(
        target_id_, window, gfx::NativeView(), std::move(null_remote));
    target_object_->set_cached_bounds(
        gfx::Rect(window->GetBoundsInScreen().size()));
  } else {
    target_object_.reset();
  }
}

void ToyDropTargetRegistry::set_source_window(ToyTabDragWindowAdapter* window) {
  source_window_ = window;
  if (window) {
    mojo::PendingAssociatedRemote<mojom::DropTarget> null_remote;
    source_object_ = std::make_unique<DropTarget>(
        source_id_, window, gfx::NativeView(), std::move(null_remote));
    source_object_->set_cached_bounds(
        gfx::Rect(window->GetBoundsInScreen().size()));
  } else {
    source_object_.reset();
  }
}

std::optional<gfx::Rect> ToyDropTargetRegistry::GetCachedBounds(
    DropTargetId target_id) const {
  auto* target = GetDropTarget(target_id);
  return target ? target->cached_bounds() : std::nullopt;
}

void ToyDropTargetRegistry::UpdateTargetBounds(DropTargetId target_id,
                                               const gfx::Rect& bounds) {
  auto* target = GetDropTarget(target_id);
  if (target) {
    target->set_cached_bounds(bounds);
  }
}

}  // namespace tabs_api
