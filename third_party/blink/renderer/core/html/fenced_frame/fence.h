// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class ExceptionState;
class FenceEvent;
class LocalDOMWindow;
class ScriptState;
class V8UnionFenceEventOrString;

// Fence is a collection of fencedframe-related APIs that are visible
// at `window.fence`, only inside fencedframes.
class CORE_EXPORT Fence final : public ScriptWrappable,
                                public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Fence(LocalDOMWindow& window);

  // If `event` is a FenceEvent, calls reportEvent() to send a beacon to a
  // registered destination (referenced by destination enum and event name),
  // or a custom destination URL as appropriate.
  // If `event` is a string of the name of the event (i.e.
  // FenceEvent.eventType), calls reportPrivateAggregationEvent() to trigger
  // sending the contributions associated with the given event.
  void reportEvent(ScriptState* script_state,
                   const V8UnionFenceEventOrString* event,
                   ExceptionState& exception_state);

  // Saves the event data that will be used when an automatic beacon of type
  // event.eventType is sent. Right now, it only supports saving data for the
  // "reserved.top_navigation_start", "reserved.top_navigation_commit", and the
  // deprecated "reserved.top_navigation" beacons.
  void setReportEventDataForAutomaticBeacons(ScriptState* script_state,
                                             const FenceEvent* event,
                                             ExceptionState& exception_state);

  // Returns a list of nested inner configurations for the fenced frame, if any
  // exist. This will return configs only when called from the DOM window that
  // is the main frame of a fenced frame. This will return a non-empty array if
  // the fenced frame was created through some API that uses configurations.
  HeapVector<Member<FencedFrameConfig>> getNestedConfigs(
      ExceptionState& exception_state);

  void Trace(Visitor*) const override;

 private:
  // Dispatches to `reportEventToDestinationEnum` or
  // `reportEventToDestinationURL` depending on the format of `event`.
  void reportEvent(ScriptState* script_state,
                   const FenceEvent* event,
                   ExceptionState& exception_state);

  // Sends a report with `eventData` to the reporting destinations specified by
  // `destination`.
  void reportEventToDestinationEnum(ScriptState* script_state,
                                    const FenceEvent* event,
                                    ExceptionState& exception_state);

  // Sends a report to `destinationURL`, with substitution of buyer macros.
  void reportEventToDestinationURL(ScriptState* script_state,
                                   const FenceEvent* event,
                                   ExceptionState& exception_state);

  // Triggers the sending of any contributions associated with the given event.
  // This function simply passes off the work to the fenced frame reporter in
  // the browser to handle the actual sending of contributions.
  void reportPrivateAggregationEvent(ScriptState* script_state,
                                     const String& event,
                                     ExceptionState& exception_state);

  void AddConsoleMessage(const String& message,
                         mojom::blink::ConsoleMessageLevel level =
                             mojom::blink::ConsoleMessageLevel::kError);

  FRIEND_TEST_ALL_PREFIXES(FenceTest, ReportPrivateAggregationEvent);
  FRIEND_TEST_ALL_PREFIXES(FenceTest, ReportPrivateAggregationReservedEvent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_API_H_
