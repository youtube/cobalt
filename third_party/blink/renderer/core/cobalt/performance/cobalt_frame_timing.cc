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

CobaltMainFrameSnapshot::CobaltMainFrameSnapshot() = default;
CobaltMainFrameSnapshot::CobaltMainFrameSnapshot(CobaltMainFrameSnapshot&&) =
    default;
CobaltMainFrameSnapshot& CobaltMainFrameSnapshot::operator=(
    CobaltMainFrameSnapshot&&) = default;
CobaltMainFrameSnapshot::~CobaltMainFrameSnapshot() = default;

CobaltFrameTiming::CobaltFrameTiming(
    double duration,
    DOMHighResTimeStamp start_time,
    uint32_t frame_token,
    DOMHighResTimeStamp presentation_time,
    std::optional<double> animate_duration,
    std::optional<double> style_duration,
    std::optional<double> layout_duration,
    std::optional<double> prepaint_duration,
    std::optional<double> paint_duration,
    std::optional<double> composite_commit_duration,
    std::optional<double> commit_duration,
    double frame_prep_duration,
    double draw_duration,
    double swap_duration,
    std::optional<double> paint_damage_percentage,
    DOMWindow* source)
    : PerformanceEntry(duration, AtomicString("frame"), start_time, source),
      frame_token_(frame_token),
      presentation_time_(presentation_time),
      animate_duration_(animate_duration),
      style_duration_(style_duration),
      layout_duration_(layout_duration),
      prepaint_duration_(prepaint_duration),
      paint_duration_(paint_duration),
      composite_commit_duration_(composite_commit_duration),
      commit_duration_(commit_duration),
      frame_prep_duration_(frame_prep_duration),
      draw_duration_(draw_duration),
      swap_duration_(swap_duration),
      paint_damage_percentage_(paint_damage_percentage) {}

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
  if (animate_duration_.has_value()) {
    builder.AddNumber("animateDuration", *animate_duration_);
  } else {
    builder.AddNull("animateDuration");
  }
  if (style_duration_.has_value()) {
    builder.AddNumber("styleDuration", *style_duration_);
  } else {
    builder.AddNull("styleDuration");
  }
  if (layout_duration_.has_value()) {
    builder.AddNumber("layoutDuration", *layout_duration_);
  } else {
    builder.AddNull("layoutDuration");
  }
  if (prepaint_duration_.has_value()) {
    builder.AddNumber("prepaintDuration", *prepaint_duration_);
  } else {
    builder.AddNull("prepaintDuration");
  }
  if (paint_duration_.has_value()) {
    builder.AddNumber("paintDuration", *paint_duration_);
  } else {
    builder.AddNull("paintDuration");
  }
  if (composite_commit_duration_.has_value()) {
    builder.AddNumber("compositeCommitDuration", *composite_commit_duration_);
  } else {
    builder.AddNull("compositeCommitDuration");
  }
  if (commit_duration_.has_value()) {
    builder.AddNumber("commitDuration", *commit_duration_);
  } else {
    builder.AddNull("commitDuration");
  }
  builder.AddNumber("framePrepDuration", frame_prep_duration_);
  builder.AddNumber("drawDuration", draw_duration_);
  builder.AddNumber("swapDuration", swap_duration_);
  if (paint_damage_percentage_.has_value()) {
    builder.AddNumber("paintDamagePercentage", *paint_damage_percentage_);
  } else {
    builder.AddNull("paintDamagePercentage");
  }
}

void CobaltFrameTiming::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
