// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_touch_uma_histogram.h"

#include <ostream>

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace ui {

void GestureTouchUMAHistogram::RecordGestureEvent(
    const GestureEventData& gesture) {
  UMA_HISTOGRAM_ENUMERATION(
      "Event.GestureCreated", UMAEventTypeFromEvent(gesture), UMA_ET_COUNT);
}

#define RECORD_MAX_DRAG_DISTANCE(HISTOGRAM_NAME, DIST_SQUARED) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                                 \
      HISTOGRAM_NAME, static_cast<int>(sqrt(DIST_SQUARED)), 1, 1500, 50)

void GestureTouchUMAHistogram::RecordTouchEvent(const MotionEvent& event) {
  switch (event.GetAction()) {
    case MotionEvent::Action::DOWN:
      tool_type_ = event.GetToolType();
      start_touch_position_ = gfx::Point(event.GetX(), event.GetY());
      is_single_finger_ = true;
      max_distance_from_start_squared_ = 0;
      break;

    case MotionEvent::Action::MOVE:
      if (is_single_finger_) {
        float cur_dist =
            (start_touch_position_ - gfx::Point(event.GetX(), event.GetY()))
                .LengthSquared();
        if (cur_dist > max_distance_from_start_squared_)
          max_distance_from_start_squared_ = cur_dist;
      }
      break;

    case MotionEvent::Action::UP:
      if (is_single_finger_) {
        if (tool_type_ == MotionEvent::ToolType::FINGER) {
          RECORD_MAX_DRAG_DISTANCE("Event.MaxDragDistance.FINGER",
                                   max_distance_from_start_squared_);
        } else if (tool_type_ == MotionEvent::ToolType::STYLUS) {
          RECORD_MAX_DRAG_DISTANCE("Event.MaxDragDistance.STYLUS",
                                   max_distance_from_start_squared_);
        } else if (tool_type_ == MotionEvent::ToolType::ERASER) {
          RECORD_MAX_DRAG_DISTANCE("Event.MaxDragDistance.ERASER",
                                   max_distance_from_start_squared_);
        }
        is_single_finger_ = false;
      }
      break;

    default:
      // We expect either a POINTER_DOWN (when a secondary pointer became
      // active) or a CANCEL (when an active pointer becomes invalid).
      is_single_finger_ = false;
  }
}

UMAEventType GestureTouchUMAHistogram::UMAEventTypeFromEvent(
    const GestureEventData& gesture) {
  switch (gesture.type()) {
    case ET_TOUCH_RELEASED:
      return UMA_ET_TOUCH_RELEASED;
    case ET_TOUCH_PRESSED:
      return UMA_ET_TOUCH_PRESSED;
    case ET_TOUCH_MOVED:
      return UMA_ET_TOUCH_MOVED;
    case ET_TOUCH_CANCELLED:
      return UMA_ET_TOUCH_CANCELLED;
    case ET_GESTURE_SCROLL_BEGIN:
      return UMA_ET_GESTURE_SCROLL_BEGIN;
    case ET_GESTURE_SCROLL_END:
      return UMA_ET_GESTURE_SCROLL_END;
    case ET_GESTURE_SCROLL_UPDATE: {
      int touch_points = gesture.details.touch_points();
      if (touch_points == 1)
        return UMA_ET_GESTURE_SCROLL_UPDATE;
      else if (touch_points == 2)
        return UMA_ET_GESTURE_SCROLL_UPDATE_2;
      else if (touch_points == 3)
        return UMA_ET_GESTURE_SCROLL_UPDATE_3;
      return UMA_ET_GESTURE_SCROLL_UPDATE_4P;
    }
    case ET_GESTURE_TAP: {
      int tap_count = gesture.details.tap_count();
      if (tap_count == 1)
        return UMA_ET_GESTURE_TAP;
      if (tap_count == 2)
        return UMA_ET_GESTURE_DOUBLE_TAP;
      if (tap_count == 3)
        return UMA_ET_GESTURE_TRIPLE_TAP;
      NOTREACHED() << "Received tap with tapcount " << tap_count;
      return UMA_ET_UNKNOWN;
    }
    case ET_GESTURE_TAP_DOWN:
      return UMA_ET_GESTURE_TAP_DOWN;
    case ET_GESTURE_BEGIN:
      return UMA_ET_GESTURE_BEGIN;
    case ET_GESTURE_END:
      return UMA_ET_GESTURE_END;
    case ET_GESTURE_TWO_FINGER_TAP:
      return UMA_ET_GESTURE_TWO_FINGER_TAP;
    case ET_GESTURE_PINCH_BEGIN:
      return UMA_ET_GESTURE_PINCH_BEGIN;
    case ET_GESTURE_PINCH_END:
      return UMA_ET_GESTURE_PINCH_END;
    case ET_GESTURE_PINCH_UPDATE: {
      int touch_points = gesture.details.touch_points();
      if (touch_points >= 4)
        return UMA_ET_GESTURE_PINCH_UPDATE_4P;
      else if (touch_points == 3)
        return UMA_ET_GESTURE_PINCH_UPDATE_3;
      return UMA_ET_GESTURE_PINCH_UPDATE;
    }
    case ET_GESTURE_SHORT_PRESS:
      return UMA_ET_GESTURE_SHORT_PRESS;
    case ET_GESTURE_LONG_PRESS:
      return UMA_ET_GESTURE_LONG_PRESS;
    case ET_GESTURE_LONG_TAP:
      return UMA_ET_GESTURE_LONG_TAP;
    case ET_GESTURE_SWIPE: {
      int touch_points = gesture.details.touch_points();
      if (touch_points == 1)
        return UMA_ET_GESTURE_SWIPE_1;
      else if (touch_points == 2)
        return UMA_ET_GESTURE_SWIPE_2;
      else if (touch_points == 3)
        return UMA_ET_GESTURE_SWIPE_3;
      return UMA_ET_GESTURE_SWIPE_4P;
    }
    case ET_GESTURE_TAP_CANCEL:
      return UMA_ET_GESTURE_TAP_CANCEL;
    case ET_GESTURE_SHOW_PRESS:
      return UMA_ET_GESTURE_SHOW_PRESS;
    case ET_SCROLL:
      return UMA_ET_SCROLL;
    case ET_SCROLL_FLING_START:
      return UMA_ET_SCROLL_FLING_START;
    case ET_SCROLL_FLING_CANCEL:
      return UMA_ET_SCROLL_FLING_CANCEL;
    case ET_GESTURE_TAP_UNCONFIRMED:
      return UMA_ET_GESTURE_TAP_UNCONFIRMED;
    case ET_GESTURE_DOUBLE_TAP:
      return UMA_ET_GESTURE_DOUBLE_TAP;
    default:
      NOTREACHED();
      return UMA_ET_UNKNOWN;
  }
}

}  //  namespace ui
