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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_PERFORMANCE_COBALT_FRAME_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_PERFORMANCE_COBALT_FRAME_TIMING_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace viz {
struct FrameTimingDetails;
}  // namespace viz

namespace blink {

// Main-thread data captured when a main frame commits, and attached to the
// CobaltFrameTiming entry of the frame that presents it. Move-only.
struct CORE_EXPORT CobaltMainFrameSnapshot {
  // BeginFrameArgs::frame_time of the main frame (the VSync it targets).
  base::TimeTicks bmf_start;
  // When the BeginMainFrame task started running on the main thread, before
  // rAF-aligned input dispatch.
  base::TimeTicks main_frame_run_time;
  std::unique_ptr<cc::BeginMainFrameMetrics> metrics;

  CobaltMainFrameSnapshot();
  CobaltMainFrameSnapshot(CobaltMainFrameSnapshot&&);
  CobaltMainFrameSnapshot& operator=(CobaltMainFrameSnapshot&&);
  ~CobaltMainFrameSnapshot();
};

// Main-thread input path of a main frame, in milliseconds. Both values are
// nullopt for compositor-only frames. The two stages are consecutive and do
// not overlap.
struct CORE_EXPORT CobaltFrameInputTiming {
  // VSync frame time -> BeginMainFrame task started running on the main
  // thread (before rAF-aligned input dispatch). Covers cc scheduling plus the
  // time the task waited in the main thread's queue behind other tasks
  // (non-rAF-aligned input handlers such as keydown, JS, loading).
  std::optional<double> main_frame_queue_duration;
  // rAF-aligned input dispatch at the start of this main frame. 0 when there
  // was no rAF-aligned input.
  std::optional<double> handle_input_events_duration;

  static CobaltFrameInputTiming FromMainFrameSnapshot(
      const CobaltMainFrameSnapshot* snapshot);
};

// Splits the viz side of a presented frame into consecutive stages, using the
// timestamps viz already reports in viz::FrameTimingDetails. When all are
// non-null, the last four stages sum to CobaltFrameTiming::drawDuration
// (draw_start -> swap_start). Each value is in milliseconds, or nullopt when a
// timestamp is unavailable or the stage would be negative.
struct CORE_EXPORT CobaltFrameDrawBreakdown {
  // viz received the client CompositorFrame -> viz Display started drawing.
  std::optional<double> receive_to_draw_duration;
  // viz Display draw start -> viz posted the draw to the GPU thread
  // (aggregation and SkiaRenderer CPU work on the viz thread).
  std::optional<double> viz_draw_duration;
  // Draw posted -> all GPU-thread dependencies (sync tokens, e.g. raster and
  // image uploads) were satisfied. nullopt if viz did not report it; the wait
  // is then included in `gpu_queue_duration`.
  std::optional<double> gpu_dependency_wait_duration;
  // Dependencies satisfied -> GPU thread started the draw (queued behind
  // other GPU-thread tasks).
  std::optional<double> gpu_queue_duration;
  // GPU thread started the draw -> swap start (GPU-thread draw work).
  std::optional<double> gpu_draw_duration;

  static CobaltFrameDrawBreakdown FromFrameTimingDetails(
      const viz::FrameTimingDetails& details);
};

// Non-standard, Cobalt-only performance entry ("cobalt-frame") describing one
// successfully presented compositor frame: the main-thread lifecycle stages
// (when the frame contains a main-frame update) and the compositor / viz /
// EGL swap timings, all taken from the same presentation feedback so they
// are correlated without polling.
//
// Lifetime: garbage collected; created by WindowPerformance and handed to
// PerformanceObservers (entries are not buffered in the performance timeline).
// Threading: main thread only.
class CORE_EXPORT CobaltFrameTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CobaltFrameTiming(double duration,
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
                    const CobaltFrameDrawBreakdown& draw_breakdown,
                    const CobaltFrameInputTiming& input_timing,
                    DOMWindow* source);
  ~CobaltFrameTiming() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  uint32_t frameToken() const { return frame_token_; }
  DOMHighResTimeStamp presentationTime() const { return presentation_time_; }

  std::optional<double> animateDuration() const { return animate_duration_; }
  std::optional<double> styleDuration() const { return style_duration_; }
  std::optional<double> layoutDuration() const { return layout_duration_; }
  std::optional<double> prepaintDuration() const { return prepaint_duration_; }
  std::optional<double> paintDuration() const { return paint_duration_; }

  double framePrepDuration() const { return frame_prep_duration_; }
  double drawDuration() const { return draw_duration_; }
  double swapDuration() const { return swap_duration_; }

  std::optional<double> receiveToDrawDuration() const {
    return draw_breakdown_.receive_to_draw_duration;
  }
  std::optional<double> vizDrawDuration() const {
    return draw_breakdown_.viz_draw_duration;
  }
  std::optional<double> gpuDependencyWaitDuration() const {
    return draw_breakdown_.gpu_dependency_wait_duration;
  }
  std::optional<double> gpuQueueDuration() const {
    return draw_breakdown_.gpu_queue_duration;
  }
  std::optional<double> gpuDrawDuration() const {
    return draw_breakdown_.gpu_draw_duration;
  }

  std::optional<double> mainFrameQueueDuration() const {
    return input_timing_.main_frame_queue_duration;
  }
  std::optional<double> handleInputEventsDuration() const {
    return input_timing_.handle_input_events_duration;
  }

  void Trace(Visitor*) const override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  uint32_t frame_token_;
  DOMHighResTimeStamp presentation_time_;
  std::optional<double> animate_duration_;
  std::optional<double> style_duration_;
  std::optional<double> layout_duration_;
  std::optional<double> prepaint_duration_;
  std::optional<double> paint_duration_;
  double frame_prep_duration_;
  double draw_duration_;
  double swap_duration_;
  CobaltFrameDrawBreakdown draw_breakdown_;
  CobaltFrameInputTiming input_timing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_PERFORMANCE_COBALT_FRAME_TIMING_H_
