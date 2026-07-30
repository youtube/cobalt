// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"

namespace tabs_api {

TabDragSession::TabDragSession(TabDragSessionParams params,
                               TabDragSessionInjector* injector)
    : dragged_tabs_(std::move(params.source_tab_ids)),
      injector_(CHECK_DEREF(injector)),
      end_callback_(std::move(params.end_callback)),
      start_point_in_screen_(params.start_point),
      last_mouse_screen_point_(params.start_point),
      dragged_window_(params.source_window) {
  CHECK(dragged_window_);
}

base::expected<void, mojo_base::mojom::ErrorPtr> TabDragSession::Start() {
  auto result =
      injector_->GetInputAdapter().StartInputCapture(base::BindRepeating(
          &TabDragSession::OnInputEvent, base::Unretained(this)));
  if (result.has_value()) {
    dragged_window_->SetCapture();
    injector_->GetSessionListener().OnSessionStarted(dragged_tabs_,
                                                     dragged_window_);
  }
  return result;
}

TabDragSession::~TabDragSession() {
  dragged_window_->ReleaseCapture();
  injector_->GetInputAdapter().ReleaseInputCapture();
}

void TabDragSession::EndSession() {
  if (end_callback_) {
    std::move(end_callback_).Run();
  }
}

void TabDragSession::OnInputEvent(const TabDragInputEvent& event) {
  switch (event.type) {
    case TabDragInputEvent::Type::kCancelled:
      injector_->GetSessionListener().OnSessionCancelled();
      EndSession();
      break;
    case TabDragInputEvent::Type::kCaptureChanged:
      if (dragged_window_->HasCapture()) {
        // Window has capture - ignore.
        return;
      }
      injector_->GetSessionListener().OnSessionCancelled();
      EndSession();
      break;
    case TabDragInputEvent::Type::kDropped: {
      last_mouse_screen_point_ = event.screen_point;
      delta_ = event.screen_point - start_point_in_screen_;
      auto new_target = injector_->GetDropTargetRegistry().FindTargetWindow(
          event.screen_point, dragged_window_);
      TabDragWindowAdapter* new_target_ptr =
          new_target ? &new_target->get() : nullptr;
      if (new_target_ptr != current_target_) {
        current_target_ = new_target_ptr;
        injector_->GetSessionListener().OnTargetWindowChanged(
            current_target_, event.screen_point);
      }
      injector_->GetSessionListener().OnSessionDropped(event.screen_point);
      EndSession();
      break;
    }
    case TabDragInputEvent::Type::kMoved: {
      last_mouse_screen_point_ = event.screen_point;
      delta_ = event.screen_point - start_point_in_screen_;
      auto new_target = injector_->GetDropTargetRegistry().FindTargetWindow(
          event.screen_point, dragged_window_);
      TabDragWindowAdapter* new_target_ptr =
          new_target ? &new_target->get() : nullptr;
      if (new_target_ptr != current_target_) {
        current_target_ = new_target_ptr;
        injector_->GetSessionListener().OnTargetWindowChanged(
            current_target_, event.screen_point);
      } else if (current_target_) {
        injector_->GetSessionListener().OnDragMoved(event.screen_point);
      }
      break;
    }
  }
}

}  // namespace tabs_api
