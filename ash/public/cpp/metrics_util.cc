// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/metrics_util.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash::metrics_util {

namespace {

bool g_data_collection_enabled = false;

std::vector<AnimationData>& GetDataCollector() {
  static base::NoDestructor<std::vector<AnimationData>> data;
  return *data;
}

void CollectDataAndForwardReport(
    base::TimeTicks start_tick,
    ReportCallback callback,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  // An arbitrary cap on the maximum number of animations being collected.
  DCHECK_LT(GetDataCollector().size(), 1000u);

  AnimationData animation_data = {
      .start_tick = start_tick,
      .stop_tick = base::TimeTicks::Now(),
      .smoothness_data = data,
  };

  GetDataCollector().emplace_back(std::move(animation_data));
  std::move(callback).Run(data);
}

// Calculates smoothness from |throughput| and sends to |callback|.
void ForwardSmoothness(base::TimeTicks start_tick,
                       SmoothnessCallback callback,
                       const cc::FrameSequenceMetrics::CustomReportData& data) {
  bool animation_in_test =
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() !=
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION;
  // Do not report if tracker is started and stopped closely in time.
  // There are 2 cases:
  // *   frame_expected == 0
  //        when there is no BeginFrame between start and stop.
  // *   frame_expected == 1 && frames_produced == 0
  //        when there is one BeginFrame but no frame generated between
  //        start and stop; should not included this case in tests.
  if (!data.frames_expected ||
      (!animation_in_test && data.frames_expected == 1 &&
       data.frames_produced == 0)) {
    return;
  }

  // Threshold to trigger logs for feedback analyzer.
  constexpr base::TimeDelta kLongAnimation = base::Seconds(1);
  constexpr int kLowSmoothness = 20;

  const base::TimeDelta duration = base::TimeTicks::Now() - start_tick;
  const int smoothness = CalculateSmoothness(data);

  if (duration > kLongAnimation) {
    VLOG(1) << "Ash system animation takes longer than usual, duration= "
            << duration.InMilliseconds() << " ms";
  } else if (smoothness < kLowSmoothness) {
    VLOG(1) << "Ash system animation drops too many frames, smoothness= "
            << smoothness;
  }

  callback.Run(smoothness);
}

}  // namespace

ReportCallback ForSmoothness(SmoothnessCallback callback,
                             bool exclude_from_data_collection) {
  const base::TimeTicks now = base::TimeTicks::Now();
  auto forward_smoothness =
      base::BindRepeating(&ForwardSmoothness, now, std::move(callback));
  if (!g_data_collection_enabled || exclude_from_data_collection)
    return forward_smoothness;

  return base::BindRepeating(&CollectDataAndForwardReport, now,
                             std::move(forward_smoothness));
}

void StartDataCollection() {
  DCHECK(!g_data_collection_enabled);
  g_data_collection_enabled = true;
}

std::vector<AnimationData> StopDataCollection() {
  DCHECK(g_data_collection_enabled);
  g_data_collection_enabled = false;

  return GetCollectedData();
}

std::vector<AnimationData> GetCollectedData() {
  std::vector<AnimationData> data;
  data.swap(GetDataCollector());
  return data;
}

int CalculateSmoothness(
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  DCHECK(data.frames_expected);
  return std::floor(100.0f * data.frames_produced / data.frames_expected);
}

int CalculateJank(const cc::FrameSequenceMetrics::CustomReportData& data) {
  DCHECK(data.frames_expected);
  return std::floor(100.0f * data.jank_count / data.frames_expected);
}

}  // namespace ash::metrics_util
