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

#include "cobalt/ui_navigation/scroll_engine/scroll_engine.h"

#include <algorithm>

#include "cobalt/dom/pointer_event.h"

namespace cobalt {
namespace ui_navigation {
namespace scroll_engine {

namespace {

const base::TimeDelta kFreeScrollDuration =
    base::TimeDelta::FromMilliseconds(700);

void BoundValuesByNavItemBounds(scoped_refptr<ui_navigation::NavItem> nav_item,
                                float* x, float* y) {
  float scroll_top_lower_bound;
  float scroll_left_lower_bound;
  float scroll_top_upper_bound;
  float scroll_left_upper_bound;
  nav_item->GetBounds(&scroll_top_lower_bound, &scroll_left_lower_bound,
                      &scroll_top_upper_bound, &scroll_left_upper_bound);

  *x = std::min(scroll_left_upper_bound, *x);
  *x = std::max(scroll_left_lower_bound, *x);
  *y = std::min(scroll_top_upper_bound, *y);
  *y = std::max(scroll_top_lower_bound, *y);
}

math::Vector2dF BoundValuesByNavItemBounds(
    scoped_refptr<ui_navigation::NavItem> nav_item, math::Vector2dF vector) {
  float x = vector.x();
  float y = vector.y();
  BoundValuesByNavItemBounds(nav_item, &x, &y);
  vector.set_x(x);
  vector.set_y(y);
  return vector;
}

bool ShouldFreeScroll(scoped_refptr<ui_navigation::NavItem> scroll_container,
                      math::Vector2dF drag_vector) {
  float x = drag_vector.x();
  float y = drag_vector.y();
  bool scrolling_right = x > 0;
  bool scrolling_down = y > 0;
  bool scrolling_left = !scrolling_right;
  bool scrolling_up = !scrolling_down;

  float scroll_top_lower_bound, scroll_left_lower_bound, scroll_top_upper_bound,
      scroll_left_upper_bound;
  float offset_x, offset_y;
  scroll_container->GetBounds(&scroll_top_lower_bound, &scroll_left_lower_bound,
                              &scroll_top_upper_bound,
                              &scroll_left_upper_bound);
  scroll_container->GetContentOffset(&offset_x, &offset_y);

  bool can_scroll_left = scroll_left_lower_bound < offset_x;
  bool can_scroll_right = scroll_left_upper_bound > offset_x;
  bool can_scroll_up = scroll_top_lower_bound < offset_y;
  bool can_scroll_down = scroll_top_upper_bound > offset_y;
  return (
      ((can_scroll_left && scrolling_left) ||
       (can_scroll_right && scrolling_right)) &&
      ((can_scroll_down && scrolling_down) || (can_scroll_up && scrolling_up)));
}

void ScrollNavItemWithVector(scoped_refptr<NavItem> nav_item,
                             math::Vector2dF vector) {
  float offset_x;
  float offset_y;
  nav_item->GetContentOffset(&offset_x, &offset_y);
  offset_x += vector.x();
  offset_y += vector.y();
  BoundValuesByNavItemBounds(nav_item, &offset_x, &offset_y);

  nav_item->SetContentOffset(offset_x, offset_y);
}

}  // namespace

ScrollEngine::ScrollEngine()
    : timing_function_(cssom::TimingFunction::GetEaseInOut()) {}
ScrollEngine::~ScrollEngine() { free_scroll_timer_.Stop(); }

void ScrollEngine::MaybeFreeScrollActiveNavItem() {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());

  DCHECK(previous_events_.size() == 2);
  if (previous_events_.size() != 2) {
    return;
  }

  auto previous_event = previous_events_.back();
  auto current_event = previous_events_.front();
  math::Vector2dF distance_delta =
      previous_event.position - current_event.position;
  // TODO(andrewsavage): See if we need this
  // if (distance_delta.Length() < kFreeScrollThreshold) {
  //   return;
  // }

  // Get the average velocity for the entire run
  math::Vector2dF average_velocity = distance_delta;
  average_velocity.Scale(1.0f / static_cast<float>(current_event.time_stamp -
                                                   previous_event.time_stamp));
  average_velocity.Scale(0.5);

  // Get the distance
  average_velocity.Scale(kFreeScrollDuration.ToSbTime());

  float initial_offset_x;
  float initial_offset_y;
  active_item_->GetContentOffset(&initial_offset_x, &initial_offset_y);
  math::Vector2dF initial_offset(initial_offset_x, initial_offset_y);

  math::Vector2dF target_offset = initial_offset + average_velocity;
  target_offset = BoundValuesByNavItemBounds(active_item_, target_offset);

  nav_items_with_decaying_scroll_.push_back(
      FreeScrollingNavItem(active_item_, initial_offset, target_offset));
  if (!free_scroll_timer_.IsRunning()) {
    free_scroll_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(5),
                             this,
                             &ScrollEngine::ScrollNavItemsWithDecayingScroll);
  }

  while (!previous_events_.empty()) {
    previous_events_.pop();
  }
}

void ScrollEngine::HandlePointerEventForActiveItem(
    scoped_refptr<dom::PointerEvent> pointer_event) {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());

  if (pointer_event->type() == base::Tokens::pointerup()) {
    MaybeFreeScrollActiveNavItem();
    active_item_ = nullptr;
    active_velocity_ = math::Vector2dF(0.0f, 0.0f);
    return;
  }

  if (pointer_event->type() != base::Tokens::pointermove()) {
    return;
  }

