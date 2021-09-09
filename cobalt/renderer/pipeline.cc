// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/renderer/pipeline.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/address_sanitizer.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/extension/graphics.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/clear_rect_node.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/dump_render_tree_to_string.h"
#include "nb/memory_scope.h"
#include "starboard/system.h"

using cobalt::render_tree::Node;
using cobalt::render_tree::animations::AnimateNode;

namespace cobalt {
namespace renderer {

namespace {

#if !defined(COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS)
// This default value has been moved from cobalt/build/cobalt_configuration.gypi
// in favor of the usage of
// CobaltExtensionGraphicsApi::GetMinimumFrameIntervalInMilliseconds API.
const float kCobaltMinimumFrameTimeInMilliseconds = 16.0f;
#endif
// How quickly the renderer time adjusts to changing submission times.
// 500ms is chosen as a default because it is fast enough that the user will not
// usually notice input lag from a slow timeline renderer, but slow enough that
// quick updates while a quick animation is playing should not jank.
const double kTimeToConvergeInMS = 500.0;

// The stack size to be used for the renderer thread.  This is must be large
// enough to support recursing on the render tree.
#if defined(COBALT_BUILD_TYPE_DEBUG)
const int kRendererThreadStackSize =
    256 * 1024 + base::kAsanAdditionalStackSize;
#else
const int kRendererThreadStackSize =
    128 * 1024 + base::kAsanAdditionalStackSize;
#endif

// How many entries the rasterize periodic timer will contain before updating.
const size_t kRasterizePeriodicTimerEntriesPerUpdate = 60;

// The maximum number of entries that the rasterize animations timer can contain
// before automatically updating. In the typical use case, the update will
// occur manually when the animations expire.
const size_t kRasterizeAnimationsTimerMaxEntries = 60;

void DestructSubmissionOnMessageLoop(base::MessageLoop* message_loop,
                                     std::unique_ptr<Submission> submission) {
  TRACE_EVENT0("cobalt::renderer", "DestructSubmissionOnMessageLoop()");
  if (base::MessageLoop::current() != message_loop) {
    message_loop->task_runner()->DeleteSoon(FROM_HERE, submission.release());
  }
}

bool ShouldClearFrameOnShutdown(render_tree::ColorRGBA* out_clear_color) {
  const CobaltExtensionGraphicsApi* graphics_extension =
      static_cast<const CobaltExtensionGraphicsApi*>(
          SbSystemGetExtension(kCobaltExtensionGraphicsName));
  if (graphics_extension &&
      strcmp(graphics_extension->name, kCobaltExtensionGraphicsName) == 0 &&
      graphics_extension->version >= 4) {
    float r, g, b, a;
    if (graphics_extension->ShouldClearFrameOnShutdown(&r, &g, &b, &a)) {
      out_clear_color->set_r(r);
      out_clear_color->set_g(g);
      out_clear_color->set_b(b);
      out_clear_color->set_a(a);
      return true;
    }
    return false;
  }

  // Default is to clear to opaque black.
  out_clear_color->set_r(0.0f);
  out_clear_color->set_g(0.0f);
  out_clear_color->set_b(0.0f);
  out_clear_color->set_a(1.0f);
  return true;
}

}  // namespace

Pipeline::Pipeline(const CreateRasterizerFunction& create_rasterizer_function,
                   const scoped_refptr<backend::RenderTarget>& render_target,
                   backend::GraphicsContext* graphics_context,
                   bool submit_even_if_render_tree_is_unchanged,
                   ShutdownClearMode clear_on_shutdown_mode,
                   const Options& options)
    : rasterizer_created_event_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      render_target_(render_target),
      graphics_context_(graphics_context),
      rasterizer_thread_("Rasterizer"),
      submission_disposal_thread_("RasterzrSubDisp"),
      submit_even_if_render_tree_is_unchanged_(
          submit_even_if_render_tree_is_unchanged),
      last_did_rasterize_(false),
      last_animations_expired_(true),
      last_stat_tracked_animations_expired_(true),
      rasterize_animations_timer_("Renderer.Rasterize.Animations",
                                  kRasterizeAnimationsTimerMaxEntries,
                                  true /*enable_entry_list_c_val*/),
      ALLOW_THIS_IN_INITIALIZER_LIST(rasterize_periodic_interval_timer_(
          "Renderer.Rasterize.DurationInterval",
          kRasterizeAnimationsTimerMaxEntries, true /*enable_entry_list_c_val*/,
          base::Bind(&Pipeline::FrameStatsOnFlushCallback,
                     base::Unretained(this)))),
      rasterize_animations_interval_timer_(
          "Renderer.Rasterize.AnimationsInterval",
          kRasterizeAnimationsTimerMaxEntries,
          true /*enable_entry_list_c_val*/),
      new_render_tree_rasterize_count_(
          "Count.Renderer.Rasterize.NewRenderTree", 0,
          "Total number of new render trees rasterized."),
      new_render_tree_rasterize_time_(
          "Time.Renderer.Rasterize.NewRenderTree", 0,
          "The last time a new render tree was rasterized."),
      has_active_animations_c_val_(
          "Renderer.HasActiveAnimations", false,
          "Is non-zero if the current render tree has active animations."),
      animations_start_time_(
          "Time.Renderer.Rasterize.Animations.Start", 0,
          "The most recent time animations started playing."),
      animations_end_time_("Time.Renderer.Rasterize.Animations.End", 0,
                           "The most recent time animations ended playing."),
      fallback_rasterize_count_(
          "Count.Renderer.Rasterize.FallbackRasterize", 0,
          "Total number of times Skia was used to render a "
          "non-text render tree node."),
#if defined(ENABLE_DEBUGGER)
      ALLOW_THIS_IN_INITIALIZER_LIST(dump_current_render_tree_command_handler_(
          "dump_render_tree",
          base::Bind(&Pipeline::OnDumpCurrentRenderTree,
                     base::Unretained(this)),
          "Dumps the current render tree to text.",
          "Dumps the current render tree either to the console if no parameter "
          "is specified, or to a file with the specified filename relative to "
          "the debug output folder.")),
      ALLOW_THIS_IN_INITIALIZER_LIST(toggle_fps_stdout_command_handler_(
          "toggle_fps_stdout",
          base::Bind(&Pipeline::OnToggleFpsStdout, base::Unretained(this)),
          "Toggles printing framerate stats to stdout.",
          "When enabled, at the end of each animation (or every time a maximum "
          "number of frames are rendered), framerate statistics are printed "
          "to stdout.")),
      ALLOW_THIS_IN_INITIALIZER_LIST(toggle_fps_overlay_command_handler_(
          "toggle_fps_overlay",
          base::Bind(&Pipeline::OnToggleFpsOverlay, base::Unretained(this)),
          "Toggles rendering framerate stats to an overlay on the display.",
          "Framerate statistics are rendered to a display overlay.  The "
          "numbers are updated at the end of each animation (or every time a "
          "maximum number of frames are rendered), framerate statistics are "
          "printed to stdout.")),
#endif  // defined(ENABLE_DEBUGGER)
      clear_on_shutdown_mode_(clear_on_shutdown_mode),
      enable_fps_stdout_(options.enable_fps_stdout),
      enable_fps_overlay_(options.enable_fps_overlay),
      fps_overlay_update_pending_(false) {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::Pipeline()");
  // The actual Pipeline can be constructed from any thread, but we want
  // rasterizer_thread_checker_ to be associated with the rasterizer thread,
  // so we detach it here and let it reattach itself to the rasterizer thread
  // when CalledOnValidThread() is called on rasterizer_thread_checker_ below.
  DETACH_FROM_THREAD(rasterizer_thread_checker_);

  base::Thread::Options thread_options(base::MessageLoop::TYPE_DEFAULT,
                                       kRendererThreadStackSize);
  thread_options.priority = base::ThreadPriority::HIGHEST;
  rasterizer_thread_.StartWithOptions(thread_options);

  rasterizer_thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::InitializeRasterizerThread, base::Unretained(this),
                 create_rasterizer_function));
}

