// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"

#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/android/content_jni_headers/SyntheticGestureTarget_jni.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/view_configuration.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace content {

SyntheticGestureTargetAndroid::SyntheticGestureTargetAndroid(
    RenderWidgetHostImpl* host,
    ui::ViewAndroid* view)
    : SyntheticGestureTargetBase(host), view_(view) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(
      Java_SyntheticGestureTarget_create(env, view->GetContainerView()));
}

SyntheticGestureTargetAndroid::~SyntheticGestureTargetAndroid() = default;

void SyntheticGestureTargetAndroid::TouchSetPointer(int index,
                                                    float x,
                                                    float y,
                                                    int id) {
  TRACE_EVENT0("input", "SyntheticGestureTargetAndroid::TouchSetPointer");
  JNIEnv* env = base::android::AttachCurrentThread();
  float scale_factor = view_->GetDipScale();
  Java_SyntheticGestureTarget_setPointer(
      env, java_ref_, index, x * scale_factor, y * scale_factor, id);
}

void SyntheticGestureTargetAndroid::TouchSetScrollDeltas(float x,
                                                         float y,
                                                         float dx,
                                                         float dy) {
  TRACE_EVENT0("input", "SyntheticGestureTargetAndroid::TouchSetScrollDeltas");
  JNIEnv* env = base::android::AttachCurrentThread();

  // Android motion events work by passing the number of wheel ticks and pixels
  // per tick so the deltas should be passed in as number of ticks.
  int wheel_ticks_multiplier =
      render_widget_host()->GetView()->GetMouseWheelMinimumGranularity();
  if (wheel_ticks_multiplier) {
    dx /= wheel_ticks_multiplier;
    dy /= wheel_ticks_multiplier;
  }

  Java_SyntheticGestureTarget_setScrollDeltas(env, java_ref_, x, y, dx, dy);
}

void SyntheticGestureTargetAndroid::TouchInject(MotionEventAction action,
                                                int pointer_count,
                                                int pointer_index,
                                                base::TimeTicks time) {
  TRACE_EVENT0("input", "SyntheticGestureTargetAndroid::TouchInject");
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SyntheticGestureTarget_inject(env, java_ref_, action, pointer_count,
                                     pointer_index,
                                     time.since_origin().InMilliseconds());
}

void SyntheticGestureTargetAndroid::DispatchWebTouchEventToPlatform(
    const WebTouchEvent& web_touch,
    const ui::LatencyInfo&) {
  MotionEventAction action = MOTION_EVENT_ACTION_INVALID;
  switch (web_touch.GetType()) {
    case WebInputEvent::Type::kTouchStart:
      action = MOTION_EVENT_ACTION_START;
      break;
    case WebInputEvent::Type::kTouchMove:
      action = MOTION_EVENT_ACTION_MOVE;
      break;
    case WebInputEvent::Type::kTouchCancel:
      action = MOTION_EVENT_ACTION_CANCEL;
      break;
    case WebInputEvent::Type::kTouchEnd:
      action = MOTION_EVENT_ACTION_END;
      break;
    default:
      NOTREACHED();
  }
  const unsigned num_touches = web_touch.touches_length;
  int touch_index = -1;
  DCHECK_LE(num_touches, 2u);
  for (unsigned i = 0; i < num_touches; ++i) {
    const blink::WebTouchPoint* point = &web_touch.touches[i];
    if (point->state != blink::WebTouchPoint::State::kStateStationary) {
      if (action == MOTION_EVENT_ACTION_START ||
          action == MOTION_EVENT_ACTION_END) {
        // We should have only one non-stationary touch for start/end.
        DCHECK_EQ(touch_index, -1);
      }
      touch_index = i;
    }
    TouchSetPointer(i, point->PositionInWidget().x(),
                    point->PositionInWidget().y(), point->id);
  }
  DCHECK_GE(touch_index, 0);

  TouchInject(action, num_touches, touch_index, web_touch.TimeStamp());
}

void SyntheticGestureTargetAndroid::DispatchWebMouseWheelEventToPlatform(
    const WebMouseWheelEvent& web_wheel,
    const ui::LatencyInfo&) {
  TouchSetScrollDeltas(web_wheel.PositionInWidget().x(),
                       web_wheel.PositionInWidget().y(), web_wheel.delta_x,
                       web_wheel.delta_y);
  // We only have a single pointer.
  const unsigned pointer_index = 0;
  const unsigned num_pointers = 1;
  TouchInject(MOTION_EVENT_ACTION_SCROLL, num_pointers, pointer_index,
              web_wheel.TimeStamp());
}

void SyntheticGestureTargetAndroid::DispatchWebGestureEventToPlatform(
    const WebGestureEvent& web_gesture,
    const ui::LatencyInfo& latency_info) {
  DCHECK_EQ(blink::WebGestureDevice::kTouchpad, web_gesture.SourceDevice());
  DCHECK(blink::WebInputEvent::IsPinchGestureEventType(web_gesture.GetType()) ||
         blink::WebInputEvent::IsFlingGestureEventType(web_gesture.GetType()));
  GetView()->SendGestureEvent(web_gesture);
}

void SyntheticGestureTargetAndroid::DispatchWebMouseEventToPlatform(
    const WebMouseEvent& web_mouse,
    const ui::LatencyInfo& info) {
  GetView()->SendMouseEvent(web_mouse, info);
}

content::mojom::GestureSourceType
SyntheticGestureTargetAndroid::GetDefaultSyntheticGestureSourceType() const {
  return content::mojom::GestureSourceType::kTouchInput;
}

float SyntheticGestureTargetAndroid::GetTouchSlopInDips() const {
  // TODO(jdduke): Have all targets use the same ui::GestureConfiguration
  // codepath.
  return gfx::ViewConfiguration::GetTouchSlopInDips();
}

float SyntheticGestureTargetAndroid::GetMinScalingSpanInDips() const {
  // TODO(jdduke): Have all targets use the same ui::GestureConfiguration
  // codepath.
  return gfx::ViewConfiguration::GetMinScalingSpanInDips();
}

RenderWidgetHostViewAndroid* SyntheticGestureTargetAndroid::GetView() const {
  auto* view = static_cast<RenderWidgetHostViewAndroid*>(
      render_widget_host()->GetView());
  DCHECK(view);
  return view;
}

}  // namespace content
