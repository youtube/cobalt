// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"

namespace media {

// This equates to ~16.67 fps, which is just slow enough to be tolerable when
// our video renderer is ahead of the audio playback.
//
// A higher value will be a slower frame rate, which looks worse but allows the
// audio renderer to catch up faster.  A lower value will be a smoother frame
// rate, but results in the video being out of sync for longer.
static const int64 kMaxSleepMilliseconds = 60;

// The number of milliseconds to idle when we do not have anything to do.
// Nothing special about the value, other than we're being more OS-friendly
// than sleeping for 1 millisecond.
static const int kIdleMilliseconds = 10;

VideoRendererBase::VideoRendererBase()
    : frame_available_(&lock_),
      state_(kUninitialized),
      thread_(base::kNullThreadHandle),
      pending_read_(false),
      pending_paint_(false),
      pending_paint_with_last_available_(false),
      playback_rate_(0),
      read_cb_(base::Bind(&VideoRendererBase::FrameReady,
                          base::Unretained(this))) {
}

VideoRendererBase::~VideoRendererBase() {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kUninitialized || state_ == kStopped) << state_;
}

void VideoRendererBase::Play(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPrerolled, state_);
  state_ = kPlaying;
  callback.Run();
}

void VideoRendererBase::Pause(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized || state_ == kError);
  state_ = kPaused;
  callback.Run();
}

void VideoRendererBase::Flush(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPaused);
  flush_callback_ = callback;
  state_ = kFlushing;

  AttemptFlush_Locked();
}

void VideoRendererBase::Stop(const base::Closure& callback) {
  base::PlatformThreadHandle thread_to_join = base::kNullThreadHandle;
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;

    if (!pending_paint_ && !pending_paint_with_last_available_)
      DoStopOrError_Locked();

    // Clean up our thread if present.
    if (thread_ != base::kNullThreadHandle) {
      // Signal the thread since it's possible to get stopped with the video
      // thread waiting for a read to complete.
      frame_available_.Signal();
      thread_to_join = thread_;
      thread_ = base::kNullThreadHandle;
    }
  }
  if (thread_to_join != base::kNullThreadHandle)
    base::PlatformThread::Join(thread_to_join);

  // Signal the subclass we're stopping.
  OnStop(callback);
}

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(state_, kFlushed) << "Must flush prior to seeking.";
    DCHECK(!cb.is_null());
    DCHECK(seek_cb_.is_null());

    state_ = kSeeking;
    seek_cb_ = cb;
    seek_timestamp_ = time;
    AttemptRead_Locked();
  }
}

void VideoRendererBase::Initialize(VideoDecoder* decoder,
                                   const base::Closure& callback,
                                   const StatisticsCallback& stats_callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK(decoder);
  DCHECK(!callback.is_null());
  DCHECK(!stats_callback.is_null());
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;

  statistics_callback_ = stats_callback;

  // Notify the pipeline of the video dimensions.
  host()->SetNaturalVideoSize(decoder_->natural_size());

  // Initialize the subclass.
  // TODO(scherkus): do we trust subclasses not to do something silly while
  // we're holding the lock?
  if (!OnInitialize(decoder)) {
    state_ = kError;
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback.Run();
    return;
  }

  // We're all good!  Consider ourselves flushed. (ThreadMain() should never
  // see us in the kUninitialized state).
  // Since we had an initial Seek, we consider ourself flushed, because we
  // have not populated any buffers yet.
  state_ = kFlushed;

  // Create our video thread.
  if (!base::PlatformThread::Create(0, this, &thread_)) {
    NOTREACHED() << "Video thread creation failed";
    state_ = kError;
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback.Run();
    return;
  }

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)
  callback.Run();
}

bool VideoRendererBase::HasEnded() {
  base::AutoLock auto_lock(lock_);
  return state_ == kEnded;
}

