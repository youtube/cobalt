// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_SUBMISSION_H_
#define COBALT_RENDERER_SUBMISSION_H_

#include <vector>

#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace renderer {

// A package of all information associated with a render tree submission.
struct Submission {
  // Convenience constructor that assumes there are no animations and sets up
  // an empty animation map.
  explicit Submission(scoped_refptr<render_tree::Node> render_tree)
      : render_tree(render_tree) {}

  // Submit a render tree as well as associated animations.  The
  // time_offset parameter indicates a time that will be used to offset all
  // times passed into animation functions.
  Submission(scoped_refptr<render_tree::Node> render_tree,
             base::TimeDelta time_offset)
      : render_tree(render_tree), time_offset(time_offset) {}

  ~Submission() {
    TRACE_EVENT0("cobalt::renderer", "~Submission()");

    // Explicitly dereference the render tree and animation under the scope of
    // the above trace event.
    render_tree = NULL;
  }

  // Maintains the current render tree that is to be rendered next frame.
  scoped_refptr<render_tree::Node> render_tree;

  // The time from some origin that the associated render tree animations were
  // created.  This permits the render thread to compute times relative
  // to the same origin when updating the animations, as well as hinting
  // at the latency between animation creation and submission to render
  // thread.
  base::TimeDelta time_offset;

  // All callbacks within the vector will be called every time this submission
  // is rasterized.
  std::vector<base::Closure> on_rasterized_callbacks;

  // Information about the specific timeline that this submission is intended
  // to run on.  The most important part of TimelineInfo is TimelineInfo::id,
  // which the renderer pipeline will check to see if it is equal to the
  // submissions timeline, and if so assume animation continuity.  If not, it
  // will reset its submission queue, and possibly apply any animation playback
  // configuration changes specified by the other fields in this structure.
  struct TimelineInfo {
    // An id of -1 is valid, and acts as the default id.
    TimelineInfo()
        : id(-1), allow_latency_reduction(true), max_submission_queue_size(4) {}

    // A number identifying this timeline and used to check for timeline
    // continuity between submissions.  If this changes, the renderer pipeline
    // will reset its submission queue.
    int id;

    // If true, allows the vector from renderer time to submission time to
    // increase over time, in effect reducing latency between when a submission
    // is submitted to when it appears on the screen.  This is typically
    // desirable for interactive applications, but not as necessary for
    // non-interactive content (and in this case can result in some frames
    // being skipped).
    bool allow_latency_reduction;

    // In order to put a bound on memory we set a maximum submission queue size.
    // The queue size refers to how many submissions which the renderer has
    // not caught up to rendering yet will be stored.  If latency reduction
    // is disallowed, this will likely need to be higher to accommodate for
    // the larger latency between submission and render.
    int max_submission_queue_size;
  };
  TimelineInfo timeline_info;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_SUBMISSION_H_
