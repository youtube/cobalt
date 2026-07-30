// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_LISTENER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_LISTENER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class TabDragWindowAdapter;

class ToyTabDragSessionListener : public TabDragSessionListener {
 public:
  struct Event {
    enum class Type {
      kStarted,
      kTargetChanged,
      kMoved,
      kDropped,
      kCancelled,
    };
    Type type;
    raw_ptr<TabDragWindowAdapter> window = nullptr;
    gfx::Point point;
    std::vector<tabs_api::NodeId> dragged_tabs;
  };

  ToyTabDragSessionListener();
  ~ToyTabDragSessionListener() override;

  // TabDragSessionListener:
  void OnSessionStarted(std::vector<tabs_api::NodeId> dragged_tabs,
                        TabDragWindowAdapter* source_window) override;
  void OnTargetWindowChanged(TabDragWindowAdapter* new_target,
                             const gfx::Point& screen_point) override;
  void OnDragMoved(const gfx::Point& screen_point) override;
  void OnSessionDropped(const gfx::Point& screen_point) override;
  void OnSessionCancelled() override;

  const std::vector<Event>& events() const { return events_; }
  void Clear() { events_.clear(); }

 private:
  std::vector<Event> events_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_LISTENER_H_
