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

SubmissionQueue::SubmissionQueue(
    size_t max_queue_size, base::TimeDelta time_to_converge,
    const DisposeSubmissionFunction& dispose_function)
    : max_queue_size_(max_queue_size),
      dispose_function_(dispose_function),
      to_submission_time_in_ms_(time_to_converge),
      to_submission_time_cval_(
          "Renderer.ToSubmissionTime", base::TimeDelta(),
          "The current difference in milliseconds between the layout's clock "
          "and the renderer's clock.  The absolute value does not mean much, "
          "but how it changes as the user navigates can show how the "
          "renderer's clock changes relative to layout's clock."),
      queue_size_(
          "Renderer.SubmissionQueueSize", 0,
          "The current size of the renderer submission queue.  Each item in "
          "queue contains a render tree and associated animations.") {}

void SubmissionQueue::PushSubmission(const Submission& submission,
                                     const base::TimeTicks& now) {
  TRACE_EVENT0("cobalt::renderer", "SubmissionQueue::PushSubmission()");

  CheckThatNowIsMonotonicallyIncreasing(now);

  if (submission_queue_.size() >= max_queue_size_) {
    // If we are at capacity, then make room for the new submission by erasing
    // our first element.
    submission_queue_.pop_front();

    // Ensure that the next oldest item in the queue is older than the current
    // time.  If this is not the case, snap time forward so that it is.
    double to_front_submission_time_in_ms =
        (submission_queue_.front().time_offset - render_time(now))
            .InMillisecondsF();
    if (to_submission_time_in_ms_.GetValueAtTime(now) <
        to_front_submission_time_in_ms) {
      to_submission_time_in_ms_.SetTarget(to_front_submission_time_in_ms, now);
      to_submission_time_in_ms_.SnapToTarget();
    }
  }

  base::TimeDelta latest_to_submission_time =
      submission.time_offset - render_time(now);

  double latest_to_submission_time_in_ms =
      latest_to_submission_time.InMillisecondsF();

  to_submission_time_in_ms_.SetTarget(latest_to_submission_time_in_ms, now);

  // Snap time backwards if the incoming submission is in the past.
  if (latest_to_submission_time_in_ms <
      to_submission_time_in_ms_.GetValueAtTime(now)) {
    Reset();
    to_submission_time_in_ms_.SnapToTarget();
  }

  // Save the new submission.
  submission_queue_.push_back(submission);

  // Possibly purge old stale submissions.
  PurgeStaleSubmissionsFromQueue(now);
}

Submission SubmissionQueue::GetCurrentSubmission(const base::TimeTicks& now) {
  TRACE_EVENT0("cobalt::renderer", "SubmissionQueue::GetCurrentSubmission()");

  CheckThatNowIsMonotonicallyIncreasing(now);

  DCHECK(!submission_queue_.empty());

  // First get rid of any stale submissions from our queue.
  PurgeStaleSubmissionsFromQueue(now);

  // Create a new submission with an updated time offset to account for the
  // fact that time has passed since it was submitted.
  Submission updated_time_submission(submission_queue_.front());

  // Our current clock should always be setup (via PushSubmission()) such
  // that it is always larger than the front of the queue.
  DCHECK_GE(to_submission_time_in_ms_.GetValueAtTime(now),
            (submission_queue_.front().time_offset - render_time(now))
                .InMillisecondsF());

  base::TimeDelta updated_time =
      base::TimeDelta::FromMillisecondsD(
          to_submission_time_in_ms_.GetValueAtTime(now)) +
      render_time(now);
  // This if statement is very similar to the DCHECK above, but not exactly
  // the same because of rounding issues.
  if (updated_time > updated_time_submission.time_offset) {
    updated_time_submission.time_offset = updated_time;
  }

  return updated_time_submission;
}

base::TimeDelta SubmissionQueue::render_time(const base::TimeTicks& time) {
  if (!renderer_time_origin_) {
    renderer_time_origin_ = time;
  }

  return time - *renderer_time_origin_;
}

void SubmissionQueue::PurgeStaleSubmissionsFromQueue(
    const base::TimeTicks& time) {
  TRACE_EVENT0("cobalt::renderer",
               "SubmissionQueue::PurgeStaleSubmissionsFromQueue()");
  double current_to_submission_time_in_ms =
      to_submission_time_in_ms_.GetValueAtTime(time);

  SubmissionQueueInternal::iterator submission = submission_queue_.end();
  --submission;

  // If there is more than one element in the queue...
  if (submission != submission_queue_.begin()) {
    // Skip past the submissions that are in the future.  This means we start
    // from the back because the queue is sorted in ascending order of time.
    while (current_to_submission_time_in_ms <
           (submission->time_offset - render_time(time)).InMillisecondsF()) {
      if (submission == submission_queue_.begin()) {
        // It is an invariant of this class that the oldest submission in the
        // queue is older than the current render time.  This should be
        // managed within PushSubmission().
        NOTREACHED();
        break;
      }
      --submission;
    }
  }

  // Delete all previous, old render trees.
  while (submission_queue_.begin() != submission) {
    TRACE_EVENT0("cobalt::renderer", "Delete Render Tree Submission");
    if (!dispose_function_.is_null()) {
      // Package the submission for sending to the disposal callback function.
      // We erase it from our queue before calling the callback in order to
      // ensure that the callback disposes of the Submission object after we
      // do.
      scoped_ptr<Submission> submission(
          new Submission(submission_queue_.front()));
      submission_queue_.pop_front();
      dispose_function_.Run(submission.Pass());
    } else {
      // If no callback is passed in to dispose of submissions for us, just
      // delete it immediately.
      submission_queue_.pop_front();
    }
  }

  // Update our CVal tracking the current (smoothed) to_submission_time value
  // and the one tracking submission queue size.
  to_submission_time_cval_ = base::TimeDelta::FromMilliseconds(
      to_submission_time_in_ms_.GetValueAtTime(time));
  queue_size_ = submission_queue_.size();
}

void SubmissionQueue::CheckThatNowIsMonotonicallyIncreasing(
    const base::TimeTicks& now) {
  if (last_now_) {
    DCHECK(now >= *last_now_);
  }
  last_now_ = now;
}

}  // namespace renderer
}  // namespace cobalt