Pipeline::~Pipeline() {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::~Pipeline()");

  // First we shutdown the submission queue.  We do this as a separate step from
  // rasterizer shutdown because it may post messages back to the rasterizer
  // thread as it clears itself out (e.g. it may ask the rasterizer thread to
  // delete textures).  We wait for this shutdown to complete before proceeding
  // to shutdown the rasterizer thread.
  rasterizer_thread_.message_loop()->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&Pipeline::ShutdownSubmissionQueue, base::Unretained(this)));

  // Submit a shutdown task to the rasterizer thread so that it can shutdown
  // anything that must be shutdown from that thread.
  rasterizer_thread_.message_loop()->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&Pipeline::ShutdownRasterizerThread, base::Unretained(this)));

  // Finally shutdown the rasterizer. Do this after ShutdownRasterizerThread()
  // has run, just in case it posted more tasks to the rasterizer thread. This
  // will free the rasterizer after the posted tasks have executed (unless they
  // were delayed tasks).
  rasterizer_thread_.message_loop()->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&Pipeline::ShutdownRasterizer, base::Unretained(this)));

  rasterizer_thread_.Stop();
}

render_tree::ResourceProvider* Pipeline::GetResourceProvider() {
  rasterizer_created_event_.Wait();
  return rasterizer_->GetResourceProvider();
}

