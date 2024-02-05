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

#include "base/logging.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/math/clamp.h"

namespace cobalt {
namespace ui_navigation {
namespace scroll_engine {

namespace {

const base::TimeDelta kMaxFreeScrollDuration =
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

float GetMaxAbsoluteDimension(math::Vector2dF vector) {
  return std::abs(vector.x()) >= std::abs(vector.y()) ? vector.x() : vector.y();
}

math::Vector2dF GetVelocityInMilliseconds(
    EventPositionWithTimeStamp previous_event,
    EventPositionWithTimeStamp current_event) {
  auto time_delta = current_event.time_stamp - previous_event.time_stamp;
  auto distance_delta = current_event.position - previous_event.position;

  // Don't recognize infinite velocity.
  if (time_delta.is_zero()) {
    return math::Vector2dF(0, 0);
  }

  distance_delta.Scale(static_cast<float>(1.f / time_delta.InMilliseconds()));
  return distance_delta;
}

math::Vector2dF GetNewTarget(EventPositionWithTimeStamp previous_event,
                             EventPositionWithTimeStamp current_event) {
  auto velocity = GetVelocityInMilliseconds(previous_event, current_event);
  velocity.Scale(kMaxFreeScrollDuration.InMilliseconds());
  velocity.Scale(-1);
  return current_event.position + velocity;
}

math::Vector2dF GetNewDelta(EventPositionWithTimeStamp previous_event,
                            EventPositionWithTimeStamp current_event) {
  auto new_target = GetNewTarget(previous_event, current_event);
  return new_target - current_event.position;
}

base::TimeDelta GetAnimationDurationTimeBound(
    EventPositionWithTimeStamp previous_event,
    EventPositionWithTimeStamp current_event) {
  const float time_bound_multiplier = 2.5f;
  auto time_delta = current_event.time_stamp - previous_event.time_stamp;
  return time_delta * time_bound_multiplier;
}

base::TimeDelta GetAnimationDurationEaseInOutBound(
    EventPositionWithTimeStamp previous_event,
    EventPositionWithTimeStamp current_event) {
  auto new_delta = GetNewDelta(previous_event, current_event);
  auto duration = std::sqrt(std::abs(GetMaxAbsoluteDimension(new_delta)));
  return base::TimeDelta::FromMillisecondsD(duration);
}

base::TimeDelta GetAnimationDuration(EventPositionWithTimeStamp previous_event,
                                     EventPositionWithTimeStamp current_event) {
  // TODO(b/265864360): Duration should be calculated as it is in the comment
  //                    below, but it seems to always be too small. Re-evaluate
  //                    once something workable is in.
  // auto duration_time_bound =
  //     GetAnimationDurationTimeBound(previous_event, current_event);
  // auto ease_in_out_bound =
  //     GetAnimationDurationEaseInOutBound(previous_event, current_event);
  // auto min_bound = duration_time_bound < ease_in_out_bound ?
  // duration_time_bound
  //                                                          :
  //                                                          ease_in_out_bound;
  // return min_bound < kMaxFreeScrollDuration ? min_bound
  //                                           : kMaxFreeScrollDuration;
  return kMaxFreeScrollDuration;
}

float GetAnimationSlope(EventPositionWithTimeStamp previous_event,
                        EventPositionWithTimeStamp current_event) {
  auto animation_duration = GetAnimationDuration(previous_event, current_event);
  auto time_delta = previous_event.time_stamp - current_event.time_stamp;
  if (time_delta.is_zero()) {
    return 0.f;
  }
  auto slope = std::abs(animation_duration / time_delta);
  float cubic_bezier_ease_in_out_x1 = 0.42f;
  return math::Clamp(static_cast<float>(slope) * cubic_bezier_ease_in_out_x1,
                     0.f, 1.f);
}

math::Matrix3F CalculateActiveTransform(math::Matrix3F initial_transform) {
  auto active_transform = initial_transform.Inverse();
  if (active_transform.IsZeros()) {
    return math::Matrix3F::Identity();
  }
  return active_transform;
}

}  // namespace

ScrollEngine::ScrollEngine() : active_transform_(math::Matrix3F::Identity()) {}
ScrollEngine::~ScrollEngine() { free_scroll_timer_.Stop(); }

void ScrollEngine::MaybeFreeScrollActiveNavItem() {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());

