// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_presentation_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/egl_timestamps.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gpu_timing.h"

namespace gl {

#if BUILDFLAG(IS_STARBOARD)
namespace {

// Re-poll interval for a GLFence that has not signalled yet. Upstream waits
// for the next (synthetic) VSync tick, i.e. up to 16.7 ms, which lets two
// buffered frames complete before one poll and stamps them together.
constexpr base::TimeDelta kStarboardFencePollInterval = base::Milliseconds(2);

// A fence that has not signalled this long after eglSwapBuffers() returned,
// or a backlog this deep, is treated as a non-signalling driver fence.
constexpr base::TimeDelta kStarboardMaxFenceWait = base::Milliseconds(100);
constexpr size_t kStarboardMaxPendingFrames = 4;

// Depth of the swapchain cushion within which a frame's timestamp may be
// moved forward onto the next VSync slot.
constexpr int kStarboardMaxBufferedFrames = 2;

}  // namespace
#endif  // BUILDFLAG(IS_STARBOARD)

GLSurfacePresentationHelper::ScopedSwapBuffers::ScopedSwapBuffers(
    GLSurfacePresentationHelper* helper,
    GLSurface::PresentationCallback callback)
    : ScopedSwapBuffers(helper, std::move(callback), -1) {}

GLSurfacePresentationHelper::ScopedSwapBuffers::ScopedSwapBuffers(
    GLSurfacePresentationHelper* helper,
    GLSurface::PresentationCallback callback,
    int frame_id)
    : helper_(helper) {
  if (helper_)
    helper_->PreSwapBuffers(std::move(callback), frame_id);
}

GLSurfacePresentationHelper::ScopedSwapBuffers::~ScopedSwapBuffers() {
  if (helper_)
    helper_->PostSwapBuffers(result_);
}

GLSurfacePresentationHelper::Frame::Frame(Frame&& other) = default;

GLSurfacePresentationHelper::Frame::Frame(
    int frame_id,
    GLSurface::PresentationCallback callback)
    : frame_id(frame_id), callback(std::move(callback)) {}

GLSurfacePresentationHelper::Frame::Frame(
    std::unique_ptr<GPUTimer>&& timer,
    GLSurface::PresentationCallback callback)
    : timer(std::move(timer)), callback(std::move(callback)) {}

GLSurfacePresentationHelper::Frame::Frame(
    std::unique_ptr<GLFence>&& fence,
    GLSurface::PresentationCallback callback)
    : fence(std::move(fence)), callback(std::move(callback)) {}

GLSurfacePresentationHelper::Frame::Frame(
    GLSurface::PresentationCallback callback)
    : callback(std::move(callback)) {}

GLSurfacePresentationHelper::Frame::~Frame() = default;

GLSurfacePresentationHelper::Frame& GLSurfacePresentationHelper::Frame::
operator=(Frame&& other) = default;

bool GLSurfacePresentationHelper::GetFrameTimestampInfoIfAvailable(
    const Frame& frame,
    base::TimeTicks* timestamp,
    base::TimeDelta* interval,
    base::TimeTicks* writes_done,
    uint32_t* flags) {
  DCHECK(frame.timer || frame.fence || egl_timestamp_client_);

  if (egl_timestamp_client_) {
    bool result = egl_timestamp_client_->GetFrameTimestampInfoIfAvailable(
        timestamp, interval, writes_done, flags, frame.frame_id);

    // Workaround null timestamp by setting it to TimeTicks::Now() snapped to
    // the next vsync interval. See
    // https://bugs.chromium.org/p/chromium/issues/detail?id=966638 for more
    // details.
    if (result && timestamp->is_null()) {
      *timestamp = base::TimeTicks::Now();
      *interval = vsync_interval_;
      *flags = 0;
      if (!vsync_interval_.is_zero()) {
        *timestamp =
            timestamp->SnappedToNextTick(vsync_timebase_, vsync_interval_);
        *flags = gfx::PresentationFeedback::kVSync;
      }
    }
    return result;
  } else if (frame.timer) {
    if (!frame.timer->IsAvailable())
      return false;
    int64_t start = 0;
    int64_t end = 0;
    frame.timer->GetStartEndTimestamps(&start, &end);
    *timestamp = base::TimeTicks() + base::Microseconds(start);
  } else {
#if BUILDFLAG(IS_STARBOARD)
    // Some embedded drivers never signal fences, or signal them only after a
    // later swap. Do not let one such fence hold back every later frame: after
    // kStarboardMaxFenceWait (or with a deep backlog) complete the frame from
    // the time its swap returned and stop relying on fences.
    if (!frame.fence->HasCompleted()) {
      const base::TimeTicks now = base::TimeTicks::Now();
      const bool hung =
          (!frame.swap_completed_time.is_null() &&
           now - frame.swap_completed_time > kStarboardMaxFenceWait) ||
          pending_frames_.size() > kStarboardMaxPendingFrames;
      if (!hung)
        return false;
      LOG(WARNING) << "GLFence did not signal within " << kStarboardMaxFenceWait
                   << "; falling back to swap completion time.";
      gl_fence_supported_ = false;
      *timestamp =
          frame.swap_completed_time.is_null() ? now : frame.swap_completed_time;
    } else {
      *timestamp = base::TimeTicks::Now();
    }
#else
    if (!frame.fence->HasCompleted())
      return false;
    *timestamp = base::TimeTicks::Now();
#endif
  }
  // Below logic is used to calculate final values of timestamp, interval and
  // flags when using timer/fence to report the timestamps.
  const bool fixed_vsync = !vsync_provider_;
  const bool hw_clock = vsync_provider_ && vsync_provider_->IsHWClock();
  *interval = vsync_interval_;
  *flags = 0;
  if (vsync_interval_.is_zero() || fixed_vsync) {
#if BUILDFLAG(IS_STARBOARD)
    // No VSync source at all (e.g. Mali on RDK: no EGL_CHROMIUM_sync_control,
    // no EGL_ANDROID_get_frame_timestamps, GPU timer skipped above). The raw
    // fence time measures GPU completion, not scan-out; space consecutive
    // frames by whole VSync slots so presentation feedback reflects the
    // swapchain's FIFO cadence.
    ApplyFifoVSyncSlot(timestamp, interval, flags);
#endif
    // If VSync parameters are fixed or not available, we just run
    // presentation callbacks with timestamp from GPUTimers.
    return true;
  } else if (*timestamp < vsync_timebase_) {
    // We got a VSync whose timestamp is after GPU finished rendering this
    // back buffer.
    *flags = gfx::PresentationFeedback::kVSync |
             gfx::PresentationFeedback::kHWCompletion;
    auto delta = vsync_timebase_ - *timestamp;
    if (delta < vsync_interval_) {
      // The |vsync_timebase_| is the closest VSync's timestamp after the GPU
      // finished rendering.
      *timestamp = vsync_timebase_;
      if (hw_clock)
        *flags |= gfx::PresentationFeedback::kHWClock;
    } else {
      // The |vsync_timebase_| isn't the closest VSync's timestamp after the
      // GPU finished rendering. We have to compute the closest VSync's
      // timestmp.
      *timestamp =
          timestamp->SnappedToNextTick(vsync_timebase_, vsync_interval_);
    }
  } else {
    // The |vsync_timebase_| is earlier than |timestamp|, we will compute the
    // next vSync's timestamp and use it to run callback.
    if (!vsync_interval_.is_zero()) {
      *timestamp =
          timestamp->SnappedToNextTick(vsync_timebase_, vsync_interval_);
      *flags = gfx::PresentationFeedback::kVSync;
    }
  }
  return true;
}

#if BUILDFLAG(IS_STARBOARD)
void GLSurfacePresentationHelper::ApplyFifoVSyncSlot(base::TimeTicks* timestamp,
                                                     base::TimeDelta* interval,
                                                     uint32_t* flags) {
  const base::TimeDelta slot =
      vsync_interval_.is_zero() ? base::Seconds(1) / 60 : vsync_interval_;
  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks ts = *timestamp;

  if (!last_presented_timestamp_.is_null()) {
    const base::TimeTicks next_slot = last_presented_timestamp_ + slot;
    // Only frames that completed within the swapchain cushion of the previous
    // one are candidates: anything older is a genuine late frame (or the
    // first frame after an idle period) and keeps its own time.
    if (ts < next_slot &&
        next_slot - ts <= slot * kStarboardMaxBufferedFrames) {
      ts = next_slot;
      *flags |= gfx::PresentationFeedback::kVSync;
    }
  }
  // viz::Display::SanitizePresentationFeedback() rejects timestamps in the
  // future; never hand one out.
  if (ts > now)
    ts = now;

  *timestamp = ts;
  *interval = slot;
  last_presented_timestamp_ = ts;
}
#endif  // BUILDFLAG(IS_STARBOARD)

void GLSurfacePresentationHelper::Frame::Destroy(bool has_context) {
  if (timer) {
    timer->Destroy(has_context);
  } else if (fence) {
    if (has_context)
      fence = nullptr;
    else
      fence->Invalidate();
  }
  std::move(callback).Run(gfx::PresentationFeedback::Failure());
}

GLSurfacePresentationHelper::GLSurfacePresentationHelper(
    gfx::VSyncProvider* vsync_provider)
    : vsync_provider_(vsync_provider) {}

GLSurfacePresentationHelper::GLSurfacePresentationHelper(
    base::TimeTicks timebase,
    base::TimeDelta interval)
    : vsync_provider_(nullptr),
      vsync_timebase_(timebase),
      vsync_interval_(interval) {}

GLSurfacePresentationHelper::~GLSurfacePresentationHelper() {
  // Discard pending frames and run presentation callback with empty
  // PresentationFeedback.
  bool has_context = gl_context_ && gl_context_->IsCurrent(surface_);
  for (auto& frame : pending_frames_) {
    frame.Destroy(has_context);
  }
  pending_frames_.clear();
}

void GLSurfacePresentationHelper::OnMakeCurrent(GLContext* context,
                                                GLSurface* surface) {
  DCHECK(context);
  DCHECK(surface);
  DCHECK(surface == surface_ || !surface_);
  if (context == gl_context_)
    return;

  surface_ = surface;
  // If context is changed, we assume SwapBuffers issued for previous context
  // will be discarded.
  if (gpu_timing_client_)
    gpu_timing_client_ = nullptr;
  for (auto& frame : pending_frames_) {
    frame.Destroy();
  }
  pending_frames_.clear();

  gl_context_ = context;

  // Get an egl timestamp client.
  egl_timestamp_client_ = surface_->GetEGLTimestampClient();

  // If there is an egl timestamp client, check if egl timestamps are supported
  // or not. If supported, then return as there is no need to use gpu timestamp
  // client or fence.
  if (egl_timestamp_client_) {
    if (egl_timestamp_client_->IsEGLTimestampSupported())
      return;
    else
      egl_timestamp_client_ = nullptr;
  }

#if BUILDFLAG(IS_STARBOARD)
  // Some embedded GL drivers (e.g. Mali on RDK reference boxes) advertise
  // GL_EXT_disjoint_timer_query but report GL_TIMESTAMP_EXT in raw GPU
  // counter ticks (24 MHz on RDK) instead of nanoseconds. GPU time as seen by
  // GPUTimingImpl then advances ~40x too slowly, the resulting
  // presentation timestamps fail Display::SanitizePresentationFeedback()'s
  // before-draw / in-the-future checks, and every frame is reported as
  // PresentationFeedback::Failure(). Skip the GPU timer and fall through to
  // the GLFence / VSync-snapped paths, which produce valid feedback on these
  // devices.
  gpu_timing_client_ = nullptr;
#else
  gpu_timing_client_ = context->CreateGPUTimingClient();
  if (!gpu_timing_client_->IsAvailable())
    gpu_timing_client_ = nullptr;
#endif

// https://crbug.com/854298 : disable GLFence on Android as they seem to cause
// issues on some devices.
#if !BUILDFLAG(IS_ANDROID)
  gl_fence_supported_ = GLFence::IsSupported();
#endif
}

void GLSurfacePresentationHelper::PreSwapBuffers(
    GLSurface::PresentationCallback callback,
    int frame_id) {
  if (egl_timestamp_client_) {
    pending_frames_.emplace_back(frame_id, std::move(callback));
  } else if (gpu_timing_client_) {
    std::unique_ptr<GPUTimer> timer;
    timer = gpu_timing_client_->CreateGPUTimer(false /* prefer_elapsed_time */);
    timer->QueryTimeStamp();
    pending_frames_.push_back(Frame(std::move(timer), std::move(callback)));
  } else if (gl_fence_supported_) {
    auto fence = GLFence::Create();
    pending_frames_.push_back(Frame(std::move(fence), std::move(callback)));
  } else {
    pending_frames_.push_back(Frame(std::move(callback)));
  }
}

void GLSurfacePresentationHelper::PostSwapBuffers(gfx::SwapResult result) {
  DCHECK(!pending_frames_.empty());
  auto& frame = pending_frames_.back();
  frame.result = result;
#if BUILDFLAG(IS_STARBOARD)
  frame.swap_completed_time = base::TimeTicks::Now();
#endif
  ScheduleCheckPendingFrames(false /* align_with_next_vsync */);
}

void GLSurfacePresentationHelper::CheckPendingFrames() {
  DCHECK(gl_context_ || pending_frames_.empty());

  if (vsync_provider_ &&
      vsync_provider_->SupportGetVSyncParametersIfAvailable()) {
    if (!vsync_provider_->GetVSyncParametersIfAvailable(&vsync_timebase_,
                                                        &vsync_interval_)) {
      vsync_timebase_ = base::TimeTicks();
      vsync_interval_ = base::TimeDelta();
      static unsigned int count = 0;
      // GetVSyncParametersIfAvailable() could be called and failed frequently,
      // so we have to limit the LOG to avoid flooding the log.
      LOG_IF(ERROR, ++count < 4 || !(count & 0xffff))
          << "GetVSyncParametersIfAvailable() failed for " << count
          << " times!";
    }
  }

  if (pending_frames_.empty())
    return;

  if (!gl_context_->MakeCurrent(surface_)) {
    gl_context_ = nullptr;
    egl_timestamp_client_ = nullptr;
    gpu_timing_client_ = nullptr;
    for (auto& frame : pending_frames_)
      frame.Destroy();
    pending_frames_.clear();
    return;
  }

  bool disjoint_occurred =
      gpu_timing_client_ && gpu_timing_client_->CheckAndResetTimerErrors();
  if (disjoint_occurred ||
      (!egl_timestamp_client_ && !gpu_timing_client_ && !gl_fence_supported_)) {
    // If EGLTimestamps, GPUTimer and GLFence are not available or disjoint
    // occurred, we will compute the next VSync's timestamp and use it to run
    // presentation callback.
    uint32_t flags = 0;
    auto timestamp = base::TimeTicks::Now();
    if (!vsync_interval_.is_zero()) {
      timestamp = timestamp.SnappedToNextTick(vsync_timebase_, vsync_interval_);
      flags = gfx::PresentationFeedback::kVSync;
    }
    gfx::PresentationFeedback feedback(timestamp, vsync_interval_, flags);
    for (auto& frame : pending_frames_) {
      if (frame.timer)
        frame.timer->Destroy(true /* has_context */);
      if (frame.result == gfx::SwapResult::SWAP_ACK) {
#if BUILDFLAG(IS_STARBOARD)
        if (!vsync_provider_) {
          // No fence and no VSync source: the best per-frame signal is when
          // its swap returned. Space frames by VSync slots as in the fence
          // path so the feedback stays a FIFO cadence rather than a burst.
          base::TimeTicks ts = frame.swap_completed_time.is_null()
                                   ? timestamp
                                   : frame.swap_completed_time;
          base::TimeDelta slot_interval;
          uint32_t slot_flags = 0;
          ApplyFifoVSyncSlot(&ts, &slot_interval, &slot_flags);
          std::move(frame.callback)
              .Run(gfx::PresentationFeedback(ts, slot_interval, slot_flags));
          continue;
        }
#endif
        std::move(frame.callback).Run(feedback);
      } else {
        std::move(frame.callback).Run(gfx::PresentationFeedback::Failure());
      }
    }
    pending_frames_.clear();
  }

  while (!pending_frames_.empty()) {
    auto& frame = pending_frames_.front();
    // Helper lambda for running the presentation callback and releasing the
    // frame.
    auto frame_presentation_callback =
        [this, &frame](const gfx::PresentationFeedback& feedback) {
          if (frame.timer)
            frame.timer->Destroy(true /* has_context */);
          std::move(frame.callback).Run(feedback);
          pending_frames_.pop_front();
        };

    if (frame.result != gfx::SwapResult::SWAP_ACK) {
      frame_presentation_callback(gfx::PresentationFeedback::Failure());
      continue;
    }

    base::TimeTicks timestamp;
    base::TimeDelta interval;
    base::TimeTicks writes_done;
    uint32_t flags = 0;
    // Get timestamp info for a frame if available. If timestamp is not
    // available, it means this frame is not yet done.
    if (!GetFrameTimestampInfoIfAvailable(frame, &timestamp, &interval,
                                          &writes_done, &flags))
      break;

    gfx::PresentationFeedback feedback(timestamp, interval, flags);
    feedback.writes_done_timestamp = writes_done;
    frame_presentation_callback(feedback);
  }

  if (!pending_frames_.empty())
    ScheduleCheckPendingFrames(true /* align_with_next_vsync */);
}

void GLSurfacePresentationHelper::CheckPendingFramesCallback() {
  DCHECK(check_pending_frame_scheduled_);
  check_pending_frame_scheduled_ = false;
  CheckPendingFrames();
}

void GLSurfacePresentationHelper::UpdateVSyncCallback(
    bool should_check_pending_frames,
    const base::TimeTicks timebase,
    const base::TimeDelta interval) {
  DCHECK(update_vsync_pending_);
  update_vsync_pending_ = false;
  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
  if (should_check_pending_frames) {
    DCHECK(check_pending_frame_scheduled_);
    check_pending_frame_scheduled_ = false;
    CheckPendingFrames();
  }
}

void GLSurfacePresentationHelper::ScheduleCheckPendingFrames(
    bool align_with_next_vsync) {
  // Always GetVSyncParameters to minimize clock-skew in vsync_timebase_.
  bool vsync_provider_schedules_check = false;
  if (vsync_provider_ &&
      !vsync_provider_->SupportGetVSyncParametersIfAvailable() &&
      !update_vsync_pending_) {
    update_vsync_pending_ = true;
    vsync_provider_schedules_check =
        !check_pending_frame_scheduled_ && !align_with_next_vsync;
    vsync_provider_->GetVSyncParameters(base::BindOnce(
        &GLSurfacePresentationHelper::UpdateVSyncCallback,
        weak_ptr_factory_.GetWeakPtr(), vsync_provider_schedules_check));
  }

  if (check_pending_frame_scheduled_)
    return;
  check_pending_frame_scheduled_ = true;

  if (vsync_provider_schedules_check)
    return;

  if (!align_with_next_vsync) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&GLSurfacePresentationHelper::CheckPendingFramesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If the |vsync_provider_| can not notify us for the next VSync
  // asynchronically, we have to compute the next VSync time and post a delayed
  // task so we can check the VSync later.
#if BUILDFLAG(IS_STARBOARD)
  if (!vsync_provider_ && gl_fence_supported_) {
    // Without a VSync source the "next VSync" below is a synthetic 16.7 ms
    // grid unrelated to the display. Waiting for it lets two buffered frames
    // complete before one poll and stamps them together; poll the fence
    // promptly instead. The check is cheap and only runs while a frame is
    // pending.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GLSurfacePresentationHelper::CheckPendingFramesCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        kStarboardFencePollInterval);
    return;
  }
#endif
  base::TimeDelta interval =
      vsync_interval_.is_zero() ? base::Seconds(1) / 60 : vsync_interval_;
  auto now = base::TimeTicks::Now();
  auto next_vsync = now.SnappedToNextTick(vsync_timebase_, interval);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GLSurfacePresentationHelper::CheckPendingFramesCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      next_vsync - now);
}

}  // namespace gl