bool Pipeline::IsMapToMeshEnabled(const Pipeline* pipeline) {
  backend::GraphicsContext* graphics_context =
      pipeline ? pipeline->graphics_context_ : nullptr;
  return backend::GraphicsContext::IsMapToMeshEnabled(graphics_context);
}

void Pipeline::Submit(const Submission& render_tree_submission) {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::Submit()");

  // Execute the actual set of the new render tree on the rasterizer tree.
  rasterizer_thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&Pipeline::SetNewRenderTree, base::Unretained(this),
                            CollectAnimations(render_tree_submission)));
}

void Pipeline::Clear() {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::Clear()");
  rasterizer_thread_.message_loop()->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&Pipeline::ClearCurrentRenderTree, base::Unretained(this)));
}

void Pipeline::RasterizeToRGBAPixels(
    const scoped_refptr<render_tree::Node>& render_tree_root,
    const base::Optional<math::Rect>& clip_rect,
    const RasterizationCompleteCallback& complete) {
  TRACK_MEMORY_SCOPE("Renderer");
  TRACE_EVENT0("cobalt::renderer", "Pipeline::RasterizeToRGBAPixels()");

  if (base::MessageLoop::current() != rasterizer_thread_.message_loop()) {
    rasterizer_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&Pipeline::RasterizeToRGBAPixels, base::Unretained(this),
                   render_tree_root, clip_rect, complete));
    return;
  }

  // The default render_rect is the the whole display target.
  const math::Rect& render_rect =
      clip_rect ? clip_rect.value() : math::Rect(render_target_->GetSize());

  // Create a new target that is the same dimensions as the render_rect.
  scoped_refptr<backend::RenderTarget> offscreen_target =
      graphics_context_->CreateDownloadableOffscreenRenderTarget(
          render_rect.size());

  // Offset the whole render tree to put the render_rect at the origin.
  scoped_refptr<render_tree::Node> offset_tree_root =
      new render_tree::CompositionNode(
          render_tree_root.get(),
          math::Vector2dF(-render_rect.x(), -render_rect.y()));

  scoped_refptr<render_tree::Node> animate_node =
      new render_tree::animations::AnimateNode(offset_tree_root);

  Submission submission = Submission(animate_node);
  // Rasterize this submission into the newly created target.
  RasterizeSubmissionToRenderTarget(submission, offscreen_target, true);

  // Load the texture's pixel data into a CPU memory buffer and return it.
  complete.Run(graphics_context_->DownloadPixelDataAsRGBA(offscreen_target),
               render_rect.size());
}

