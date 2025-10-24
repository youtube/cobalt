// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_timing_history.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
namespace cc {
namespace {

// Using the 90th percentile will disable latency recovery
// if we are missing the deadline approximately ~6 times per
// second.
// TODO(brianderson): Fine tune the percentiles below.
const size_t kDurationHistorySize = 60;
const double kBeginMainFrameQueueDurationEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationCriticalEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationNotCriticalEstimationPercentile = 90.0;
const double kBeginMainFrameStartToReadyToCommitEstimationPercentile = 90.0;
const double kCommitEstimatePercentile = 90.0;
const double kCommitToReadyToActivateEstimationPercentile = 90.0;
const double kActivateEstimationPercentile = 90.0;
const double kDrawEstimationPercentile = 90.0;

// ~90 VSync aligned UMA buckets.
const int kUMAVSyncBuckets[] = {
    // Powers of two from 0 to 2048 us @ 50% precision
    1,
    2,
    4,
    8,
    16,
    32,
    64,
    128,
    256,
    512,
    1024,
    2048,
    // Every 8 Hz from 256 Hz to 128 Hz @ 3-6% precision
    3906,
    4032,
    4167,
    4310,
    4464,
    4630,
    4808,
    5000,
    5208,
    5435,
    5682,
    5952,
    6250,
    6579,
    6944,
    7353,
    // Every 4 Hz from 128 Hz to 64 Hz @ 3-6% precision
    7813,
    8065,
    8333,
    8621,
    8929,
    9259,
    9615,
    10000,
    10417,
    10870,
    11364,
    11905,
    12500,
    13158,
    13889,
    14706,
    // Every 2 Hz from 64 Hz to 32 Hz @ 3-6% precision
    15625,
    16129,
    16667,
    17241,
    17857,
    18519,
    19231,
    20000,
    20833,
    21739,
    22727,
    23810,
    25000,
    26316,
    27778,
    29412,
    // Every 1 Hz from 32 Hz to 1 Hz @ 3-33% precision
    31250,
    32258,
    33333,
    34483,
    35714,
    37037,
    38462,
    40000,
    41667,
    43478,
    45455,
    47619,
    50000,
    52632,
    55556,
    58824,
    62500,
    66667,
    71429,
    76923,
    83333,
    90909,
    100000,
    111111,
    125000,
    142857,
    166667,
    200000,
    250000,
    333333,
    500000,
    // Powers of two from 1s to 32s @ 50% precision
    1000000,
    2000000,
    4000000,
    8000000,
    16000000,
    32000000,
};

#define UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(name, sample) \
  do {                                                         \
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(                          \
        name "2", sample.InMicroseconds(),                     \
        std::vector<int>(std::begin(kUMAVSyncBuckets),         \
                         std::end(kUMAVSyncBuckets)));         \
  } while (false)

}  // namespace

CompositorTimingHistory::CompositorTimingHistory(UMACategory uma_category)
    : enabled_(false),
      compositor_drawing_continuously_(false),
      begin_main_frame_queue_duration_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_critical_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_not_critical_history_(
          kDurationHistorySize),
      begin_main_frame_start_to_ready_to_commit_duration_history_(
          kDurationHistorySize),
      commit_duration_history_(kDurationHistorySize),
      commit_to_ready_to_activate_duration_history_(kDurationHistorySize),
      activate_duration_history_(kDurationHistorySize),
      draw_duration_history_(kDurationHistorySize),
      pending_tree_is_impl_side_(false),
      uma_category_(uma_category) {}

CompositorTimingHistory::~CompositorTimingHistory() = default;

base::TimeTicks CompositorTimingHistory::Now() const {
  return base::TimeTicks::Now();
}

void CompositorTimingHistory::SetRecordingEnabled(bool enabled) {
  enabled_ = enabled;
}

void CompositorTimingHistory::SetCompositorDrawingContinuously(bool active) {
  if (active == compositor_drawing_continuously_)
    return;
  draw_end_time_prev_ = base::TimeTicks();
  compositor_drawing_continuously_ = active;
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueDurationCriticalEstimate() const {
  base::TimeDelta all = begin_main_frame_queue_duration_history_.Percentile(
      kBeginMainFrameQueueDurationEstimationPercentile);
  base::TimeDelta critical =
      begin_main_frame_queue_duration_critical_history_.Percentile(
          kBeginMainFrameQueueDurationCriticalEstimationPercentile);
  // Return the min since critical BeginMainFrames are likely fast if
  // the non critical ones are.
  return std::min(critical, all);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueDurationNotCriticalEstimate()
    const {
  base::TimeDelta all = begin_main_frame_queue_duration_history_.Percentile(
      kBeginMainFrameQueueDurationEstimationPercentile);
  base::TimeDelta not_critical =
      begin_main_frame_queue_duration_not_critical_history_.Percentile(
          kBeginMainFrameQueueDurationNotCriticalEstimationPercentile);
  // Return the max since, non critical BeginMainFrames are likely slow if
  // the critical ones are.
  return std::max(not_critical, all);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameStartToReadyToCommitDurationEstimate()
    const {
  return begin_main_frame_start_to_ready_to_commit_duration_history_.Percentile(
      kBeginMainFrameStartToReadyToCommitEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::CommitDurationEstimate() const {
  return commit_duration_history_.Percentile(kCommitEstimatePercentile);
}

base::TimeDelta
CompositorTimingHistory::CommitToReadyToActivateDurationEstimate() const {
  return commit_to_ready_to_activate_duration_history_.Percentile(
      kCommitToReadyToActivateEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::ActivateDurationEstimate() const {
  return activate_duration_history_.Percentile(kActivateEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::DrawDurationEstimate() const {
  return draw_duration_history_.Percentile(kDrawEstimationPercentile);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameStartToReadyToCommitCriticalEstimate()
    const {
  return BeginMainFrameStartToReadyToCommitDurationEstimate() +
         BeginMainFrameQueueDurationCriticalEstimate();
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameStartToReadyToCommitNotCriticalEstimate()
    const {
  return BeginMainFrameStartToReadyToCommitDurationEstimate() +
         BeginMainFrameQueueDurationNotCriticalEstimate();
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueToActivateCriticalEstimate() const {
  return BeginMainFrameStartToReadyToCommitDurationEstimate() +
         CommitDurationEstimate() + CommitToReadyToActivateDurationEstimate() +
         ActivateDurationEstimate() +
         BeginMainFrameQueueDurationCriticalEstimate();
}

void CompositorTimingHistory::WillFinishImplFrame(bool needs_redraw) {
  if (!needs_redraw)
    SetCompositorDrawingContinuously(false);
}

void CompositorTimingHistory::BeginImplFrameNotExpectedSoon() {
  SetCompositorDrawingContinuously(false);
}

void CompositorTimingHistory::WillBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_sent_time_);

  begin_main_frame_on_critical_path_ = args.on_critical_path;
  begin_main_frame_sent_time_ = Now();
}

void CompositorTimingHistory::BeginMainFrameStarted(
    base::TimeTicks main_thread_start_time) {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_start_time_);
  begin_main_frame_start_time_ = main_thread_start_time;
}

void CompositorTimingHistory::BeginMainFrameAborted() {
  base::TimeTicks begin_main_frame_end_time = Now();
  DidBeginMainFrame(begin_main_frame_end_time);
}

void CompositorTimingHistory::NotifyReadyToCommit() {
  DCHECK_NE(begin_main_frame_start_time_, base::TimeTicks());
  DCHECK_EQ(ready_to_commit_time_, base::TimeTicks());
  ready_to_commit_time_ = Now();
  pending_commit_on_critical_path_ = begin_main_frame_on_critical_path_;
  base::TimeDelta bmf_duration =
      ready_to_commit_time_ - begin_main_frame_start_time_;
  DidBeginMainFrame(ready_to_commit_time_);
  begin_main_frame_start_to_ready_to_commit_duration_history_.InsertSample(
      bmf_duration);
}

void CompositorTimingHistory::WillCommit() {
  DCHECK_NE(ready_to_commit_time_, base::TimeTicks());
  commit_start_time_ = Now();
  ready_to_commit_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidCommit() {
  DCHECK_EQ(pending_tree_creation_time_, base::TimeTicks());
  DCHECK_NE(commit_start_time_, base::TimeTicks());

  base::TimeTicks commit_end_time = Now();
  commit_duration_history_.InsertSample(commit_end_time - commit_start_time_);

  pending_tree_is_impl_side_ = false;
  pending_tree_creation_time_ = commit_end_time;
}

void CompositorTimingHistory::DidBeginMainFrame(
    base::TimeTicks begin_main_frame_end_time) {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);

  // If the BeginMainFrame start time isn't known, assume it was immediate
  // for scheduling purposes, but don't report it for UMA to avoid skewing
  // the results.
  // TODO(szager): Can this be true? begin_main_frame_start_time_ should be
  // unconditionally assigned in BeginMainFrameStarted().
  if (begin_main_frame_start_time_.is_null())
    begin_main_frame_start_time_ = begin_main_frame_sent_time_;

  if (enabled_) {
    base::TimeDelta bmf_queue_duration =
        begin_main_frame_start_time_ - begin_main_frame_sent_time_;
    begin_main_frame_queue_duration_history_.InsertSample(bmf_queue_duration);
    if (begin_main_frame_on_critical_path_) {
      begin_main_frame_queue_duration_critical_history_.InsertSample(
          bmf_queue_duration);
    } else {
      begin_main_frame_queue_duration_not_critical_history_.InsertSample(
          bmf_queue_duration);
    }
  }

  begin_main_frame_sent_time_ = base::TimeTicks();
  begin_main_frame_start_time_ = base::TimeTicks();
  begin_main_frame_on_critical_path_ = false;
}

void CompositorTimingHistory::WillInvalidateOnImplSide() {
  DCHECK(!pending_tree_is_impl_side_);
  DCHECK_EQ(pending_tree_creation_time_, base::TimeTicks());

  pending_tree_is_impl_side_ = true;
  pending_tree_on_critical_path_ = false;
  pending_tree_bmf_queue_duration_ = base::TimeDelta();
  pending_tree_creation_time_ = base::TimeTicks::Now();
}

void CompositorTimingHistory::ReadyToActivate() {
  DCHECK_NE(pending_tree_creation_time_, base::TimeTicks());
  DCHECK_EQ(pending_tree_ready_to_activate_time_, base::TimeTicks());

  pending_tree_ready_to_activate_time_ = Now();
  if (!pending_tree_is_impl_side_ && enabled_) {
    base::TimeDelta time_since_commit =
        pending_tree_ready_to_activate_time_ - pending_tree_creation_time_;
    commit_to_ready_to_activate_duration_history_.InsertSample(
        time_since_commit);
  }
}

void CompositorTimingHistory::WillActivate() {
  DCHECK_EQ(base::TimeTicks(), activate_start_time_);

  activate_start_time_ = Now();

  pending_tree_is_impl_side_ = false;
  pending_tree_creation_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidActivate() {
  DCHECK_NE(base::TimeTicks(), activate_start_time_);
  base::TimeTicks activate_end_time = Now();
  base::TimeDelta activate_duration = activate_end_time - activate_start_time_;

  if (enabled_) {
    activate_duration_history_.InsertSample(activate_duration);
  }

  pending_tree_ready_to_activate_time_ = base::TimeTicks();
  activate_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillDraw() {
  DCHECK_EQ(base::TimeTicks(), draw_start_time_);
  draw_start_time_ = Now();
}

void CompositorTimingHistory::DidDraw() {
  DCHECK_NE(base::TimeTicks(), draw_start_time_);
  base::TimeTicks draw_end_time = Now();
  if (enabled_) {
    base::TimeDelta draw_duration = draw_end_time - draw_start_time_;
    draw_duration_history_.InsertSample(draw_duration);
  }

  SetCompositorDrawingContinuously(true);
  if (!draw_end_time_prev_.is_null() && uma_category_ == RENDERER_UMA) {
    base::TimeDelta draw_interval = draw_end_time - draw_end_time_prev_;
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED("Scheduling.Renderer.DrawInterval",
                                             draw_interval);
  }
  draw_end_time_prev_ = draw_end_time;

  draw_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::ClearHistory() {
  TRACE_EVENT0("cc,benchmark", "CompositorTimingHistory::ClearHistory");

  begin_main_frame_queue_duration_history_.Clear();
  begin_main_frame_queue_duration_critical_history_.Clear();
  begin_main_frame_queue_duration_not_critical_history_.Clear();
  begin_main_frame_start_to_ready_to_commit_duration_history_.Clear();
  commit_duration_history_.Clear();
  commit_to_ready_to_activate_duration_history_.Clear();
  activate_duration_history_.Clear();
  draw_duration_history_.Clear();
}

size_t CompositorTimingHistory::CommitDurationSampleCountForTesting() const {
  return commit_duration_history_.sample_count();
}
}  // namespace cc
