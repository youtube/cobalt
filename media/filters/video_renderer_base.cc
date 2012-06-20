// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"

namespace media {

VideoRendererBase::VideoRendererBase(const base::Closure& paint_cb,
                                     const SetOpaqueCB& set_opaque_cb,
                                     bool drop_frames)
    : frame_available_(&lock_),
      state_(kUninitialized),
      thread_(base::kNullThreadHandle),
      pending_read_(false),
      pending_paint_(false),
      pending_paint_with_last_available_(false),
      drop_frames_(drop_frames),
      playback_rate_(0),
      paint_cb_(paint_cb),
      set_opaque_cb_(set_opaque_cb) {
  DCHECK(!paint_cb_.is_null());
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
  flush_cb_ = callback;
  state_ = kFlushingDecoder;

  // We must unlock here because the callback might run within the Flush()
  // call.
  // TODO: Remove this line when fixing http://crbug.com/125020
  base::AutoUnlock auto_unlock(lock_);
  decoder_->Reset(base::Bind(&VideoRendererBase::OnDecoderFlushDone, this));
}

void VideoRendererBase::Stop(const base::Closure& callback) {
  if (state_ == kStopped) {
    callback.Run();
    return;
  }

  base::PlatformThreadHandle thread_to_join = base::kNullThreadHandle;
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;

    statistics_cb_.Reset();
    time_cb_.Reset();
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

  decoder_->Stop(callback);
}

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kFlushed) << "Must flush prior to seeking.";
  DCHECK(!cb.is_null());
  DCHECK(seek_cb_.is_null());

  state_ = kSeeking;
  seek_cb_ = cb;
  seek_timestamp_ = time;
  AttemptRead_Locked();
}