void Pipeline::TimeFence(base::TimeDelta time_fence) {
  TRACK_MEMORY_SCOPE("Renderer");
  TRACE_EVENT0("cobalt::renderer", "Pipeline::TimeFence()");

  if (base::MessageLoop::current() != rasterizer_thread_.message_loop()) {
    rasterizer_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&Pipeline::TimeFence, base::Unretained(this), time_fence));
    return;
  }

  if (!time_fence_) {
    time_fence_ = time_fence;
  } else {
    LOG(ERROR) << "Attempting to set a time fence while one was already set.";
  }
}

void Pipeline::SetNewRenderTree(const Submission& render_tree_submission) {
  DCHECK_CALLED_ON_VALID_THREAD(rasterizer_thread_checker_);
  DCHECK(render_tree_submission.render_tree.get());

  TRACE_EVENT0("cobalt::renderer", "Pipeline::SetNewRenderTree()");

  // If a time fence is active, save the submission to be queued only after
  // we pass the time fence.  Overwrite any existing waiting submission in this
  // case.
  if (time_fence_) {
    post_fence_submission_ = render_tree_submission;
    post_fence_receipt_time_ = base::TimeTicks::Now();
    return;
  }

  QueueSubmission(render_tree_submission, base::TimeTicks::Now());

  // Start the rasterization timer if it is not yet started.
  if (!rasterize_timer_) {
    // Artificially limit the period between submissions. This is useful for
    // platforms which do not rate limit themselves during swaps. Be careful
    // to use a non-zero interval time even if throttling occurs during frame
    // swaps. It is possible that a submission is not rendered (this can
    // happen if the render tree has not changed between submissions), so no
    // frame swap occurs, and the minimum frame time is the only throttle.
    float minimum_frame_interval_milliseconds =
        graphics_context_
            ? graphics_context_->GetMinimumFrameIntervalInMilliseconds()
            : -1.0f;
#if SB_API_VERSION >= 12 && defined(COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS)
#error \
    "'cobalt_minimum_frame_time_in_milliseconds' was replaced by" \
    "CobaltExtensionGraphicsApi::GetMinimumFrameIntervalInMilliseconds."
#elif SB_API_VERSION < 12 && defined(COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS)
    COMPILE_ASSERT(COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS > 0,
                   frame_time_must_be_positive);
    if (minimum_frame_interval_milliseconds < 0.0f) {
      minimum_frame_interval_milliseconds =
          COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS;
    } else {
      DLOG(ERROR)
          << "COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS and "
             "CobaltExtensionGraphicsApi::GetMinimumFrameIntervalInMilliseconds"
             "are both defined."
             "Remove the 'cobalt_minimum_frame_time_in_milliseconds' ";
      "from ../gyp_configuration.gypi in favor of the usage of "
      "CobaltExtensionGraphicsApi::GetMinimumFrameIntervalInMilliseconds."
    }
#else
    if (minimum_frame_interval_milliseconds < 0.0f) {
      minimum_frame_interval_milliseconds =
          kCobaltMinimumFrameTimeInMilliseconds;
    }
#endif
    DCHECK(minimum_frame_interval_milliseconds > 0.0f);
    rasterize_timer_.emplace(
        FROM_HERE,
        base::TimeDelta::FromMillisecondsD(minimum_frame_interval_milliseconds),
        base::BindRepeating(&Pipeline::RasterizeCurrentTree,
                            base::Unretained(this)));
    rasterize_timer_->Reset();
  }
}

void Pipeline::ClearCurrentRenderTree() {
  DCHECK_CALLED_ON_VALID_THREAD(rasterizer_thread_checker_);
  TRACE_EVENT0("cobalt::renderer", "Pipeline::ClearCurrentRenderTree()");

  ResetSubmissionQueue();
  rasterize_timer_ = base::nullopt;
}

