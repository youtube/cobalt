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

#include "cobalt/renderer/pipeline.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"

using cobalt::render_tree::Node;
using cobalt::render_tree::animations::NodeAnimationsMap;

namespace cobalt {
namespace renderer {

namespace {
// In order to put a bound on memory we set a maximum submission queue size that
// is empirically found to be a nice balance between animation smoothing and
// memory usage.
const size_t kMaxSubmissionQueueSize = 4u;

// How quickly the renderer time adjusts to changing submission times.
// 500ms is chosen as a default because it is fast enough that the user will not
// usually notice input lag from a slow timeline renderer, but slow enough that
// quick updates while a quick animation is playing should not jank.
const double kTimeToConvergeInMS = 500.0;

// The stack size to be used for the renderer thread.  This is must be large
// enough to support recursing on the render tree.
const int kRendererThreadStackSize = 128 * 1024;

void DestructSubmissionOnMessageLoop(MessageLoop* message_loop,
                                     scoped_ptr<Submission> submission) {
  TRACE_EVENT0("cobalt::renderer", "DestructSubmissionOnMessageLoop()");
  if (MessageLoop::current() != message_loop) {
    message_loop->DeleteSoon(FROM_HERE, submission.release());
  }
}

}  // namespace

Pipeline::Pipeline(const CreateRasterizerFunction& create_rasterizer_function,
                   const scoped_refptr<backend::RenderTarget>& render_target,
                   backend::GraphicsContext* graphics_context)
    : rasterizer_created_event_(true, false),
      render_target_(render_target),
      graphics_context_(graphics_context),
      rasterizer_thread_("Rasterizer"),
      submission_disposal_thread_("Rasterizer Submission Disposal") {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::Pipeline()");
  // The actual Pipeline can be constructed from any thread, but we want
  // rasterizer_thread_checker_ to be associated with the rasterizer thread,
  // so we detach it here and let it reattach itself to the rasterizer thread
  // when CalledOnValidThread() is called on rasterizer_thread_checker_ below.
  rasterizer_thread_checker_.DetachFromThread();

  rasterizer_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_DEFAULT, kRendererThreadStackSize,
                            base::kThreadPriority_Highest));

  rasterizer_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::InitializeRasterizerThread, base::Unretained(this),
                 create_rasterizer_function));
}

Pipeline::~Pipeline() {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::~Pipeline()");

  // First we shutdown the submission queue.  We do this as a seperate step from
  // rasterizer shutdown because it may post messages back to the rasterizer
  // thread as it clears itself out (e.g. it may ask the rasterizer thread to
  // delete textures).  We wait for this shutdown to complete before proceeding
  // to shutdown the rasterizer thread.
  base::WaitableEvent submission_queue_shutdown(true, false);
  rasterizer_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::ShutdownSubmissionQueue, base::Unretained(this)));
  rasterizer_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&submission_queue_shutdown)));
  submission_queue_shutdown.Wait();

  // Submit a shutdown task to the rasterizer thread so that it can shutdown
  // anything that must be shutdown from that thread.
  rasterizer_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::ShutdownRasterizerThread, base::Unretained(this)));

  rasterizer_thread_.Stop();
}

render_tree::ResourceProvider* Pipeline::GetResourceProvider() {
  rasterizer_created_event_.Wait();
  return rasterizer_->GetResourceProvider();
}

void Pipeline::Submit(const Submission& render_tree_submission) {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::Submit()");
  // Execute the actual set of the new render tree on the rasterizer tree.
  rasterizer_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&Pipeline::SetNewRenderTree, base::Unretained(this),
                            render_tree_submission));
}

void Pipeline::Clear() {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::Clear()");
  base::WaitableEvent wait_event(true, false);
  rasterizer_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::ClearCurrentRenderTree, base::Unretained(this),
                 base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&wait_event))));
  wait_event.Wait();
}

void Pipeline::RasterizeToRGBAPixels(
    const Submission& render_tree_submission,
    const RasterizationCompleteCallback& complete) {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::RasterizeToRGBAPixels()");

  if (MessageLoop::current() != rasterizer_thread_.message_loop()) {
    rasterizer_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&Pipeline::RasterizeToRGBAPixels, base::Unretained(this),
                   render_tree_submission, complete));
    return;
  }
  // Create a new target that is the same dimensions as the display target.
  scoped_refptr<backend::RenderTarget> offscreen_target =
      graphics_context_->CreateOffscreenRenderTarget(render_target_->GetSize());

  // Rasterize this submission into the newly created target.
  RasterizeSubmissionToRenderTarget(render_tree_submission, offscreen_target);

  // Load the texture's pixel data into a CPU memory buffer and return it.
  complete.Run(graphics_context_->DownloadPixelDataAsRGBA(offscreen_target),
               render_target_->GetSize());
}

