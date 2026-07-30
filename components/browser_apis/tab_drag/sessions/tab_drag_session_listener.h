// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_LISTENER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_LISTENER_H_

#include <vector>

#include "components/browser_apis/tab_strip/types/node_id.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class TabDragWindowAdapter;

class TabDragSessionListener {
 public:
  virtual ~TabDragSessionListener() = default;

  // Called when a new drag session starts.
  virtual void OnSessionStarted(std::vector<tabs_api::NodeId> dragged_tabs,
                                TabDragWindowAdapter* source_window) = 0;

  // Called when the active target window for the drag changes.
  // `new_target` is the window now under the cursor, or nullptr if none.
  virtual void OnTargetWindowChanged(TabDragWindowAdapter* new_target,
                                     const gfx::Point& screen_point) = 0;

  // Called when the drag moves within the current target window.
  virtual void OnDragMoved(const gfx::Point& screen_point) = 0;

  // Called when the session ends with a drop.
  virtual void OnSessionDropped(const gfx::Point& screen_point) = 0;

  // Called when the session is cancelled.
  virtual void OnSessionCancelled() = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_LISTENER_H_
