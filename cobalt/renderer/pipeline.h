/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef RENDERER_PIPELINE_H_
#define RENDERER_PIPELINE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "cobalt/render_tree/animations/node_animations_map.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/rasterizer.h"

namespace cobalt {
namespace renderer {

// Pipeline is a thread-safe class that setups up a rendering pipeline
// for processing render trees through a rasterizer.  New render trees are
// submitted to the pipeline, from any thread, by calling Submit().  This
// pushes the submitted render tree through a rendering pipeline that eventually
// results in the render tree being submitted to the passed in rasterizer which
// can output the render tree to the display.  A new thread is created which
// hosts the rasterizer submit calls.  A timer is setup on the rasterizer thread
// so that rasterizations of the last submitted render tree are rate-limited
// to a specific frequency, such as 60hz, the refresh rate of most displays.
class Pipeline {
 public:
  typedef base::Callback<scoped_ptr<Rasterizer>()> CreateRasterizerFunction;

  // Using the provided rasterizer creation function, a rasterizer will be
  // created within the Pipeline on a separate rasterizer thread.  Thus,
  // the rasterizer created by the provided function should only reference
  // thread safe objects.
  Pipeline(const CreateRasterizerFunction& create_rasterizer_function,
           const scoped_refptr<backend::RenderTarget>& render_target);
  ~Pipeline();

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

    // Submit a render tree with animations as well as a callback function that
    // will be called every time the submission is rasterized.
    Submission(
        scoped_refptr<render_tree::Node> render_tree,
        scoped_refptr<render_tree::animations::NodeAnimationsMap> animations,
        base::TimeDelta time_offset, base::Closure submit_complete_callback)
        : render_tree(render_tree),
          animations(animations),
          time_offset(time_offset),
          submit_complete_callback(submit_complete_callback) {}

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

    // A callback to be run each time a frame submission completes.
    base::Closure submit_complete_callback;
  };

  // Submit a new render tree to the renderer pipeline.  After calling this
  // method, the submitted render tree will be the one rendered by the
  // rasterizer at the refresh rate.
  void Submit(const Submission& render_tree_submission);

  // Returns the rate, in hertz, at which the current render tree is rasterized
  // and submitted to the display.
  float refresh_rate() const { return refresh_rate_; }

  // Returns a thread-safe object from which one can produce renderer resources
  // like images and fonts which can be referenced by render trees that are
  // subsequently submitted to this pipeline.
  render_tree::ResourceProvider* GetResourceProvider();

 private:
  // All private data members should be accessed only on the rasterizer thread,
  // with the exception of rasterizer_thread_ itself through which messages
  // are posted.

  // Called by Submit() to do the work of actually setting the newly submitted
  // render tree.  This method will be called on the rasterizer thread.
  void SetNewRenderTree(const Submission& render_tree_submission);

  // Called at a specified refresh rate (e.g. 60hz) on the rasterizer thread and
  // results in the rasterization of the current tree and submission of it to
  // the render target.
  void RasterizeCurrentTree();

  // This method is executed on the rasterizer thread and is responsible for
  // constructing the rasterizer.
  void InitializeRasterizerThread(
      const CreateRasterizerFunction& create_rasterizer_function);

  // This method is executed on the rasterizer thread to shutdown anything that
  // needs to be shutdown from there.
  void ShutdownRasterizerThread();

  // The refresh rate of the rasterizer.
  const float refresh_rate_;

  // The rasterizer object that will run on the rasterizer_thread_ and is
  // effectively the last stage of the pipeline, responsible for rasterizing
  // the final render tree and submitting it to the render target.
  scoped_ptr<Rasterizer> rasterizer_;
  base::WaitableEvent rasterizer_created_event_;

  // The render_target that all submitted render trees will be rasterized to.
  scoped_refptr<backend::RenderTarget> render_target_;

  // We hold a reference to the last submitted render tree (and auxiliary
  // information) and use that as the target render tree that we will render
  // each frame until a new tree is submitted.
  base::optional<Submission> last_submission_;

  // The time at which we started animating the last submitted render tree.
  // This is used to compute the time delta passed to pass the animation
  // functions each frame (and is also offset with time_offset above).
  base::optional<base::TimeTicks> last_submission_render_start_time_;

  // A timer that signals to the rasterizer to rasterize the next frame.
  // It is common for this to be set to 60hz, the refresh rate of most displays.
  // It is an optional so that it can be constructed and destructed in the
  // rasterizer thread.
  base::optional<base::Timer> refresh_rate_timer_;

  // ThreadChecker for use by the rasterizer_thread_ defined below.
  base::ThreadChecker rasterizer_thread_checker_;

  // The thread that all rasterization will take place within.
  base::optional<base::Thread> rasterizer_thread_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // RENDERER_PIPELINE_H_
