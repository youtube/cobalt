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

#ifndef COBALT_RENDERER_PIPELINE_H_
#define COBALT_RENDERER_PIPELINE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "cobalt/render_tree/animations/node_animations_map.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"
#include "cobalt/renderer/submission.h"
#include "cobalt/renderer/submission_queue.h"

namespace cobalt {
namespace renderer {

// Pipeline is a thread-safe class that setups up a rendering pipeline
// for processing render trees through a rasterizer.  New render trees are
// submitted to the pipeline, from any thread, by calling Submit().  This
// pushes the submitted render tree through a rendering pipeline that eventually
// results in the render tree being submitted to the passed in rasterizer which
// can output the render tree to the display.  A new thread is created which
// hosts the rasterizer submit calls.  Render trees are rasterized as fast
// as the rasterizer will accept them, which is likely to be the display's
// refresh rate.
class Pipeline {
 public:
  typedef base::Callback<scoped_ptr<rasterizer::Rasterizer>()>
      CreateRasterizerFunction;
  typedef base::Callback<void(scoped_array<uint8>, const math::Size&)>
      RasterizationCompleteCallback;

  // Using the provided rasterizer creation function, a rasterizer will be
  // created within the Pipeline on a separate rasterizer thread.  Thus,
  // the rasterizer created by the provided function should only reference
  // thread safe objects.
  Pipeline(const CreateRasterizerFunction& create_rasterizer_function,
           const scoped_refptr<backend::RenderTarget>& render_target,
           backend::GraphicsContext* graphics_context);
  ~Pipeline();

  // Submit a new render tree to the renderer pipeline.  After calling this
  // method, the submitted render tree will be the one that is continuously
  // animated and rendered by the rasterizer.
  void Submit(const Submission& render_tree_submission);

  // Clears the currently submitted render tree submission and waits for the
  // pipeline to be flushed before returning.
  void Clear();

  // |render_tree_submission| will be rasterized into a new offscreen surface.
  // The RGBA pixel data will be extracted from this surface, and |complete|
  // will be called with the pixel data and the dimensions of the image.
  void RasterizeToRGBAPixels(const Submission& render_tree_submission,
                             const RasterizationCompleteCallback& complete);

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

  // Clears the current render tree and calls the callback when this is done.
  void ClearCurrentRenderTree(const base::Closure& clear_complete_callback);

  // Called repeatedly (the rate is limited by the rasterizer, so likely it
  // will be called every 1/60th of a second) on the rasterizer thread and
  // results in the rasterization of the current tree and submission of it to
  // the render target.
  void RasterizeCurrentTree();

  // Rasterize the animated |render_tree_submission| to |render_target|,
  // applying the time_offset in the submission to the animations.
  void RasterizeSubmissionToRenderTarget(
      const Submission& render_tree_submission,
      const scoped_refptr<backend::RenderTarget>& render_target);

  // This method is executed on the rasterizer thread and is responsible for
  // constructing the rasterizer.
  void InitializeRasterizerThread(
      const CreateRasterizerFunction& create_rasterizer_function);

  // Shuts down the submission queue.  This is done on the rasterizer thread
  // and is separate from general shutdown because clearing out the submission
  // queue may result in tasks being posted to the rasterizer thread (e.g.
  // texture deletions).
  void ShutdownSubmissionQueue();

  // This method is executed on the rasterizer thread to shutdown anything that
  // needs to be shutdown from there.
  void ShutdownRasterizerThread();

#if defined(ENABLE_DEBUG_CONSOLE)
  void OnDumpCurrentRenderTree(const std::string&);
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  base::WaitableEvent rasterizer_created_event_;

  // The render_target that all submitted render trees will be rasterized to.
  scoped_refptr<backend::RenderTarget> render_target_;

  backend::GraphicsContext* graphics_context_;

  // A timer that signals to the rasterizer to rasterize the next frame.
  // The timer is setup with a period of 0ms so that it will submit as fast
  // as possible, it is up to the rasterizer to pace the pipeline.  The timer
  // is used to manage the repeated posting of the rasterize task call and
  // to make proper shutdown easier.
  base::optional<base::Timer> rasterize_timer_;

  // ThreadChecker for use by the rasterizer_thread_ defined below.
  base::ThreadChecker rasterizer_thread_checker_;

  // The thread that all rasterization will take place within.
  base::Thread rasterizer_thread_;

  // The rasterizer object that will run on the rasterizer_thread_ and is
  // effectively the last stage of the pipeline, responsible for rasterizing
  // the final render tree and submitting it to the render target.
  scoped_ptr<rasterizer::Rasterizer> rasterizer_;

  // A thread whose only purpose is to destroy submissions/render trees.
  // This is important because destroying a render tree can take some time,
  // and we would like to avoid spending this time on the renderer thread.
  base::Thread submission_disposal_thread_;

  // Manages a queue of render tree submissions that are to be rendered in
  // the future.
  base::optional<SubmissionQueue> submission_queue_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Dumps the current render tree to the console.
  base::ConsoleCommandManager::CommandHandler
      dump_current_render_tree_command_handler_;
#endif
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_PIPELINE_H_
