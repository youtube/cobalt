// Copyright 2014 Google Inc. All Rights Reserved.
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
#include "cobalt/base/c_val_collection_timer_stats.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/fps_overlay.h"
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

  enum ShutdownClearMode {
    kClearToBlack,
    kNoClear,
  };

  struct Options {
    Options() : enable_fps_stdout(false), enable_fps_overlay(false) {}

    bool enable_fps_stdout;
    bool enable_fps_overlay;
  };

  // Using the provided rasterizer creation function, a rasterizer will be
  // created within the Pipeline on a separate rasterizer thread.  Thus,
  // the rasterizer created by the provided function should only reference
  // thread safe objects.  If |clear_to_black_on_shutdown| is specified,
  // the provided render_target_ (if not NULL) will be cleared to black when
  // the pipeline is destroyed.
  Pipeline(const CreateRasterizerFunction& create_rasterizer_function,
           const scoped_refptr<backend::RenderTarget>& render_target,
           backend::GraphicsContext* graphics_context,
           bool submit_even_if_render_tree_is_unchanged,
           ShutdownClearMode clear_on_shutdown_mode,
           const Options& options = Options());
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

  // Inserts a fence that ensures the rasterizer rasterizes up until the
  // submission time proceeding queuing additional submissions.  This is useful
  // when switching timelines in order to ensure that an old timeline plays out
  // completely before resetting the submission queue for a timeline change.
  // Upon passing the fence, we will immediately queue the latest submission
  // submitted after TimeFence() was called.  If a time fence is set while
  // an existing time fence already exists, the new time fence is ignored (and
  // an error is logged).
  void TimeFence(base::TimeDelta time_fence);

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
  void ClearCurrentRenderTree();

  // Called repeatedly (the rate is limited by the rasterizer, so likely it
  // will be called every 1/60th of a second) on the rasterizer thread and
  // results in the rasterization of the current tree and submission of it to
  // the render target.
  void RasterizeCurrentTree();

  // Rasterize the animated |render_tree_submission| to |render_target|,
  // applying the time_offset in the submission to the animations.
  // Returns true only if a rasterization actually took place.
  bool RasterizeSubmissionToRenderTarget(
      const Submission& render_tree_submission,
      const scoped_refptr<backend::RenderTarget>& render_target);

  // Updates the rasterizer timer stats according to the |start_time| and
  // |end_time| of the most recent rasterize call.
  void UpdateRasterizeStats(bool did_rasterize,
                            bool are_stat_tracked_animations_expired,
                            bool is_new_render_tree, base::TimeTicks start_time,
                            base::TimeTicks end_time);

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
  void OnToggleFpsStdout(const std::string&);
  void OnToggleFpsOverlay(const std::string&);
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  // Render trees may contain a number of AnimateNodes (or none).  In order
  // to optimize for applying the animations on the rasterizer thread, this
  // function searches the tree for AnimateNodes and collects all of their
  // information into a single AnimateNode at the root of the returned
  // render tree.
  Submission CollectAnimations(const Submission& render_tree_submission);

  void FrameStatsOnFlushCallback(
      const base::CValCollectionTimerStatsFlushResults& flush_results);

  // Resets the submission queue, effecitvely emptying it and restarting it
  // with the configuration specified by |current_timeline_info_| applied to it.
  void ResetSubmissionQueue();

  // Pushes the specified submission into the submission queue, where it will
  // then be picked up by subsequent rasterizations.  If the submission's
  // timeline id is different from the current timeline id (in
  // |current_timeline_info_|), then the submission queue will be reset.
  void QueueSubmission(const Submission& submission,
                       base::TimeTicks receipt_time);

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

  // If true, we will submit the current render tree to the rasterizer every
  // frame, even if it hasn't changed.
  const bool submit_even_if_render_tree_is_unchanged_;

  // Keeps track of the last rendered animated render tree.
  scoped_refptr<render_tree::Node> last_render_tree_;

  scoped_refptr<render_tree::animations::AnimateNode>
      last_animated_render_tree_;

  // Keeps track of the area of the screen that animations previously existed
  // within, so that we can know which regions of the screens would be dirty
  // next frame.
  base::optional<math::Rect> previous_animated_area_;
  // The submission time used during the last render tree render.
  base::optional<base::TimeDelta> last_render_time_;
  // Keep track of whether the last rendered tree had active animations. This
  // allows us to skip rasterizing that render tree if we see it again and it
  // did have expired animations.
  bool last_animations_expired_;
  // Keep track of whether the last rendered tree had animations that we're
  // tracking stats on.
  bool last_stat_tracked_animations_expired_;

  // Did a rasterization take place in the last frame?
  bool last_did_rasterize_;

  // Timer tracking the amount of time spent in
  // |RasterizeSubmissionToRenderTarget| while animations are active. The
  // tracking is flushed when the animations expire.
  base::CValCollectionTimerStats<base::CValPublic> rasterize_animations_timer_;

  // Accumulates render tree rasterization interval times but does not flush
  // them until the maximum number of samples is gathered.
  base::CValCollectionTimerStats<base::CValPublic>
      rasterize_periodic_interval_timer_;

  // Timer tracking the amount of time between calls to
  // |RasterizeSubmissionToRenderTarget| while animations are active. The
  // tracking is flushed when the animations expire.
  base::CValCollectionTimerStats<base::CValPublic>
      rasterize_animations_interval_timer_;

  // The total number of new render trees that have been rasterized.
  base::CVal<int, base::CValPublic> new_render_tree_rasterize_count_;
  // The last time that a newly encountered render tree was first rasterized.
  base::CVal<int64, base::CValPublic> new_render_tree_rasterize_time_;

  // Whether or not animations are currently playing.
  base::CVal<bool, base::CValPublic> has_active_animations_c_val_;
  // The most recent time animations started playing.
  base::CVal<int64, base::CValPublic> animations_start_time_;
  // The most recent time animations ended playing.
  base::CVal<int64, base::CValPublic> animations_end_time_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Dumps the current render tree to the console.
  base::ConsoleCommandManager::CommandHandler
      dump_current_render_tree_command_handler_;

  base::ConsoleCommandManager::CommandHandler
      toggle_fps_stdout_command_handler_;
  base::ConsoleCommandManager::CommandHandler
      toggle_fps_overlay_command_handler_;
#endif

  // If true, Pipeline's destructor will clear its render target to black on
  // shutdown.
  const ShutdownClearMode clear_on_shutdown_mode_;

  // If true, we will print framerate statistics to stdout upon completion
  // of each animation (or after a maximum number of frames has been issued).
  bool enable_fps_stdout_;

  // If true, an overlay will be displayed over the UI output that shows the
  // FPS statistics from the last animation.
  bool enable_fps_overlay_;

  base::optional<FpsOverlay> fps_overlay_;

  // True if the overlay has been updated and it needs to be re-rasterized.
  bool fps_overlay_update_pending_;

  // Time fence data that records if a time fence is active, at what time, and
  // what submission if any is waiting to be queued once we pass the time fence.
  base::optional<base::TimeDelta> time_fence_;
  base::optional<Submission> post_fence_submission_;
  base::optional<base::TimeTicks> post_fence_receipt_time_;

  // Information about the current timeline.  Each incoming submission
  // identifies with a particular timeline, and if that ever changes, we assume
  // a discontinuity in animations and reset our submission queue, possibly
  // with new configuration parameters specified in the new |TimelineInfo|.
  Submission::TimelineInfo current_timeline_info_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_PIPELINE_H_