void VideoRendererBase::Initialize(const scoped_refptr<VideoDecoder>& decoder,
                                   const PipelineStatusCB& status_cb,
                                   const StatisticsCB& statistics_cb,
                                   const TimeCB& time_cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK(decoder);
  DCHECK(!status_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!time_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;

  statistics_cb_ = statistics_cb;
  time_cb_ = time_cb;

  // Notify the pipeline of the video dimensions.
  host()->SetNaturalVideoSize(decoder_->natural_size());

  // We're all good!  Consider ourselves flushed. (ThreadMain() should never
  // see us in the kUninitialized state).
  // Since we had an initial Seek, we consider ourself flushed, because we
  // have not populated any buffers yet.
  state_ = kFlushed;

  set_opaque_cb_.Run(!decoder->HasAlpha());
  set_opaque_cb_.Reset();

  // Create our video thread.
  if (!base::PlatformThread::Create(0, this, &thread_)) {
    NOTREACHED() << "Video thread creation failed";
    state_ = kError;
    status_cb.Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)
  status_cb.Run(PIPELINE_OK);
}

bool VideoRendererBase::HasEnded() {
  base::AutoLock auto_lock(lock_);
  return state_ == kEnded;
}

// PlatformThread::Delegate implementation.
void VideoRendererBase::ThreadMain() {
  base::PlatformThread::SetName("CrVideoRenderer");

  // The number of milliseconds to idle when we do not have anything to do.
  // Nothing special about the value, other than we're being more OS-friendly
  // than sleeping for 1 millisecond.
  //
  // TOOD(scherkus): switch to pure event-driven frame timing instead of this
  // kIdleTimeDelta business http://crbug.com/106874
  const base::TimeDelta kIdleTimeDelta =
      base::TimeDelta::FromMilliseconds(10);

  uint32 frames_dropped = 0;

  for (;;) {
    if (frames_dropped > 0) {
      PipelineStatistics statistics;
      statistics.video_frames_dropped = frames_dropped;
      statistics_cb_.Run(statistics);

      frames_dropped = 0;
    }

    base::AutoLock auto_lock(lock_);

    // Thread exit condition.
    if (state_ == kStopped)
      return;

    // Remain idle as long as we're not playing.
    if (state_ != kPlaying || playback_rate_ == 0) {
      frame_available_.TimedWait(kIdleTimeDelta);
      continue;
    }

    // Remain idle until we have the next frame ready for rendering.
    if (ready_frames_.empty()) {
      frame_available_.TimedWait(kIdleTimeDelta);
      continue;
    }

    // Remain idle until we've initialized |current_frame_| via prerolling.
    if (!current_frame_) {
      // This can happen if our preroll only contains end of stream frames.
      if (ready_frames_.front()->IsEndOfStream()) {
        state_ = kEnded;
        host()->NotifyEnded();
        ready_frames_.clear();

        // No need to sleep here as we idle when |state_ != kPlaying|.
        continue;
      }

      frame_available_.TimedWait(kIdleTimeDelta);
      continue;
    }

    // Calculate how long until we should advance the frame, which is
    // typically negative but for playback rates < 1.0f may be long enough
    // that it makes more sense to idle and check again.
    base::TimeDelta remaining_time =
        CalculateSleepDuration(ready_frames_.front(), playback_rate_);

    // Sleep up to a maximum of our idle time until we're within the time to
    // render the next frame.
    if (remaining_time.InMicroseconds() > 0) {
      remaining_time = std::min(remaining_time, kIdleTimeDelta);
      frame_available_.TimedWait(remaining_time);
      continue;
    }


    // We're almost there!
    //
    // At this point we've rendered |current_frame_| for the proper amount
    // of time and also have the next frame that ready for rendering.


    // If the next frame is end of stream then we are truly at the end of the
    // video stream.
    //
    // TODO(scherkus): deduplicate this end of stream check after we get rid of
    // |current_frame_|.
    if (ready_frames_.front()->IsEndOfStream()) {
      state_ = kEnded;
      host()->NotifyEnded();
      ready_frames_.clear();

      // No need to sleep here as we idle when |state_ != kPlaying|.
      continue;
    }

    // We cannot update |current_frame_| until we've completed the pending
    // paint. Furthermore, the pending paint might be really slow: check to
    // see if we have any ready frames that we can drop if they've already
    // expired.
    if (pending_paint_) {
      while (!ready_frames_.empty()) {
        // Can't drop anything if we're at the end.
        if (ready_frames_.front()->IsEndOfStream())
          break;

        base::TimeDelta remaining_time =
            ready_frames_.front()->GetTimestamp() - host()->GetTime();

        // Still a chance we can render the frame!
        if (remaining_time.InMicroseconds() > 0)
          break;

        if (!drop_frames_)
          break;

        // Frame dropped: read again.
        ++frames_dropped;
        ready_frames_.pop_front();
        AttemptRead_Locked();
      }
      // Continue waiting for the current paint to finish.
      frame_available_.TimedWait(kIdleTimeDelta);
      continue;
    }


    // Congratulations! You've made it past the video frame timing gauntlet.
    //
    // We can now safely update the current frame, request another frame, and
    // signal to the client that a new frame is available.
    DCHECK(!pending_paint_);
    DCHECK(!ready_frames_.empty());
    current_frame_ = ready_frames_.front();
    ready_frames_.pop_front();
    AttemptRead_Locked();

    base::AutoUnlock auto_unlock(lock_);
    paint_cb_.Run();
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
  if (state_ == kFlushingDecoder)
    return;

  if (state_ == kFlushing) {
    AttemptFlush_Locked();
    return;
  }

  if (state_ == kError || state_ == kStopped) {
    DoStopOrError_Locked();
  }
}

VideoRendererBase::~VideoRendererBase() {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kUninitialized || state_ == kStopped) << state_;
}

void VideoRendererBase::FrameReady(VideoDecoder::DecoderStatus status,
                                   const scoped_refptr<VideoFrame>& frame) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, kUninitialized);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status != VideoDecoder::kOk) {
    DCHECK(!frame);
    PipelineStatus error = PIPELINE_ERROR_DECODE;
    if (status == VideoDecoder::kDecryptError)
      error = PIPELINE_ERROR_DECRYPT;

    if (!seek_cb_.is_null()) {
      base::ResetAndReturn(&seek_cb_).Run(error);
      return;
    }

    host()->SetError(error);
    return;
  }

  // Already-queued Decoder ReadCB's can fire after various state transitions
  // have happened; in that case just drop those frames immediately.
  if (state_ == kStopped || state_ == kError || state_ == kFlushed ||
      state_ == kFlushingDecoder)
    return;

  if (state_ == kFlushing) {
    AttemptFlush_Locked();
    return;
  }

  if (!frame) {
    if (state_ != kSeeking)
      return;

    // Abort seek early for a NULL frame because we won't get more frames.
    // A new seek will be requested after this one completes so there is no
    // point trying to collect more frames.
    state_ = kPrerolled;
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
    return;
  }

  // Discard frames until we reach our desired seek timestamp.
  if (state_ == kSeeking && !frame->IsEndOfStream() &&
      (frame->GetTimestamp() + frame->GetDuration()) <= seek_timestamp_) {
    AttemptRead_Locked();
    return;
  }

  // Adjust the incoming frame if its rendering stop time is past the duration
  // of the video itself. This is typically the last frame of the video and
  // occurs if the container specifies a duration that isn't a multiple of the
  // frame rate.  Another way for this to happen is for the container to state a
  // smaller duration than the largest packet timestamp.
  if (!frame->IsEndOfStream()) {
    if (frame->GetTimestamp() > host()->GetDuration())
      frame->SetTimestamp(host()->GetDuration());
    if ((frame->GetTimestamp() + frame->GetDuration()) > host()->GetDuration())
      frame->SetDuration(host()->GetDuration() - frame->GetTimestamp());
  }

  // This one's a keeper! Place it in the ready queue.
  ready_frames_.push_back(frame);
  DCHECK_LE(NumFrames_Locked(), limits::kMaxVideoFrames);
  if (!frame->IsEndOfStream())
    time_cb_.Run(frame->GetTimestamp() + frame->GetDuration());
  frame_available_.Signal();

  PipelineStatistics statistics;
  statistics.video_frames_decoded = 1;
  statistics_cb_.Run(statistics);

  // Always request more decoded video if we have capacity. This serves two
  // purposes:
  //   1) Prerolling while paused
  //   2) Keeps decoding going if video rendering thread starts falling behind
  if (NumFrames_Locked() < limits::kMaxVideoFrames && !frame->IsEndOfStream()) {
    AttemptRead_Locked();
    return;
  }

  // If we're at capacity or end of stream while seeking we need to transition
  // to prerolled.
  if (state_ == kSeeking) {
    DCHECK(!current_frame_);
    state_ = kPrerolled;

    // Because we might remain in the prerolled state for an undetermined amount
    // of time (i.e., we were not playing before we received a seek), we'll
    // manually update the current frame and notify the subclass below.
    if (!ready_frames_.front()->IsEndOfStream()) {
      current_frame_ = ready_frames_.front();
      ready_frames_.pop_front();
    }

    // ...and we're done seeking!
    DCHECK(!seek_cb_.is_null());
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);

    base::AutoUnlock ul(lock_);
    paint_cb_.Run();
  }
}

