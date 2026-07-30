// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

class TabDragWindowAdapter;
class TabDragSessionInjector;
struct TabDragInputEvent;

struct TabDragSessionParams {
  raw_ptr<TabDragWindowAdapter> source_window = nullptr;
  std::vector<tabs_api::NodeId> source_tab_ids;
  gfx::Point start_point;
  base::OnceClosure end_callback;
};

// Platform-agnostic coordinator for tab dragging.
// Managed and owned by TabDragSessionManager.
class TabDragSession {
 public:
  // `injector` must outlive this session.
  TabDragSession(TabDragSessionParams params, TabDragSessionInjector* injector);
  TabDragSession(const TabDragSession&) = delete;
  TabDragSession& operator=(const TabDragSession&) = delete;
  ~TabDragSession();

  // Starts the session by initiating input capture.
  base::expected<void, mojo_base::mojom::ErrorPtr> Start();

  const gfx::Point& start_point_in_screen() const {
    return start_point_in_screen_;
  }
  const gfx::Point& last_mouse_screen_point() const {
    return last_mouse_screen_point_;
  }
  const gfx::Vector2d& delta() const { return delta_; }
  const std::vector<tabs_api::NodeId>& dragged_tabs() const {
    return dragged_tabs_;
  }
  TabDragWindowAdapter* dragged_window() const { return dragged_window_; }

 private:
  void EndSession();
  void OnInputEvent(const TabDragInputEvent& event);

  std::vector<tabs_api::NodeId> dragged_tabs_;
  const raw_ref<TabDragSessionInjector> injector_;

  base::OnceClosure end_callback_;

  const gfx::Point start_point_in_screen_;
  gfx::Point last_mouse_screen_point_;
  gfx::Vector2d delta_;
  raw_ptr<TabDragWindowAdapter> dragged_window_ = nullptr;
  raw_ptr<TabDragWindowAdapter> current_target_ = nullptr;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_
