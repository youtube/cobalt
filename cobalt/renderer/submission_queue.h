// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_SUBMISSION_QUEUE_H_
#define COBALT_RENDERER_SUBMISSION_QUEUE_H_

#include <list>
#include <memory>
#include <string>

#include "base/time/time.h"
#include "cobalt/base/c_val.h"
#include "cobalt/renderer/smoothed_value.h"
#include "cobalt/renderer/submission.h"

namespace cobalt {
namespace renderer {

// The submission queue encapsulates the logic behind selecting which
// of the recent render tree submissions should be used to render at any
// given time, and what time offset should be used to render them at.  It
// manages smoothing between submissions whose animations have differing time
// offsets.
//
// As an example, consider the following timelines.  Assume that a layout engine
// is producing submissions, and the top timeline represents the layout engine's
// time.  To maintain generality, we will call this the submission timeline.
// The bottom timeline is the renderer timeline.
//
//  Submission Timeline
//
//  {} ----A------------------B------------C--------------------------D-------->
//    .     \                  \            \                          \       .
//    .      ---------          ---------    \                          \      .
//    .               \                  \    \                          \     .
//  () ----------------aA-----------------bB---c--------C-----------------d-D-->
//
//  Renderer Timeline
//
// In the diagram above, {X} represents the event that submission X was
// created with the specified designated submission timeline time.  Event (x)
// represents that the renderer received submission X at the specified renderer
// timeline time.  Event (X) represents that the renderer displayed submission
// X at the specified time.
//
// Ideally, we would like to keep the time spacing between subsequent (X) events
// equal to the spacing between subsequent {X} events.  Note that when the first
// submission arrives at the renderer, event (a), we can display it right away
// since it is the first submission and no spacing has been established yet.
//
// Next, we see that the spacing between {B} and (b) is equal to the spacing
// between {A} and (a), so we can display (B) immediately (like we did with (A))
// as well.
//
// When (c) occurs however, we see that it took much less time for the renderer
// to receive the submission, (c), since it was created, {C}.  This may happen
// if for example a layout engine performed a relatively quick layout to produce
// submission C.  In order to maintain a similar distance between {C} and
// (C) as we saw for {B} and (B), we must delay rendering submission C for a
// bit.  In this case, we store submission C in the queue and only display it
// when the time is right, at which point we also purge the old submission B.
//
// If we find that the time between {X} and (x) is consistently small, we would
// like to start showing (X) sooner since the longer we wait to display it,
// the larger the input lag.  Thus, as time goes on we slowly change our wait
// time between {X} and (X) to match the last seen time difference between {X}
// and (x).  In the case above, we see that when (d) arrives, because submission
// C had previously established a faster response time, and D is similar, we
// are able to show (D) almost right away.
//
// In the code below, the set target value of |to_submission_time_in_ms_|
// represents the time difference {X} - (x).  This value is represented by the
// vertical lines in the diagram above.  The smoothed value of
// |to_submission_time_in_ms_| (or in other words,
// |to_submission_time_in_ms_.GetCurrentValue()|) is the time difference
// {X} - (X) that slowly and smoothly is always moving towards the target,
// {X} - (x).
//

class SubmissionQueue {
 public:
  typedef base::Callback<void(std::unique_ptr<Submission>)>
      DisposeSubmissionFunction;

  // |max_queue_size| indicates the maximum size of the submission queue.  If
  // a new submission is pushed which would increase the queue size to its
  // maximum, we drop the oldest submission and snap to the time of the next
  // one.  It provides a bound on the number of intermediate submissions, and
  // so, memory.
  // |time_to_converge| is a time value that indicates how long each transition
  // between time values will take.
  // |dispose_function| specifies a function that will be called and
  // passed a Submission that the submission queue is done with.  This may be
  // used to allow the Submission/render tree to be disposed/destroyed on a
  // separate thread.
  SubmissionQueue(size_t max_queue_size, base::TimeDelta time_to_converge,
                  bool allow_latency_reduction = true,
                  const DisposeSubmissionFunction& dispose_function =
                      DisposeSubmissionFunction());

  // Pushes a new submission into the submission queue, possibly updating
  // internal timing parameters based on the submission's time offset.
  void PushSubmission(const Submission& submission, const base::TimeTicks& now);

  // For the current time, returns a submission to be used for rendering, with
  // timing information already setup.  Time must be monotonically increasing.
  Submission GetCurrentSubmission(const base::TimeTicks& now);

  // Returns the corresponding submission time for a given TimeTicks
  // "real world" system value.
  base::TimeDelta submission_time(const base::TimeTicks& time);

  // Returns the corresponding renderer time for a given TimeTicks value
  // (e.g. base::TimeTicks::Now()).
  base::TimeDelta render_time(const base::TimeTicks& time);

 private:
  typedef std::list<Submission> SubmissionQueueInternal;

  void PurgeStaleSubmissionsFromQueue(const base::TimeTicks& time);

  void CheckThatNowIsMonotonicallyIncreasing(const base::TimeTicks& now);

  // The maximum size of the queue.  If we go over this, we snap time forward.
  const size_t max_queue_size_;

  // Function to call before releasing a handle on a render tree.
  DisposeSubmissionFunction dispose_function_;

  // An arbitrary time chosen upon construction to fully specify the renderer
  // timeline.  The first time |render_time(t)| is called, this will be set
  // to |t| such that the first time it is called, |render_time(t)| will return
  // 0.  Theoretically, its actual value doesn't really matter, but this method
  // keeps the origin on the same order as the current clock values in order
  // to avoid the chance of floating point error.
  base::Optional<base::TimeTicks> renderer_time_origin_;

  // The queue of submissions, sorted in ascending order of times.
  SubmissionQueueInternal submission_queue_;

  // A good way to think of this value is that adding it to render_time() gives
  // you a time on the source (e.g. the submissions) timeline.  So, for example,
  // to see if an incoming submission time, s, is in the renderer's past, you
  // could check if
  //   base::TimeTicks now = base::TimeTicks::Now();
  //   s.time_offset - render_time(now) <
  //       base::TimeDelta::FromMillisecondsD(
  //           to_submission_time_in_ms_.GetCurrentValue(now))
  // is true.
  SmoothedValue to_submission_time_in_ms_;

  // Debug value to help DCHECK that input |now| values are monotonically
  // increasing.
  base::Optional<base::TimeTicks> last_now_;

  base::CVal<base::TimeDelta> to_submission_time_cval_;
  base::CVal<size_t> queue_size_;

  // If false, we will only ever allow to_submission_time_cval_ to move
  // backwards ensuring that animations never speed up during playback (at the
  // cost of increased and non-recoverable input latency).  This is good for
  // non-interactive content.
  const bool allow_latency_reduction_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_SUBMISSION_QUEUE_H_
