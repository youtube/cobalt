// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/input/input_router.h"

#include "base/task/sequenced_task_runner.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/input/event_with_latency_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"

namespace content {
class InputRouterClient;

class MockInputRouter : public InputRouter {
 public:
  explicit MockInputRouter(InputRouterClient* client)
      : sent_mouse_event_(false),
        sent_wheel_event_(false),
        sent_keyboard_event_(false),
        sent_gesture_event_(false),
        send_touch_event_not_cancelled_(false),
        has_handlers_(false),
        client_(client) {}

  MockInputRouter(const MockInputRouter&) = delete;
  MockInputRouter& operator=(const MockInputRouter&) = delete;

  ~MockInputRouter() override {}

  // InputRouter:
  void SendMouseEvent(const MouseEventWithLatencyInfo& mouse_event,
                      MouseEventCallback event_result_callback) override;
  void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override;
  void SendKeyboardEvent(const NativeWebKeyboardEventWithLatencyInfo& key_event,
                         KeyboardEventCallback event_result_callback) override;
  void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) override;
  void SendTouchEvent(const TouchEventWithLatencyInfo& touch_event) override;
  void NotifySiteIsMobileOptimized(bool is_mobile_optimized) override {}
  bool HasPendingEvents() const override;
  void SetDeviceScaleFactor(float device_scale_factor) override {}
  absl::optional<cc::TouchAction> AllowedTouchAction() override;
  absl::optional<cc::TouchAction> ActiveTouchAction() override;
  void SetForceEnableZoom(bool enabled) override {}
  mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost> BindNewHost(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;
  void StopFling() override {}
  void ForceSetTouchActionAuto() override {}
  void OnHasTouchEventConsumers(
      blink::mojom::TouchEventConsumersPtr consumers) override;
  void WaitForInputProcessed(base::OnceClosure callback) override {}
  void FlushTouchEventQueue() override {}

  bool sent_mouse_event_;
  bool sent_wheel_event_;
  bool sent_keyboard_event_;
  bool sent_gesture_event_;
  bool send_touch_event_not_cancelled_;
  bool has_handlers_;

 private:
  raw_ptr<InputRouterClient> client_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_H_