  DCHECK(previous_events_.size() == 2);
  if (previous_events_.size() != 2) {
    return;
  }

  auto current_event = previous_events_.back();
  auto previous_event = previous_events_.front();

  auto new_delta = GetNewDelta(previous_event, current_event);
  base::TimeDelta animation_duration =
      GetAnimationDuration(previous_event, current_event);
  float animation_slope = GetAnimationSlope(previous_event, current_event);

  float initial_offset_x;
  float initial_offset_y;
  active_item_->GetContentOffset(&initial_offset_x, &initial_offset_y);
  math::Vector2dF initial_offset(initial_offset_x, initial_offset_y);

  math::Vector2dF target_offset = initial_offset + new_delta;
  target_offset = BoundValuesByNavItemBounds(active_item_, target_offset);

  nav_items_with_decaying_scroll_.push_back(
      FreeScrollingNavItem(active_item_, initial_offset, target_offset,
                           animation_duration, animation_slope));
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

  auto transformed_point =
      active_transform_ * math::PointF(pointer_event->x(), pointer_event->y());
  auto current_coordinates =
      math::Vector2dF(transformed_point.x(), transformed_point.y());
  auto current_time = base::Time::FromJsTime(pointer_event->time_stamp());
  if (previous_events_.size() != 2) {
    // This is an error.
    previous_events_.push(
        EventPositionWithTimeStamp(current_coordinates, current_time));
    return;
  }

  previous_events_.pop();
  previous_events_.push(
      EventPositionWithTimeStamp(current_coordinates, current_time));

  auto drag_vector = previous_events_.front().position - current_coordinates;
  if (active_scroll_type_ == ScrollType::Horizontal) {
    drag_vector.set_y(0.0f);
  } else if (active_scroll_type_ == ScrollType::Vertical) {
    drag_vector.set_x(0.0f);
  }

  active_velocity_ = drag_vector;

  ScrollNavItemWithVector(active_item_, drag_vector);
}

void ScrollEngine::HandlePointerEvent(base_token::Token type,
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
    math::Vector2dF current_coordinates, uint64 current_time_stamp,
    const math::Matrix3F& initial_transform) {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());
  active_transform_ = CalculateActiveTransform(initial_transform);
  auto initial_point =
      active_transform_ *
      math::PointF(initial_coordinates.x(), initial_coordinates.y());
  auto current_point =
      active_transform_ *
      math::PointF(current_coordinates.x(), current_coordinates.y());
  initial_coordinates.SetVector(initial_point.x(), initial_point.y());
  current_coordinates.SetVector(current_point.x(), current_point.y());

  if (active_item_) {
    events_to_handle_.erase(pointer_id);
    return;
  }

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

  previous_events_.push(EventPositionWithTimeStamp(
      initial_coordinates, base::Time::FromJsTime(initial_time_stamp)));
  previous_events_.push(EventPositionWithTimeStamp(
      current_coordinates, base::Time::FromJsTime(current_time_stamp)));

  active_velocity_ = drag_vector;
  ScrollNavItemWithVector(active_item_, drag_vector);

  auto event_to_handle = events_to_handle_.find(pointer_id);
  if (event_to_handle != events_to_handle_.end()) {
    HandlePointerEventForActiveItem(event_to_handle->second);
    events_to_handle_.erase(pointer_id);
  }
}

void ScrollEngine::CancelActiveScrollsForNavItems(
    std::vector<scoped_refptr<ui_navigation::NavItem>> scrolls_to_cancel) {
  DCHECK(base::MessageLoop::current() == scroll_engine_.message_loop());

  for (auto scroll_to_cancel : scrolls_to_cancel) {
    for (std::vector<FreeScrollingNavItem>::iterator it =
             nav_items_with_decaying_scroll_.begin();
         it != nav_items_with_decaying_scroll_.end();) {
      if (it->nav_item().get() == scroll_to_cancel.get()) {
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
    auto current_offset = it->GetCurrentOffset();
    it->nav_item()->SetContentOffset(current_offset.x(), current_offset.y());

    if (it->AnimationIsComplete()) {
      it = nav_items_with_decaying_scroll_.erase(it);
    } else {
      it++;
    }
  }
}

}  // namespace scroll_engine
}  // namespace ui_navigation
}  // namespace cobalt