void Pipeline::RasterizeCurrentTree() {
  TRACK_MEMORY_SCOPE("Renderer");
  DCHECK_CALLED_ON_VALID_THREAD(rasterizer_thread_checker_);
  TRACE_EVENT0("cobalt::renderer", "Pipeline::RasterizeCurrentTree()");

  base::TimeTicks start_rasterize_time = base::TimeTicks::Now();
  Submission submission =
      submission_queue_->GetCurrentSubmission(start_rasterize_time);

  bool is_new_render_tree = submission.render_tree != last_render_tree_;
  bool has_render_tree_changed =
      !last_animations_expired_ || is_new_render_tree;
  bool force_rasterize =
      submit_even_if_render_tree_is_unchanged_ || fps_overlay_update_pending_;

  float maximum_frame_interval_milliseconds =
      graphics_context_
          ? graphics_context_->GetMaximumFrameIntervalInMilliseconds()
          : -1.0f;
  if (maximum_frame_interval_milliseconds >= 0.0f) {
    base::TimeDelta max_time_between_rasterize =
        base::TimeDelta::FromMillisecondsD(maximum_frame_interval_milliseconds);
    if (start_rasterize_time - last_rasterize_time_ >
        max_time_between_rasterize) {
      force_rasterize = true;
    }
  }

  // If our render tree hasn't changed from the one that was previously
  // rendered and it's okay on this system to not flip the display buffer
  // frequently, then we can just not do anything here.
  if (force_rasterize || has_render_tree_changed) {
    // Check whether the animations in the render tree that is being rasterized
    // are active.
    render_tree::animations::AnimateNode* animate_node =
        base::polymorphic_downcast<render_tree::animations::AnimateNode*>(
            submission.render_tree.get());

    // Rasterize the last submitted render tree.
    bool did_rasterize = RasterizeSubmissionToRenderTarget(
        submission, render_target_, force_rasterize);
    if (did_rasterize) {
      last_rasterize_time_ = start_rasterize_time;
    }

    bool animations_expired = animate_node->expiry() <= submission.time_offset;
    bool stat_tracked_animations_expired =
        animate_node->depends_on_time_expiry() <= submission.time_offset;

    UpdateRasterizeStats(did_rasterize, stat_tracked_animations_expired,
                         is_new_render_tree, start_rasterize_time,
                         base::TimeTicks::Now());

    last_did_rasterize_ = did_rasterize;
    last_animations_expired_ = animations_expired;
    last_stat_tracked_animations_expired_ = stat_tracked_animations_expired;
  }

  if (time_fence_ && submission_queue_->submission_time(
                         base::TimeTicks::Now()) >= *time_fence_) {
    // A time fence was active and we just crossed it, so reset it.
    time_fence_ = base::nullopt;

    if (post_fence_submission_) {
      // A submission was waiting to be queued once we passed the time fence,
      // so go ahead and queue it now.
      QueueSubmission(*post_fence_submission_, *post_fence_receipt_time_);
      post_fence_submission_ = base::nullopt;
      post_fence_receipt_time_ = base::nullopt;
    }
  }
}

void Pipeline::UpdateRasterizeStats(bool did_rasterize,
                                    bool are_stat_tracked_animations_expired,
                                    bool is_new_render_tree,
                                    base::TimeTicks start_time,
                                    base::TimeTicks end_time) {
  bool last_animations_active =
      !last_stat_tracked_animations_expired_ && last_did_rasterize_;
  bool animations_active =
      !are_stat_tracked_animations_expired && did_rasterize;

  // Rasterizations are only tracked by the timers when there are animations.
  // This applies when animations were active during either the last
  // rasterization or the current one. The reason for including the last one is
  // that if animations have just expired, then this rasterization produces the
  // final state of the animated tree.
  if (last_animations_active || animations_active) {
    if (did_rasterize) {
      rasterize_animations_timer_.Start(start_time);
      rasterize_animations_timer_.Stop(end_time);

      // The interval timers require animations to have been active the previous
      // frame so that there is a full interval to record.
      if (last_animations_active) {
        rasterize_periodic_interval_timer_.Stop(end_time);
        rasterize_animations_interval_timer_.Stop(end_time);
      }
    } else {
      // If we didn't actually rasterize anything, then don't count this sample.
      rasterize_periodic_interval_timer_.Cancel();
      rasterize_animations_interval_timer_.Cancel();
    }

    // If animations are active, then they are guaranteed at least one more
    // interval. Start the timer to record its duration.
    if (animations_active) {
      rasterize_periodic_interval_timer_.Start(end_time);
      rasterize_animations_interval_timer_.Start(end_time);
    }

    // Check for if the animations are starting or ending.
    if (!last_animations_active && animations_active) {
      animations_start_time_ = end_time.ToInternalValue();
      has_active_animations_c_val_ = true;
    } else if (last_animations_active && !animations_active) {
      animations_end_time_ = end_time.ToInternalValue();
      rasterize_animations_interval_timer_.Flush();
      rasterize_animations_timer_.Flush();
      has_active_animations_c_val_ = false;
    }
  }

  if (is_new_render_tree) {
    ++new_render_tree_rasterize_count_;
    new_render_tree_rasterize_time_ = end_time.ToInternalValue();
  }

  fallback_rasterize_count_ = rasterizer_->GetFallbackRasterizeCount();
}

