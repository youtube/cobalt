// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_BEGIN_MAIN_FRAME_METRICS_H_
#define CC_METRICS_BEGIN_MAIN_FRAME_METRICS_H_

#include <bitset>

#include "base/time/time.h"
#include "cc/cc_export.h"

namespace cc {

// Reason that a BeginMainFrame was triggered. Used for metrics only,
// specifically: |Compositing.BeginMainFrame.BMFReason*|.
enum class BeginMainFrameReason {
  // Catch-all bucket for anything unclassified.
  kOther = 0,
  // ServiceScriptedAnimations almost always occurs as a result of RAF, so
  // these two can be grouped together.
  kRAFOrServiceScriptedAnimations = 1,
  kRAF = kRAFOrServiceScriptedAnimations,
  kServiceScriptedAnimations = kRAFOrServiceScriptedAnimations,
  kVideoFrameCallback = 2,
  kAnimation = 3,
  kCSSAnimation = kAnimation,
  // These three are relatively infrequent, so group them all together for now.
  kStylePaintOrLayoutInvalidation = 4,
  kStyleInvalidation = kStylePaintOrLayoutInvalidation,
  kPaintInvalidation = kStylePaintOrLayoutInvalidation,
  kLayoutInvalidation = kStylePaintOrLayoutInvalidation,
  kScroll = 5,
  kInput = 6,
  kMainThreadScroll = 7,
  kDelayedTimerFired = 8,
  kMaxValue = kDelayedTimerFired,
};

inline constexpr size_t BeginMainFrameReasonSize =
    static_cast<size_t>(BeginMainFrameReason::kMaxValue) + 1;

// We use this metric in a bitfield. UMA can only record 1000 buckets for a
// histogram. So, assert that we do not go over this max size.
static_assert(1 << BeginMainFrameReasonSize < 1000);

using BeginMainFrameReasons = std::bitset<BeginMainFrameReasonSize>;

// Latency timing data for Main Frame lifecycle updates triggered by cc.
// The data is captured in LocalFrameViewUKMAggregator and passed back through
// the proxy when a main frame ends. LayerTreeHost updates the update_layers_
// value in LayerTreeHost::UpdateLayers.
struct CC_EXPORT BeginMainFrameMetrics {
  base::TimeDelta handle_input_events;
  base::TimeDelta animate;
  base::TimeDelta style_update;
  base::TimeDelta layout_update;
  base::TimeDelta accessibility;
  base::TimeDelta prepaint;
  base::TimeDelta compositing_inputs;
  base::TimeDelta paint;
  base::TimeDelta composite_commit;
  base::TimeDelta update_layers;
  // True if we should measure smoothness in TotalFrameCounter.
  // Currently true when first contentful paint is done.
  bool should_measure_smoothness = false;

  BeginMainFrameMetrics();

  BeginMainFrameMetrics(const BeginMainFrameMetrics& other);
  BeginMainFrameMetrics& operator=(const BeginMainFrameMetrics& other);
};

}  // namespace cc

#endif  // CC_METRICS_BEGIN_MAIN_FRAME_METRICS_H_