// PlatformThread::Delegate implementation.
void VideoRendererBase::ThreadMain() {
  base::PlatformThread::SetName("CrVideoRenderer");
  base::TimeDelta remaining_time;

  uint32 frames_dropped = 0;

  for (;;) {
    if (frames_dropped > 0) {
      PipelineStatistics statistics;
      statistics.video_frames_dropped = frames_dropped;
      statistics_callback_.Run(statistics);

      frames_dropped = 0;
    }

    base::AutoLock auto_lock(lock_);

    const base::TimeDelta kIdleTimeDelta =
        base::TimeDelta::FromMilliseconds(kIdleMilliseconds);

    if (state_ == kStopped)
      return;

    if (state_ != kPlaying || playback_rate_ == 0) {
      remaining_time = kIdleTimeDelta;
    } else if (frames_queue_ready_.empty() ||
               frames_queue_ready_.front()->IsEndOfStream()) {
      if (current_frame_)
        remaining_time = CalculateSleepDuration(NULL, playback_rate_);
      else
        remaining_time = kIdleTimeDelta;
    } else {
      // Calculate how long until we should advance the frame, which is
      // typically negative but for playback rates < 1.0f may be long enough
      // that it makes more sense to idle and check again.
      scoped_refptr<VideoFrame> next_frame = frames_queue_ready_.front();
      remaining_time = CalculateSleepDuration(next_frame, playback_rate_);
    }

    // TODO(jiesun): I do not think we should wake up every 10ms.
    // We should only wait up when following is true:
    // 1. frame arrival (use event);
    // 2. state_ change (use event);
    // 3. playback_rate_ change (use event);
    // 4. next frame's pts (use timeout);
    if (remaining_time > kIdleTimeDelta)
      remaining_time = kIdleTimeDelta;

    // We can not do anything about this until next frame arrival.
    // We do not want to spin in this case though.
    if (remaining_time.InMicroseconds() < 0 && frames_queue_ready_.empty())
      remaining_time = kIdleTimeDelta;

    if (remaining_time.InMicroseconds() > 0)
      frame_available_.TimedWait(remaining_time);

    if (state_ != kPlaying || playback_rate_ == 0)
      continue;

    // Otherwise we're playing, so advance the frame and keep reading from the
    // decoder when following condition is satisfied:
    // 1. We had at least one backup frame.
    // 2. We had not reached end of stream.
    // 3. Current frame is out-dated.
    if (frames_queue_ready_.empty())
      continue;

    scoped_refptr<VideoFrame> next_frame = frames_queue_ready_.front();
    if (next_frame->IsEndOfStream()) {
      state_ = kEnded;
      DVLOG(1) << "Video render gets EOS";
      host()->NotifyEnded();
      continue;
    }

    // Normally we're ready to loop again at this point, but there are
    // exceptions that cause us to drop a frame and/or consider painting a
    // "next" frame.
    if (next_frame->GetTimestamp() > host()->GetTime() + kIdleTimeDelta &&
        current_frame_ &&
        current_frame_->GetTimestamp() <= host()->GetDuration()) {
      continue;
    }

    // If we got here then:
    // 1. next frame's timestamp is already current; or
    // 2. we do not have a current frame yet; or
    // 3. a special case when the stream is badly formatted and
    //    we got a frame with timestamp greater than overall duration.
    //    In this case we should proceed anyway and try to obtain the
    //    end-of-stream packet.

    if (pending_paint_) {
      // The pending paint might be really slow. Check if we have any frames
      // available that we can drop if they've already expired.
      while (!frames_queue_ready_.empty()) {
        // Can't drop anything if we're at the end.
        if (frames_queue_ready_.front()->IsEndOfStream())
          break;

        base::TimeDelta remaining_time =
            frames_queue_ready_.front()->GetTimestamp() - host()->GetTime();

        // Still a chance we can render the frame!
        if (remaining_time.InMicroseconds() > 0)
          break;

        // Frame dropped: read again.
        ++frames_dropped;
        frames_queue_ready_.pop_front();
        AttemptRead_Locked();
      }

      // Continue waiting for the current paint to finish.
      continue;
    }

    // Congratulations! You've made it past the video frame timing gauntlet.
    //
    // We can now safely update the current frame, request another frame, and
    // signal to the client that a new frame is available.
    DCHECK(!pending_paint_);
    DCHECK(!frames_queue_ready_.empty());
    current_frame_ = frames_queue_ready_.front();
    frames_queue_ready_.pop_front();
    AttemptRead_Locked();

    base::AutoUnlock auto_unlock(lock_);
    OnFrameAvailable();
  }
}

void VideoRendererBase::GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!pending_paint_ && !pending_paint_with_last_available_);

  if ((!current_frame_ || current_frame_->IsEndOfStream()) &&
      (!last_available_frame_ || last_available_frame_->IsEndOfStream())) {
    *frame_out = NULL;
    return;
  }

  // We should have initialized and have the current frame.
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kStopped);
  DCHECK_NE(state_, kError);

  if (current_frame_) {
    *frame_out = current_frame_;
    last_available_frame_ = current_frame_;
    pending_paint_ = true;
  } else {
    DCHECK(last_available_frame_);
    *frame_out = last_available_frame_;
    pending_paint_with_last_available_ = true;
  }
}