bool Pipeline::RasterizeSubmissionToRenderTarget(
    const Submission& submission,
    const scoped_refptr<backend::RenderTarget>& render_target,
    bool force_rasterize) {
  TRACE_EVENT0("cobalt::renderer",
               "Pipeline::RasterizeSubmissionToRenderTarget()");

  // Keep track of the last render tree that we rendered so that we can watch
  // if it changes, in which case we should reset our tracked
  // |previous_animated_area_|.
  if (submission.render_tree != last_render_tree_) {
    last_render_tree_ = submission.render_tree;
    last_animated_render_tree_ = NULL;
    previous_animated_area_ = base::nullopt;
    last_render_time_ = base::nullopt;
  }

  // Animate the render tree using the submitted animations.
  render_tree::animations::AnimateNode* animate_node =
      last_animated_render_tree_
          ? last_animated_render_tree_.get()
          : base::polymorphic_downcast<render_tree::animations::AnimateNode*>(
                submission.render_tree.get());

  // Some animations require a GL graphics context to be current.  Specifically,
  // a call to SbPlayerGetCurrentFrame() may be made to get the current video
  // frame to drive a video-as-an-animated-image.
  rasterizer::Rasterizer::ScopedMakeCurrent scoped_make_current(
      rasterizer_.get());

  render_tree::animations::AnimateNode::AnimateResults results =
      animate_node->Apply(submission.time_offset);

  if (results.animated == last_animated_render_tree_ && !force_rasterize) {
    return false;
  }
  last_animated_render_tree_ = results.animated;

  // Calculate a bounding box around the active animations.  Union it with the
  // bounding box around active animations from the previous frame, and we get
  // a scissor rectangle marking the dirty regions of the screen.
  math::RectF animated_bounds = results.get_animation_bounds_since.Run(
      last_render_time_ ? *last_render_time_ : base::TimeDelta());
  math::Rect rounded_bounds = math::RoundOut(animated_bounds);
  base::Optional<math::Rect> redraw_area;
  if (previous_animated_area_) {
    redraw_area = math::UnionRects(rounded_bounds, *previous_animated_area_);
  }
  previous_animated_area_ = rounded_bounds;

  scoped_refptr<render_tree::Node> submit_tree = results.animated->source();
  if (enable_fps_overlay_ && fps_overlay_) {
    submit_tree = fps_overlay_->AnnotateRenderTreeWithOverlay(
        results.animated->source().get());
    fps_overlay_update_pending_ = false;
  }

  // Rasterize the animated render tree.
  rasterizer::Rasterizer::Options rasterizer_options;
  rasterizer_options.dirty = redraw_area;
  rasterizer_->Submit(submit_tree, render_target, rasterizer_options);

  // Run all of this submission's callbacks.
  for (const auto& callback : submission.on_rasterized_callbacks) {
    callback.Run();
  }

  last_render_time_ = submission.time_offset;

  return true;
}

