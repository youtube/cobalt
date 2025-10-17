// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/frame_rate_estimator.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {
namespace {

constexpr auto kInputPriorityDelay = base::Milliseconds(250);

}  // namespace

FrameRateEstimator::FrameRateEstimator(base::SequencedTaskRunner* task_runner)
    : notifier_(
          task_runner,
          base::BindRepeating(&FrameRateEstimator::OnExitInputPriorityMode,
                              base::Unretained(this)),
          kInputPriorityDelay) {}

FrameRateEstimator::~FrameRateEstimator() = default;

void FrameRateEstimator::SetVideoConferenceMode(bool enabled) {
  if (enabled == in_video_conference_mode_) {
    return;
  }

  in_video_conference_mode_ = enabled;
  last_draw_time_in_video_conference_mode_ = base::TimeTicks();
  num_of_consecutive_frames_with_min_delta_ = 0u;
}

void FrameRateEstimator::WillDraw(base::TimeTicks now) {
  TRACE_EVENT1(
      "cc,benchmark", "FrameRateEstimator::WillDraw", "Info",
      base::StringPrintf("\nin_video_conference_mode: %d\ninput_priority_mode: "
                         "%d",
                         in_video_conference_mode_, input_priority_mode_));

  if (!in_video_conference_mode_ || input_priority_mode_) {
    return;
  }

  if (last_draw_time_in_video_conference_mode_ == base::TimeTicks()) {
    last_draw_time_in_video_conference_mode_ = now;
    return;
  }

  auto draw_delta = now - last_draw_time_in_video_conference_mode_;
  last_draw_time_in_video_conference_mode_ = now;

  // If we see that the page is animating consistently at 30 fps or more, then
  // we assume that BeginFrames can not be throttled. But if the animation
  // frequency is lower than that, then using a lower frame rate is permitted.
  // The delta below is to account for minor offsets in frame times.
  constexpr auto kFudgeDelta = base::Milliseconds(1);
  constexpr auto kMinDelta =
      (viz::BeginFrameArgs::DefaultInterval() * 2) - kFudgeDelta;
  if (draw_delta < kMinDelta)
    num_of_consecutive_frames_with_min_delta_++;
  else
    num_of_consecutive_frames_with_min_delta_ = 0u;
}

base::TimeDelta FrameRateEstimator::GetPreferredInterval() const {
  if (input_priority_mode_) {
    return viz::BeginFrameArgs::MinInterval();
  }

  constexpr size_t kMinNumOfFramesWithMinDelta = 4u;
  if (in_video_conference_mode_ &&
      num_of_consecutive_frames_with_min_delta_ < kMinNumOfFramesWithMinDelta) {
    return viz::BeginFrameArgs::DefaultInterval() * 2;
  }

  return viz::BeginFrameArgs::MinInterval();
}

void FrameRateEstimator::NotifyInputEvent() {
  input_priority_mode_ = true;
  notifier_.Schedule();
}

void FrameRateEstimator::OnExitInputPriorityMode() {
  input_priority_mode_ = false;
  last_draw_time_in_video_conference_mode_ = base::TimeTicks();
  num_of_consecutive_frames_with_min_delta_ = 0u;
}

void FrameRateEstimator::DidNotProduceFrame() {
  num_of_consecutive_frames_with_min_delta_ = 0u;
}

}  // namespace cc