void Pipeline::SetNewRenderTree(const Submission& render_tree_submission) {
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  DCHECK(render_tree_submission.render_tree.get());

  TRACE_EVENT0("cobalt::renderer", "Pipeline::SetNewRenderTree()");

  submission_queue_->PushSubmission(render_tree_submission,
                                    base::TimeTicks::Now());

  // Start the rasterization timer if it is not yet started.
  if (!rasterize_timer_) {
    // We submit render trees as fast as the rasterizer can consume them.
    // Practically, this will result in the rate being limited to the
    // display's refresh rate.
    rasterize_timer_.emplace(
        FROM_HERE, base::TimeDelta(),
        base::Bind(&Pipeline::RasterizeCurrentTree, base::Unretained(this)),
        true, true);
    rasterize_timer_->Reset();
  }
}

void Pipeline::ClearCurrentRenderTree(
    const base::Closure& clear_complete_callback) {
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("cobalt::renderer", "Pipeline::ClearCurrentRenderTree()");

  submission_queue_->Reset();
  rasterize_timer_ = base::nullopt;

  if (!clear_complete_callback.is_null()) {
    clear_complete_callback.Run();
  }
}

void Pipeline::RasterizeCurrentTree() {
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("cobalt::renderer", "Pipeline::RasterizeCurrentTree()");

  // Rasterize the last submitted render tree.
  RasterizeSubmissionToRenderTarget(
      submission_queue_->GetCurrentSubmission(base::TimeTicks::Now()),
      render_target_);
}

void Pipeline::RasterizeSubmissionToRenderTarget(
    const Submission& submission,
    const scoped_refptr<backend::RenderTarget>& render_target) {
  TRACE_EVENT0("cobalt::renderer",
               "Pipeline::RasterizeSubmissionToRenderTarget()");
  // Animate the render tree using the submitted animations.
  scoped_refptr<Node> animated_render_tree = submission.animations->Apply(
      submission.render_tree, submission.time_offset);

  // Rasterize the animated render tree.
  rasterizer_->Submit(animated_render_tree, render_target);

  if (!submission.on_rasterized_callback.is_null()) {
    submission.on_rasterized_callback.Run();
  }
}

void Pipeline::InitializeRasterizerThread(
    const CreateRasterizerFunction& create_rasterizer_function) {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::InitializeRasterizerThread");
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  rasterizer_ = create_rasterizer_function.Run();
  rasterizer_created_event_.Signal();

  // Note that this is setup as high priority, but lower than the rasterizer
  // thread's priority (kThreadPriority_Highest).  This is to ensure that
  // we never interrupt the rasterizer in order to dispose render trees, but
  // at the same time we do want to prioritize cleaning them up to avoid
  // large queues of pending render tree disposals.
  submission_disposal_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_DEFAULT, kRendererThreadStackSize,
                            base::kThreadPriority_High));

  submission_queue_.emplace(
      kMaxSubmissionQueueSize,
      base::TimeDelta::FromMillisecondsD(kTimeToConvergeInMS),
      base::Bind(&DestructSubmissionOnMessageLoop,
                 submission_disposal_thread_.message_loop()));
}

void Pipeline::ShutdownSubmissionQueue() {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::ShutdownSubmissionQueue()");
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  // Stop and shutdown the raterizer timer.  If we won't have a submission
  // queue anymore, we won't be able to rasterize anymore.
  rasterize_timer_ = base::nullopt;

  // Do not retain any more references to the current render tree (which
  // may refer to rasterizer resources) or animations which may refer to
  // render trees.
  submission_queue_ = base::nullopt;

  // Shut down our submission disposer thread.  This needs to happen now to
  // ensure that any pending "dispose" messages are processed.  Each disposal
  // may result in new messages being posted to this rasterizer thread's message
  // loop, and so we want to make sure these are all queued up before
  // proceeding.
  submission_disposal_thread_.Stop();
}

void Pipeline::ShutdownRasterizerThread() {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::ShutdownRasterizerThread()");
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  // Finally, destroy the rasterizer.
  rasterizer_.reset();
}

}  // namespace renderer
}  // namespace cobalt