void VideoRendererBase::PutCurrentFrame(scoped_refptr<VideoFrame> frame) {
  base::AutoLock auto_lock(lock_);

  // Note that we do not claim |pending_paint_| when we return NULL frame, in
  // that case, |current_frame_| could be changed before PutCurrentFrame.
  if (pending_paint_) {
    DCHECK_EQ(current_frame_, frame);
    DCHECK(!pending_paint_with_last_available_);
    pending_paint_ = false;
  } else if (pending_paint_with_last_available_) {
    DCHECK_EQ(last_available_frame_, frame);
    DCHECK(!pending_paint_);
    pending_paint_with_last_available_ = false;
  } else {
    DCHECK(!frame);
  }

  // We had cleared the |pending_paint_| flag, there are chances that current
  // frame is timed-out. We will wake up our main thread to advance the current
  // frame when this is true.
  frame_available_.Signal();
  if (state_ == kFlushing) {
    AttemptFlush_Locked();
  } else if (state_ == kError || state_ == kStopped) {
    DoStopOrError_Locked();
  }
}

void VideoRendererBase::FrameReady(scoped_refptr<VideoFrame> frame) {
  DCHECK(frame);

  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kStopped);
  DCHECK_NE(state_, kError);
  DCHECK_NE(state_, kFlushed);
  CHECK(pending_read_);

  pending_read_ = false;

  if (state_ == kFlushing) {
    AttemptFlush_Locked();
    return;
  }

  // Discard frames until we reach our desired seek timestamp.
  if (state_ == kSeeking && !frame->IsEndOfStream() &&
      (frame->GetTimestamp() + frame->GetDuration()) <= seek_timestamp_) {
    AttemptRead_Locked();
    return;
  }

  // This one's a keeper! Place it in the ready queue.
  frames_queue_ready_.push_back(frame);
  DCHECK_LE(frames_queue_ready_.size(),
            static_cast<size_t>(Limits::kMaxVideoFrames));
  frame_available_.Signal();

  PipelineStatistics statistics;
  statistics.video_frames_decoded = 1;
  statistics_callback_.Run(statistics);

  // Always request more decoded video if we have capacity. This serves two
  // purposes:
  //   1) Prerolling while paused
  //   2) Keeps decoding going if video rendering thread starts falling behind
  if (frames_queue_ready_.size() < Limits::kMaxVideoFrames &&
      !frame->IsEndOfStream()) {
    AttemptRead_Locked();
    return;
  }

  // If we're at capacity or end of stream while seeking we need to transition
  // to prerolled.
  if (state_ == kSeeking) {
    state_ = kPrerolled;

    // Because we might remain in the prerolled state for an undetermined amount
    // of time (i.e., we were not playing before we received a seek), we'll
    // manually update the current frame and notify the subclass below.
    if (!frames_queue_ready_.front()->IsEndOfStream()) {
      current_frame_ = frames_queue_ready_.front();
      frames_queue_ready_.pop_front();
    }

    // ...and we're done seeking!
    DCHECK(!seek_cb_.is_null());
    ResetAndRunCB(&seek_cb_, PIPELINE_OK);

    base::AutoUnlock ul(lock_);
    OnFrameAvailable();
  }
}

void VideoRendererBase::AttemptRead_Locked() {
  lock_.AssertAcquired();
  DCHECK_NE(kEnded, state_);

  if (pending_read_ || frames_queue_ready_.size() == Limits::kMaxVideoFrames) {
    return;
  }

  pending_read_ = true;
  decoder_->Read(read_cb_);
}

void VideoRendererBase::AttemptFlush_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(kFlushing, state_);

  // Get rid of any ready frames.
  while (!frames_queue_ready_.empty()) {
    frames_queue_ready_.pop_front();
  }

  if (!pending_paint_ && !pending_read_) {
    state_ = kFlushed;
    current_frame_ = NULL;
    ResetAndRunCB(&flush_callback_);
  }
}

base::TimeDelta VideoRendererBase::CalculateSleepDuration(
    VideoFrame* next_frame, float playback_rate) {
  // Determine the current and next presentation timestamps.
  base::TimeDelta now = host()->GetTime();
  base::TimeDelta this_pts = current_frame_->GetTimestamp();
  base::TimeDelta next_pts;
  if (next_frame) {
    next_pts = next_frame->GetTimestamp();
  } else {
    next_pts = this_pts + current_frame_->GetDuration();
  }

  // Determine our sleep duration based on whether time advanced.
  base::TimeDelta sleep;
  if (now == previous_time_) {
    // Time has not changed, assume we sleep for the frame's duration.
    sleep = next_pts - this_pts;
  } else {
    // Time has changed, figure out real sleep duration.
    sleep = next_pts - now;
    previous_time_ = now;
  }

  // Scale our sleep based on the playback rate.
  // TODO(scherkus): floating point badness and degrade gracefully.
  return base::TimeDelta::FromMicroseconds(
      static_cast<int64>(sleep.InMicroseconds() / playback_rate));
}

void VideoRendererBase::DoStopOrError_Locked() {
  DCHECK(!pending_paint_);
  DCHECK(!pending_paint_with_last_available_);
  lock_.AssertAcquired();
  current_frame_ = NULL;
  last_available_frame_ = NULL;
  DCHECK(!pending_read_);
}

}  // namespace media