void Pipeline::InitializeRasterizerThread(
    const CreateRasterizerFunction& create_rasterizer_function) {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::InitializeRasterizerThread");
  DCHECK_CALLED_ON_VALID_THREAD(rasterizer_thread_checker_);
  rasterizer_ = create_rasterizer_function.Run();
  rasterizer_created_event_.Signal();

  // Async load additional fonts after rasterizer thread is fully initialized.
  GetResourceProvider()->LoadAdditionalFonts();

  // Note that this is setup as high priority, but lower than the rasterizer
  // thread's priority (ThreadPriority::HIGHEST).  This is to ensure that
  // we never interrupt the rasterizer in order to dispose render trees, but
  // at the same time we do want to prioritize cleaning them up to avoid
  // large queues of pending render tree disposals.
  base::Thread::Options options(base::MessageLoop::TYPE_DEFAULT,
                                kRendererThreadStackSize);
  options.priority = base::ThreadPriority::HIGHEST;
  submission_disposal_thread_.StartWithOptions(options);

  ResetSubmissionQueue();
}

void Pipeline::ShutdownSubmissionQueue() {
  TRACE_EVENT0("cobalt::renderer", "Pipeline::ShutdownSubmissionQueue()");
  DCHECK_CALLED_ON_VALID_THREAD(rasterizer_thread_checker_);

  // Clear out our time fence data, especially |post_fence_submission_| which
  // may refer to a render tree.
  time_fence_ = base::nullopt;
  post_fence_submission_ = base::nullopt;
  post_fence_receipt_time_ = base::nullopt;

  // Stop and shutdown the rasterizer timer.  If we won't have a submission
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
  DCHECK_CALLED_ON_VALID_THREAD(rasterizer_thread_checker_);

  // Shutdown the FPS overlay which may reference render trees.
  fps_overlay_ = base::nullopt;

  // Submit a fullscreen rect node to clear the display before shutting
  // down.  This can be helpful if we quit while playing a video via
  // punch-through, which may result in unexpected images/colors appearing for
  // a flicker behind the display.
  render_tree::ColorRGBA clear_color;
  if (render_target_ && clear_on_shutdown_mode_ == kClearAccordingToPlatform &&
      ShouldClearFrameOnShutdown(&clear_color)) {
    rasterizer_->Submit(
        new render_tree::ClearRectNode(math::RectF(render_target_->GetSize()),
                                       clear_color),
        render_target_);
  }

  // This potential reference to a render tree whose animations may have ended
  // must be destroyed before we shutdown the rasterizer thread since it may
  // contain references to render tree nodes and resources.
  last_render_tree_ = NULL;
  last_animated_render_tree_ = NULL;
}

#if defined(ENABLE_DEBUGGER)
void Pipeline::OnDumpCurrentRenderTree(const std::string& message) {
  if (base::MessageLoop::current() != rasterizer_thread_.message_loop()) {
    rasterizer_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&Pipeline::OnDumpCurrentRenderTree,
                              base::Unretained(this), message));
    return;
  }

  if (!rasterize_timer_) {
    LOG(INFO) << "No render tree available yet.";
    return;
  }

  // Grab the most recent submission, animate it, and then dump the results to
  // text.
  Submission submission =
      submission_queue_->GetCurrentSubmission(base::TimeTicks::Now());

  render_tree::animations::AnimateNode* animate_node =
      base::polymorphic_downcast<render_tree::animations::AnimateNode*>(
          submission.render_tree.get());
  render_tree::animations::AnimateNode::AnimateResults results =
      animate_node->Apply(submission.time_offset);

  std::string tree_dump =
      render_tree::DumpRenderTreeToString(results.animated->source().get());
  if (message.empty() || message == "undefined") {
    // If no filename was specified, send output to the console.
    LOG(INFO) << tree_dump.c_str();
  } else {
    // If a filename was specified, dump the output to that file.
    base::FilePath out_dir;
    base::PathService::Get(paths::DIR_COBALT_DEBUG_OUT, &out_dir);

    base::WriteFile(out_dir.Append(message), tree_dump.c_str(),
                    tree_dump.length());
  }
}

