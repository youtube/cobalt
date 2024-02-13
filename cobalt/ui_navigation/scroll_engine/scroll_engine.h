// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_UI_NAVIGATION_SCROLL_ENGINE_SCROLL_ENGINE_H_
#define COBALT_UI_NAVIGATION_SCROLL_ENGINE_SCROLL_ENGINE_H_

#include <map>
#include <queue>
#include <vector>

#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cobalt/base/token.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/ui_navigation/nav_item.h"
#include "cobalt/ui_navigation/scroll_engine/free_scrolling_nav_item.h"

namespace cobalt {
namespace ui_navigation {
namespace scroll_engine {

// kDragDistanceThreshold is measured in viewport coordinate distance.
const float kDragDistanceThreshold = 20.0f;

typedef enum ScrollType {
  Unknown,
  Horizontal,
  Vertical,
  Free,
} ScrollType;

struct EventPositionWithTimeStamp {
  EventPositionWithTimeStamp(math::Vector2dF position, base::Time time_stamp)
      : position(position), time_stamp(time_stamp) {}
  math::Vector2dF position;
  base::Time time_stamp;
};

class ScrollEngine {
 public:
  ScrollEngine();
  ~ScrollEngine();

  void HandlePointerEvent(base_token::Token type,
                          const dom::PointerEventInit& event);
  void HandleScrollStart(scoped_refptr<ui_navigation::NavItem> scroll_container,
                         ScrollType scroll_type, int32_t pointer_id,
                         math::Vector2dF initial_coordinates,
                         uint64 initial_time_stamp,
                         math::Vector2dF current_coordinates,
                         uint64 current_time_stamp,
                         const math::Matrix3F& initial_transform);
  void CancelActiveScrollsForNavItems(
      std::vector<scoped_refptr<ui_navigation::NavItem>> scrolls_to_cancel);

  void HandlePointerEventForActiveItem(
      scoped_refptr<dom::PointerEvent> pointer_event);
  void ScrollNavItemsWithDecayingScroll();
  void MaybeFreeScrollActiveNavItem();

  base::Thread* thread() { return &scroll_engine_; }

 private:
  base::Thread scroll_engine_{"ScrollEngineThread"};
  base::RepeatingTimer free_scroll_timer_;

  std::queue<EventPositionWithTimeStamp> previous_events_;

  scoped_refptr<NavItem> active_item_;
  math::Vector2dF active_velocity_;
  math::Matrix3F active_transform_;
  ScrollType active_scroll_type_ = ScrollType::Unknown;
  std::map<uint32_t, scoped_refptr<dom::PointerEvent>> events_to_handle_;
  std::vector<FreeScrollingNavItem> nav_items_with_decaying_scroll_;
};


}  // namespace scroll_engine
}  // namespace ui_navigation
}  // namespace cobalt

#endif  // COBALT_UI_NAVIGATION_SCROLL_ENGINE_SCROLL_ENGINE_H_