void VideoRendererBase::AttemptRead_Locked() {
  lock_.AssertAcquired();
  DCHECK_NE(kEnded, state_);

  if (pending_read_ ||
      NumFrames_Locked() == limits::kMaxVideoFrames ||
      (!ready_frames_.empty() && ready_frames_.back()->IsEndOfStream()) ||
      state_ == kFlushingDecoder ||
      state_ == kFlushing) {
    return;
  }

  pending_read_ = true;
  decoder_->Read(base::Bind(&VideoRendererBase::FrameReady, this));
}

void VideoRendererBase::OnDecoderFlushDone() {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kFlushingDecoder, state_);
  DCHECK(!pending_read_);

  state_ = kFlushing;
  AttemptFlush_Locked();
}

void VideoRendererBase::AttemptFlush_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(kFlushing, state_);

  // Get rid of any ready frames.
  ready_frames_.clear();

  if (!pending_paint_ && !pending_read_) {
    state_ = kFlushed;
    current_frame_ = NULL;
    base::ResetAndReturn(&flush_cb_).Run();
  }
}

base::TimeDelta VideoRendererBase::CalculateSleepDuration(
    const scoped_refptr<VideoFrame>& next_frame,
    float playback_rate) {
  // Determine the current and next presentation timestamps.
  base::TimeDelta now = host()->GetTime();
  base::TimeDelta this_pts = current_frame_->GetTimestamp();
  base::TimeDelta next_pts;
  if (!next_frame->IsEndOfStream()) {
    next_pts = next_frame->GetTimestamp();
  } else {
    next_pts = this_pts + current_frame_->GetDuration();
  }

  // Scale our sleep based on the playback rate.
  base::TimeDelta sleep = next_pts - now;
  return base::TimeDelta::FromMicroseconds(
      static_cast<int64>(sleep.InMicroseconds() / playback_rate));
}

void VideoRendererBase::DoStopOrError_Locked() {
  DCHECK(!pending_paint_);
  DCHECK(!pending_paint_with_last_available_);
  lock_.AssertAcquired();
  current_frame_ = NULL;
  last_available_frame_ = NULL;
  ready_frames_.clear();
}

int VideoRendererBase::NumFrames_Locked() const {
  lock_.AssertAcquired();
  int outstanding_frames =
      (current_frame_ ? 1 : 0) + (last_available_frame_ ? 1 : 0) +
      (current_frame_ && (current_frame_ == last_available_frame_) ? -1 : 0);
  return ready_frames_.size() + outstanding_frames;
}

}  // namespace media
