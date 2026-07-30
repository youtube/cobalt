// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/drop_target.h"
#include "components/browser_apis/tab_drag/sessions/drop_target_registry.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"

namespace tabs_api {

TabDragSession::TabDragSession(TabDragSessionParams params,
                               TabDragSessionInjector* injector)
    : dragged_tabs_(std::move(params.source_tab_ids)),
      injector_(CHECK_DEREF(injector)),
      end_callback_(std::move(params.end_callback)),
      start_point_in_screen_(params.start_point),
      last_mouse_screen_point_(params.start_point),
      dragged_window_(params.source_window_id) {
  CHECK(registry());
  CHECK(dragged_window_);
  CHECK(registry()->Get(dragged_window_));
}

base::expected<void, mojo_base::mojom::ErrorPtr> TabDragSession::Start() {
  auto result =
      injector_->GetInputAdapter().StartInputCapture(base::BindRepeating(
          &TabDragSession::OnInputEvent, base::Unretained(this)));
  if (result.has_value()) {
    TabDragWindowAdapter* window = registry()->Get(dragged_window_);
    CHECK(window);
    window->SetCapture();
    injector_->GetSessionListener().OnSessionStarted(
        dragged_tabs_, dragged_window_, start_point_in_screen_);
  }
  return result;
}

TabDragSession::~TabDragSession() {
  if (TabDragWindowAdapter* window = registry()->Get(dragged_window_)) {
    window->ReleaseCapture();
  }
  injector_->GetInputAdapter().ReleaseInputCapture();
}

TabDragWindowRegistry* TabDragSession::registry() const {
  return injector_->GetWindowRegistry();
}

void TabDragSession::EndSession() {
  if (end_callback_) {
    std::move(end_callback_).Run();
  }
}

void TabDragSession::UpdateDraggedWindow(TabDragWindowId new_window_id) {
  CHECK(new_window_id);
  if (TabDragWindowAdapter* window = registry()->Get(dragged_window_)) {
    window->ReleaseCapture();
  }
  dragged_window_ = new_window_id;
  if (TabDragWindowAdapter* window = registry()->Get(dragged_window_)) {
    window->SetCapture();
  }
}

void TabDragSession::OnInputEvent(const TabDragInputEvent& event) {
  if (event.type == TabDragInputEvent::Type::kMoved ||
      event.type == TabDragInputEvent::Type::kDropped) {
    last_mouse_screen_point_ = event.screen_point;
    delta_ = event.screen_point - start_point_in_screen_;
  }

  switch (event.type) {
    case TabDragInputEvent::Type::kCancelled:
      injector_->GetSessionListener().OnSessionCancelled();
      EndSession();
      break;
    case TabDragInputEvent::Type::kCaptureChanged: {
      TabDragWindowAdapter* window = registry()->Get(dragged_window_);
      if (window && window->HasCapture()) {
        break;
      }
      injector_->GetSessionListener().OnSessionCancelled();
      EndSession();
      break;
    }
    case TabDragInputEvent::Type::kDropped:
      injector_->GetSessionListener().OnSessionDropped(event.screen_point);
      EndSession();
      break;
    case TabDragInputEvent::Type::kMoved:
      HandleMovedEvent(event.screen_point);
      break;
  }
}

void TabDragSession::HandleMovedEvent(const gfx::Point& screen_point) {
  switch (drag_mode_) {
    case DragMode::kAttachedToWindow:
      HandleAttachedMove(screen_point);
      break;
    case DragMode::kDetachedWindow:
      HandleDetachedMove(screen_point);
      break;
  }
}

void TabDragSession::HandleAttachedMove(const gfx::Point& screen_point) {
  DropTargetRegistry& drop_target_registry = injector_->GetDropTargetRegistry();
  DropTargetId target_id =
      drop_target_registry.FindTargetForWindow(dragged_window_);

  // Enforce that the source window MUST have a registered drop target to drag
  // tabs.
  CHECK(target_id) << "Source window must have a registered drop target";

  DropTarget* target = drop_target_registry.GetDropTarget(target_id);
  CHECK(target) << "Active drop target must exist";

  std::optional<gfx::Rect> bounds_opt = target->cached_bounds();

  // Enforce that the drop target MUST have cached bounds.
  // Due to Mojo message ordering, these are guaranteed to be present if the
  // client registered and pushed bounds before calling StartDrag.
  CHECK(bounds_opt) << "Active drop target must have cached bounds";

  constexpr int kTearThreshold = 15;

  gfx::Point local_point = target->ConvertScreenPointToLocal(screen_point);
  gfx::Rect bounds = *bounds_opt;
  bounds.Inset(-kTearThreshold);
  if (!bounds.Contains(local_point)) {
    drag_mode_ = DragMode::kDetachedWindow;
  } else {
    injector_->GetSessionListener().OnDragMoved(screen_point);
  }
}

void TabDragSession::HandleDetachedMove(const gfx::Point& screen_point) {
  DropTargetRegistry& registry = injector_->GetDropTargetRegistry();
  DropTargetId exclude_target = registry.FindTargetForWindow(dragged_window_);
  DropTargetId new_target_id =
      registry.FindTargetAtPoint(screen_point, exclude_target);

  if (new_target_id) {
    if (DropTarget* target = registry.GetDropTarget(new_target_id)) {
      drag_mode_ = DragMode::kAttachedToWindow;
      TabDragWindowId target_window_id = target->window_id();
      CHECK(target_window_id);
      dragged_window_ = target_window_id;
      injector_->GetSessionListener().OnTargetChanged(new_target_id,
                                                      screen_point);
      return;
    }
  }
}

}  // namespace tabs_api