  auto current_coordinates =
      math::Vector2dF(pointer_event->x(), pointer_event->y());
  if (previous_events_.size() != 2) {
    // This is an error.
    previous_events_.push(EventPositionWithTimeStamp(
        current_coordinates, pointer_event->time_stamp()));
    return;
  }

  auto drag_vector = previous_events_.front().position - current_coordinates;
  if (active_scroll_type_ == ScrollType::Horizontal) {
    drag_vector.set_y(0.0f);
  } else if (active_scroll_type_ == ScrollType::Vertical) {
    drag_vector.set_x(0.0f);
  }

  previous_events_.push(EventPositionWithTimeStamp(
      current_coordinates, pointer_event->time_stamp()));
  previous_events_.pop();

  active_velocity_ = drag_vector;

  ScrollNavItemWithVector(active_item_, drag_vector);
}

void ScrollEngine::HandlePointerEvent(base::Token type,
                                      const dom::PointerEventInit& event) {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());

  scoped_refptr<dom::PointerEvent> pointer_event(
      new dom::PointerEvent(type, nullptr, event));
  uint32_t pointer_id = pointer_event->pointer_id();
  if (pointer_event->type() == base::Tokens::pointerdown()) {
    events_to_handle_[pointer_id] = pointer_event;
    return;
  }
  if (active_item_) {
    HandlePointerEventForActiveItem(pointer_event);
    return;
  }

  auto last_event_to_handle = events_to_handle_.find(pointer_id);
  if (last_event_to_handle == events_to_handle_.end()) {
    // Pointer events have not come in the appropriate order.
    return;
  }

  if (pointer_event->type() == base::Tokens::pointermove()) {
    if (last_event_to_handle->second->type() == base::Tokens::pointermove() ||
        (math::Vector2dF(last_event_to_handle->second->x(),
                         last_event_to_handle->second->y()) -
         math::Vector2dF(pointer_event->x(), pointer_event->y()))
                .Length() > kDragDistanceThreshold) {
      events_to_handle_[pointer_id] = pointer_event;
    }
  } else if (pointer_event->type() == base::Tokens::pointerup()) {
    if (last_event_to_handle->second->type() == base::Tokens::pointermove()) {
      events_to_handle_[pointer_id] = pointer_event;
    } else {
      events_to_handle_.erase(pointer_id);
    }
  }
}

void ScrollEngine::HandleScrollStart(
    scoped_refptr<ui_navigation::NavItem> scroll_container,
    ScrollType scroll_type, int32_t pointer_id,
    math::Vector2dF initial_coordinates, uint64 initial_time_stamp,
    math::Vector2dF current_coordinates, uint64 current_time_stamp) {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());

  auto drag_vector = initial_coordinates - current_coordinates;
  if (ShouldFreeScroll(scroll_container, drag_vector)) {
    scroll_type = ScrollType::Free;
  }
  active_item_ = scroll_container;
  active_scroll_type_ = scroll_type;

  if (active_scroll_type_ == ScrollType::Horizontal) {
    drag_vector.set_y(0.0f);
  } else if (active_scroll_type_ == ScrollType::Vertical) {
    drag_vector.set_x(0.0f);
  }

  previous_events_.push(
      EventPositionWithTimeStamp(initial_coordinates, initial_time_stamp));
  previous_events_.push(
      EventPositionWithTimeStamp(current_coordinates, current_time_stamp));

  active_velocity_ = drag_vector;
  ScrollNavItemWithVector(active_item_, drag_vector);

  auto event_to_handle = events_to_handle_.find(pointer_id);
  if (event_to_handle != events_to_handle_.end()) {
    HandlePointerEventForActiveItem(event_to_handle->second);
  }
}

void ScrollEngine::CancelActiveScrollsForNavItems(
    std::vector<scoped_refptr<ui_navigation::NavItem>> scrolls_to_cancel) {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());

  for (auto scroll_to_cancel : scrolls_to_cancel) {
    for (std::vector<FreeScrollingNavItem>::iterator it =
             nav_items_with_decaying_scroll_.begin();
         it != nav_items_with_decaying_scroll_.end();) {
      if (it->nav_item.get() == scroll_to_cancel.get()) {
        it = nav_items_with_decaying_scroll_.erase(it);
      } else {
        it++;
      }
    }
  }
}

void ScrollEngine::ScrollNavItemsWithDecayingScroll() {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());

  if (nav_items_with_decaying_scroll_.size() == 0) {
    free_scroll_timer_.Stop();
    return;
  }
  for (std::vector<FreeScrollingNavItem>::iterator it =
           nav_items_with_decaying_scroll_.begin();
       it != nav_items_with_decaying_scroll_.end();) {
    auto now = base::Time::Now();
    auto update_delta = now - it->last_change;
    float fraction_of_time =
        std::max<float>(update_delta / kFreeScrollDuration, 1.0);
    float progress = timing_function_->Evaluate(fraction_of_time);
    math::Vector2dF current_offset = it->target_offset - it->initial_offset;
    current_offset.Scale(progress);
    current_offset += it->initial_offset;
    it->nav_item->SetContentOffset(current_offset.x(), current_offset.y());
    it->last_change = now;

    if (fraction_of_time == 1.0) {
      it = nav_items_with_decaying_scroll_.erase(it);
    } else {
      it++;
    }
  }
}

}  // namespace scroll_engine
}  // namespace ui_navigation
}  // namespace cobalt
