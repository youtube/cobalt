/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/renderer/submission_queue.h"

#include <cmath>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"

namespace cobalt {
namespace renderer {

SubmissionQueue::SubmissionQueue(size_t max_queue_size,
                                 base::TimeDelta time_to_converge)
    : max_queue_size_(max_queue_size),
      renderer_time_origin_(base::TimeTicks::HighResNow()),
      to_submission_time_in_ms_(time_to_converge),
      to_submission_time_in_ms_cval_(
          "Renderer.ToSubmissionTimeInMS", 0,
          "The current difference in milliseconds between the layout's clock "
          "and the renderer's clock.  The absolute value does not mean much, "
          "but how it changes as the user navigates can show how the "
          "renderer's clock changes relative to layout's clock."),
      queue_size_(
          "Renderer.SubmissionQueueSize", 0,
          "The current size of the renderer submission queue.  Each item in "
          "queue contains a render tree and associated animations.") {}

void SubmissionQueue::PushSubmission(const Submission& submission) {
  TRACE_EVENT0("cobalt::renderer", "SubmissionQueue::PushSubmission()");

  if (!submission_queue_.empty()) {
    if (submission.time_offset < submission_queue_.back().time_offset) {
      // We don't support non-monotonically increasing submission times.  If
      // this happens, it is likely due to a source change, so in this case we
      // just reset our state.
      Reset();
    }
  }

  base::TimeDelta latest_to_submission_time_ =
      submission.time_offset - render_time();

  float latest_to_submission_time_in_ms_ =
      static_cast<float>(latest_to_submission_time_.InMillisecondsF());

  to_submission_time_in_ms_.SetTarget(latest_to_submission_time_in_ms_);
  // Snap time backwards if the incoming submission is in the past.
  if (latest_to_submission_time_in_ms_ <
      to_submission_time_in_ms_.GetCurrentValue()) {
    to_submission_time_in_ms_.SnapToTarget();
  }

  if (submission_queue_.size() >= max_queue_size_) {
    // If we are at capacity, then make room for the new submission by erasing
    // our first element.  This will have the unfortunate effect of time
    // snapping forward to the next oldest submission.  The time snapping
    // logic is handled within
    // MoveRenderTimeOffsetTowardsTargetAndPurgeStaleSubmissions().
    submission_queue_.pop_front();
  }

  // Save the new submission.
  submission_queue_.push_back(submission);

  // Possibly purge old stale submissions.
  PurgeStaleSubmissionsFromQueue();
}

Submission SubmissionQueue::GetCurrentSubmission() {
  TRACE_EVENT0("cobalt::renderer", "SubmissionQueue::GetCurrentSubmission()");

  DCHECK(!submission_queue_.empty());

  // First get rid of any stale submissions from our queue.
  PurgeStaleSubmissionsFromQueue();

  // Create a new submission with an updated time offset to account for the
  // fact that time has passed since it was submitted.
  Submission updated_time_submission(submission_queue_.front());
  updated_time_submission.time_offset =
      base::TimeDelta::FromMillisecondsD(
          static_cast<double>(to_submission_time_in_ms_.GetCurrentValue())) +
      render_time();

  return updated_time_submission;
}

base::TimeDelta SubmissionQueue::render_time() const {
  return base::TimeTicks::HighResNow() - renderer_time_origin_;
}

void SubmissionQueue::PurgeStaleSubmissionsFromQueue() {
  TRACE_EVENT0("cobalt::renderer",
               "SubmissionQueue::PurgeStaleSubmissionsFromQueue()");

  SubmissionQueueInternal::iterator submission = submission_queue_.end();
  --submission;

  float current_to_submission_time_in_ms =
      to_submission_time_in_ms_.GetCurrentValue();
  base::TimeDelta current_to_submission_time =
      base::TimeDelta::FromMillisecondsD(
          static_cast<double>(current_to_submission_time_in_ms));

  // Skip past the submissions that are in the future.  This means we start
  // from the back because the queue is sorted in ascending order of time.
  while (current_to_submission_time + render_time() < submission->time_offset) {
    DCHECK(submission != submission_queue_.begin());
    --submission;
  }

  // Delete all previous, old render trees.
  while (submission_queue_.begin() != submission) {
    TRACE_EVENT0("cobalt::renderer", "Delete Render Tree Submission");
    submission_queue_.pop_front();
  }

  // Update our CVal tracking the current (smoothed) to_submission_time value
  // and the one tracking submission queue size.
  to_submission_time_in_ms_cval_ = current_to_submission_time_in_ms;
  queue_size_ = submission_queue_.size();
}

}  // namespace renderer
}  // namespace cobalt
