// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_PINCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_PINCH_EVENT_QUEUE_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/common/input/event_with_latency_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace content {

class QueuedTouchpadPinchEvent;

// Interface with which TouchpadPinchEventQueue can forward synthetic mouse
// wheel events and notify of pinch events.
class CONTENT_EXPORT TouchpadPinchEventQueueClient {
 public:
  virtual ~TouchpadPinchEventQueueClient() = default;

  using MouseWheelEventHandledCallback =
      base::OnceCallback<void(const MouseWheelEventWithLatencyInfo& event,
                              blink::mojom::InputEventResultSource ack_source,
                              blink::mojom::InputEventResultState ack_result)>;

  virtual void SendMouseWheelEventForPinchImmediately(
      const MouseWheelEventWithLatencyInfo& event,
      MouseWheelEventHandledCallback callback) = 0;
  virtual void OnGestureEventForPinchAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
};

// A queue for sending synthetic mouse wheel events for touchpad pinches.
// In order for a page to prevent touchpad pinch zooming, we send synthetic
// wheel events which may be cancelled. Once we've determined whether a page
// has prevented the pinch, the TouchpadPinchEventQueueClient may proceed with
// handling the pinch.
// See README.md for further details.
class CONTENT_EXPORT TouchpadPinchEventQueue {
 public:
  // The |client| must outlive the TouchpadPinchEventQueue.
  TouchpadPinchEventQueue(TouchpadPinchEventQueueClient* client);

  TouchpadPinchEventQueue(const TouchpadPinchEventQueue&) = delete;
  TouchpadPinchEventQueue& operator=(const TouchpadPinchEventQueue&) = delete;

  ~TouchpadPinchEventQueue();

  // Adds the given touchpad pinch |event| to the queue. The event may be
  // coalesced with previously queued events.
  void QueueEvent(const GestureEventWithLatencyInfo& event);

  [[nodiscard]] bool has_pending() const;

 private:
  // Notifies the queue that a synthetic mouse wheel event has been processed
  // by the renderer.
  void ProcessMouseWheelAck(const MouseWheelEventWithLatencyInfo& ack_event,
                            blink::mojom::InputEventResultSource ack_source,
                            blink::mojom::InputEventResultState ack_result);

  void TryForwardNextEventToRenderer();

  const bool touchpad_async_pinch_events_;
  raw_ptr<TouchpadPinchEventQueueClient> client_;

  base::circular_deque<std::unique_ptr<QueuedTouchpadPinchEvent>> pinch_queue_;
  std::unique_ptr<QueuedTouchpadPinchEvent> pinch_event_awaiting_ack_;
  absl::optional<bool> first_event_prevented_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_PINCH_EVENT_QUEUE_H_
