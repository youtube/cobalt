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

#ifndef COBALT_RENDERER_SUBMISSION_H_
#define COBALT_RENDERER_SUBMISSION_H_

#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/render_tree/animations/node_animations_map.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace renderer {

// A package of all information associated with a render tree submission.
struct Submission {
  // Convenience constructor that assumes there are no animations and sets up
  // an empty animation map.
  explicit Submission(scoped_refptr<render_tree::Node> render_tree)
      : render_tree(render_tree),
        animations(new render_tree::animations::NodeAnimationsMap(
            render_tree::animations::NodeAnimationsMap::Builder().Pass())) {}

  // Submit a render tree as well as associated animations.  The
  // time_offset parameter indicates a time that will be used to offset all
  // times passed into animation functions.
  Submission(
      scoped_refptr<render_tree::Node> render_tree,
      scoped_refptr<render_tree::animations::NodeAnimationsMap> animations,
      base::TimeDelta time_offset)
      : render_tree(render_tree),
        animations(animations),
        time_offset(time_offset) {}

  ~Submission() {
    TRACE_EVENT0("cobalt::renderer", "~Submission()");

    // Explicitly dereference the render tree and animation under the scope of
    // the above trace event.
    render_tree = NULL;
    animations = NULL;
  }

  // Maintains the current render tree that is to be rendered next frame.
  scoped_refptr<render_tree::Node> render_tree;

  // Maintains the current animations that are to be in effect (i.e. applied
  // to current_tree_) for all rasterizations until specifically updated by a
  // call to Submit().
  scoped_refptr<render_tree::animations::NodeAnimationsMap> animations;

  // The time from some origin that the associated render tree animations were
  // created.  This permits the render thread to compute times relative
  // to the same origin when updating the animations, as well as hinting
  // at the latency between animation creation and submission to render
  // thread.
  base::TimeDelta time_offset;

  // If non-null, |on_rasterized_callback| will be called every time this
  // submission is rasterized.
  base::Closure on_rasterized_callback;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_SUBMISSION_H_
