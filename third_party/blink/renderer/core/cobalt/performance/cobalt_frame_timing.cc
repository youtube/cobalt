// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/core/cobalt/performance/cobalt_frame_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

namespace {

void AddOptionalNumber(V8ObjectBuilder& builder,
                       const char* name,
                       const std::optional<double>& value) {
  if (value.has_value()) {
    builder.AddNumber(name, *value);
  } else {
    builder.AddNull(name);
  }
}

}  // namespace

CobaltMainFrameSnapshot::CobaltMainFrameSnapshot() = default;
CobaltMainFrameSnapshot::CobaltMainFrameSnapshot(CobaltMainFrameSnapshot&&) =
    default;
CobaltMainFrameSnapshot& CobaltMainFrameSnapshot::operator=(
    CobaltMainFrameSnapshot&&) = default;
CobaltMainFrameSnapshot::~CobaltMainFrameSnapshot() = default;

CobaltFrameTiming::CobaltFrameTiming(double duration,
                                     DOMHighResTimeStamp start_time,
                                     uint32_t frame_token,
                                     DOMHighResTimeStamp presentation_time,
                                     std::optional<double> animate_duration,
                                     std::optional<double> style_duration,
                                     std::optional<double> layout_duration,
                                     std::optional<double> prepaint_duration,
                                     std::optional<double> paint_duration,
                                     double frame_prep_duration,
                                     double draw_duration,
                                     double swap_duration,
                                     DOMWindow* source)
    : PerformanceEntry(duration, AtomicString("frame"), start_time, source),
      frame_token_(frame_token),
      presentation_time_(presentation_time),
      animate_duration_(animate_duration),
      style_duration_(style_duration),
      layout_duration_(layout_duration),
      prepaint_duration_(prepaint_duration),
      paint_duration_(paint_duration),
      frame_prep_duration_(frame_prep_duration),
      draw_duration_(draw_duration),
      swap_duration_(swap_duration) {}

CobaltFrameTiming::~CobaltFrameTiming() = default;

const AtomicString& CobaltFrameTiming::entryType() const {
  return performance_entry_names::kCobaltFrame;
}

PerformanceEntryType CobaltFrameTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kCobaltFrame;
}

void CobaltFrameTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("frameToken", frame_token_);
  builder.AddNumber("presentationTime", presentation_time_);
  AddOptionalNumber(builder, "animateDuration", animate_duration_);
  AddOptionalNumber(builder, "styleDuration", style_duration_);
  AddOptionalNumber(builder, "layoutDuration", layout_duration_);
  AddOptionalNumber(builder, "prepaintDuration", prepaint_duration_);
  AddOptionalNumber(builder, "paintDuration", paint_duration_);
  builder.AddNumber("framePrepDuration", frame_prep_duration_);
  builder.AddNumber("drawDuration", draw_duration_);
  builder.AddNumber("swapDuration", swap_duration_);
}

void CobaltFrameTiming::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