void Pipeline::OnToggleFpsStdout(const std::string& message) {
  if (base::MessageLoop::current() != rasterizer_thread_.message_loop()) {
    rasterizer_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&Pipeline::OnToggleFpsStdout,
                              base::Unretained(this), message));
    return;
  }

  enable_fps_stdout_ = !enable_fps_stdout_;
}

void Pipeline::OnToggleFpsOverlay(const std::string& message) {
  if (base::MessageLoop::current() != rasterizer_thread_.message_loop()) {
    rasterizer_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&Pipeline::OnToggleFpsOverlay,
                              base::Unretained(this), message));
    return;
  }

  enable_fps_overlay_ = !enable_fps_overlay_;
  fps_overlay_update_pending_ = enable_fps_overlay_;
}
#endif  // #if defined(ENABLE_DEBUGGER)

Submission Pipeline::CollectAnimations(
    const Submission& render_tree_submission) {
  // Constructing an AnimateNode will result in the tree being traversed to
  // collect all sub-AnimateNodes into the new one, in order to maintain the
  // invariant that a sub-tree of an AnimateNode has no AnimateNodes.
  Submission collected_submission = render_tree_submission;
  collected_submission.render_tree = new render_tree::animations::AnimateNode(
      render_tree_submission.render_tree);
  return collected_submission;
}

namespace {
void PrintFPS(const base::CValCollectionTimerStatsFlushResults& results) {
  SbLogRaw(base::StringPrintf("FPS => # samples: %d, avg: %.1fms, "
                              "[min, max]: [%.1fms, %.1fms]\n"
                              "       25th : 50th : 75th : 95th pct - "
                              "%.1fms : %.1fms : %.1fms : %.1fms\n",
                              static_cast<unsigned int>(results.sample_count),
                              results.average.InMillisecondsF(),
                              results.minimum.InMillisecondsF(),
                              results.maximum.InMillisecondsF(),
                              results.percentile_25th.InMillisecondsF(),
                              results.percentile_50th.InMillisecondsF(),
                              results.percentile_75th.InMillisecondsF(),
                              results.percentile_95th.InMillisecondsF())
               .c_str());
}
}  // namespace

void Pipeline::FrameStatsOnFlushCallback(
    const base::CValCollectionTimerStatsFlushResults& flush_results) {
  DCHECK_CALLED_ON_VALID_THREAD(rasterizer_thread_checker_);

  if (enable_fps_overlay_) {
    if (!fps_overlay_) {
      fps_overlay_.emplace(rasterizer_->GetResourceProvider());
    }

    fps_overlay_->UpdateOverlay(flush_results);
    fps_overlay_update_pending_ = true;
  }

  if (enable_fps_stdout_) {
    PrintFPS(flush_results);
  }
}

void Pipeline::ResetSubmissionQueue() {
  TRACK_MEMORY_SCOPE("Renderer");
  TRACE_EVENT0("cobalt::renderer", "Pipeline::ResetSubmissionQueue()");
  submission_queue_ = base::nullopt;
  submission_queue_.emplace(
      current_timeline_info_.max_submission_queue_size,
      base::TimeDelta::FromMillisecondsD(kTimeToConvergeInMS),
      current_timeline_info_.allow_latency_reduction,
      base::Bind(&DestructSubmissionOnMessageLoop,
                 submission_disposal_thread_.message_loop()));
}

void Pipeline::QueueSubmission(const Submission& submission,
                               base::TimeTicks receipt_time) {
  TRACK_MEMORY_SCOPE("Renderer");
  TRACE_EVENT0("cobalt::renderer", "Pipeline::QueueSubmission()");
  // Upon each submission, check if the timeline has changed.  If it has,
  // reset our submission queue (possibly with a new configuration specified
  // within |timeline_info|.
  if (submission.timeline_info.id != current_timeline_info_.id) {
    current_timeline_info_ = submission.timeline_info;
    ResetSubmissionQueue();
  }

  submission_queue_->PushSubmission(submission, receipt_time);
}

}  // namespace renderer
}  // namespace cobalt
