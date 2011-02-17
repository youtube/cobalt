// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/callback.h"
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
    : width_(0),
      height_(0),
      frame_available_(&lock_),
      state_(kUninitialized),
      thread_(base::kNullThreadHandle),
      pending_reads_(0),
      pending_paint_(false),
      pending_paint_with_last_available_(false),
      playback_rate_(0) {
}

VideoRendererBase::~VideoRendererBase() {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kUninitialized || state_ == kStopped);
}

// static
bool VideoRendererBase::ParseMediaFormat(
    const MediaFormat& media_format,
    VideoFrame::SurfaceType* surface_type_out,
    VideoFrame::Format* surface_format_out,
    int* width_out, int* height_out) {
  std::string mime_type;
  if (!media_format.GetAsString(MediaFormat::kMimeType, &mime_type))
    return false;
  if (mime_type.compare(mime_type::kUncompressedVideo) != 0)
    return false;

  int surface_type;
  if (!media_format.GetAsInteger(MediaFormat::kSurfaceType, &surface_type))
    return false;
  if (surface_type_out)
    *surface_type_out = static_cast<VideoFrame::SurfaceType>(surface_type);

  int surface_format;
  if (!media_format.GetAsInteger(MediaFormat::kSurfaceFormat, &surface_format))
    return false;
  if (surface_format_out)
    *surface_format_out = static_cast<VideoFrame::Format>(surface_format);

  int width, height;
  if (!media_format.GetAsInteger(MediaFormat::kWidth, &width))
    return false;
  if (!media_format.GetAsInteger(MediaFormat::kHeight, &height))
    return false;
  if (width_out) *width_out = width;
  if (height_out) *height_out = height;
  return true;
}

void VideoRendererBase::Play(FilterCallback* callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPrerolled, state_);
  scoped_ptr<FilterCallback> c(callback);
  state_ = kPlaying;
  callback->Run();
}

void VideoRendererBase::Pause(FilterCallback* callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized || state_ == kError);
  AutoCallbackRunner done_runner(callback);
  state_ = kPaused;
}

void VideoRendererBase::Flush(FilterCallback* callback) {
  DCHECK_EQ(state_, kPaused);

  base::AutoLock auto_lock(lock_);
  flush_callback_.reset(callback);
  state_ = kFlushing;

  if (pending_paint_ == false)
    FlushBuffers();
}

void VideoRendererBase::Stop(FilterCallback* callback) {
  DCHECK_EQ(pending_reads_, 0);

  base::PlatformThreadHandle old_thread_handle = base::kNullThreadHandle;
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;

    // Clean up our thread if present.
    if (thread_) {
      // Signal the thread since it's possible to get stopped with the video
      // thread waiting for a read to complete.
      frame_available_.Signal();
      old_thread_handle = thread_;
      thread_ = base::kNullThreadHandle;
    }
  }
  if (old_thread_handle)
    base::PlatformThread::Join(old_thread_handle);

  // Signal the subclass we're stopping.
  OnStop(callback);
}

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Seek(base::TimeDelta time, FilterCallback* callback) {
  base::AutoLock auto_lock(lock_);
  // There is a race condition between filters to receive SeekTask().
  // It turns out we could receive buffer from decoder before seek()
  // is called on us. so we do the following:
  // kFlushed => ( Receive first buffer or Seek() ) => kSeeking and
  // kSeeking => ( Receive enough buffers) => kPrerolled. )
  DCHECK(kPrerolled == state_ || kFlushed == state_ || kSeeking == state_);

  if (state_ == kPrerolled) {
    // Already get enough buffers from decoder.
    callback->Run();
    delete callback;
  } else {
    // Otherwise we are either kFlushed or kSeeking, but without enough buffers;
    // we should save the callback function and call it later.
    state_ = kSeeking;
    seek_callback_.reset(callback);
  }

  seek_timestamp_ = time;
  ScheduleRead_Locked();
}

void VideoRendererBase::Initialize(VideoDecoder* decoder,
                                   FilterCallback* callback,
                                   StatisticsCallback* stats_callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK(decoder);
  DCHECK(callback);
  DCHECK(stats_callback);
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;
  AutoCallbackRunner done_runner(callback);

  statistics_callback_.reset(stats_callback);

  decoder_->set_consume_video_frame_callback(
      NewCallback(this, &VideoRendererBase::ConsumeVideoFrame));
  // Notify the pipeline of the video dimensions.
  if (!ParseMediaFormat(decoder->media_format(),
                        &surface_type_,
                        &surface_format_,
                        &width_, &height_)) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    state_ = kError;
    return;
  }
  host()->SetVideoSize(width_, height_);

  // Initialize the subclass.
  // TODO(scherkus): do we trust subclasses not to do something silly while
  // we're holding the lock?
  if (!OnInitialize(decoder)) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    state_ = kError;
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
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    state_ = kError;
    return;
  }

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)

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
      statistics_callback_->Run(statistics);

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
      if (current_frame_.get())
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

    if (next_frame->GetTimestamp() <= host()->GetTime() + kIdleTimeDelta ||
        current_frame_.get() == NULL ||
        current_frame_->GetTimestamp() > host()->GetDuration()) {
      // 1. either next frame's time-stamp is already current.
      // 2. or we do not have any current frame yet anyway.
      // 3. or a special case when the stream is badly formatted that
      // we get video frame with time-stamp greater than the duration.
      // In this case we should proceed anyway and try to obtain the
      // end-of-stream packet.
      scoped_refptr<VideoFrame> timeout_frame;
      bool new_frame_available = false;
      if (!pending_paint_) {
        // In this case, current frame could be recycled.
        timeout_frame = current_frame_;
        current_frame_ = frames_queue_ready_.front();
        frames_queue_ready_.pop_front();
        new_frame_available = true;
      } else if (pending_paint_ && frames_queue_ready_.size() >= 2 &&
                 !frames_queue_ready_[1]->IsEndOfStream()) {
        // Painter could be really slow, therefore we had to skip frames if
        // 1. We had not reached end of stream.
        // 2. The next frame of current frame is also out-dated.
        base::TimeDelta next_remaining_time =
            frames_queue_ready_[1]->GetTimestamp() - host()->GetTime();
        if (next_remaining_time.InMicroseconds() <= 0) {
          // Since the current frame is still hold by painter/compositor, and
          // the next frame is already timed-out, we should skip the next frame
          // which is the first frame in the queue.
          timeout_frame = frames_queue_ready_.front();
          frames_queue_ready_.pop_front();
          ++frames_dropped;
        }
      }
      if (timeout_frame.get()) {
        frames_queue_done_.push_back(timeout_frame);
        ScheduleRead_Locked();
      }
      if (new_frame_available) {
        base::AutoUnlock auto_unlock(lock_);
        OnFrameAvailable();
      }
    }
  }
}

void VideoRendererBase::GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!pending_paint_ && !pending_paint_with_last_available_);

  if (!current_frame_.get() || current_frame_->IsEndOfStream()) {
    if (!last_available_frame_.get() ||
        last_available_frame_->IsEndOfStream()) {
      *frame_out = NULL;
      return;
    }
  }

  // We should have initialized and have the current frame.
  DCHECK(state_ != kUninitialized && state_ != kStopped && state_ != kError);

  if (current_frame_) {
    *frame_out = current_frame_;
    last_available_frame_ = current_frame_;
    pending_paint_ = true;
  } else {
    DCHECK(last_available_frame_.get() != NULL);
    *frame_out = last_available_frame_;
    pending_paint_with_last_available_ = true;
  }
}

void VideoRendererBase::PutCurrentFrame(scoped_refptr<VideoFrame> frame) {
  base::AutoLock auto_lock(lock_);

  // Note that we do not claim |pending_paint_| when we return NULL frame, in
  // that case, |current_frame_| could be changed before PutCurrentFrame.
  if (pending_paint_) {
    DCHECK(current_frame_.get() == frame.get());
    DCHECK(pending_paint_with_last_available_ == false);
    pending_paint_ = false;
  } else if (pending_paint_with_last_available_) {
    DCHECK(last_available_frame_.get() == frame.get());
    pending_paint_with_last_available_ = false;
  } else {
    DCHECK(frame.get() == NULL);
  }

  // We had cleared the |pending_paint_| flag, there are chances that current
  // frame is timed-out. We will wake up our main thread to advance the current
  // frame when this is true.
  frame_available_.Signal();
  if (state_ == kFlushing)
    FlushBuffers();
}

void VideoRendererBase::ConsumeVideoFrame(scoped_refptr<VideoFrame> frame) {
  PipelineStatistics statistics;
  statistics.video_frames_decoded = 1;
  statistics_callback_->Run(statistics);

  base::AutoLock auto_lock(lock_);

  // Decoder could reach seek state before our Seek() get called.
  // We will enter kSeeking
  if (kFlushed == state_)
    state_ = kSeeking;

  // Synchronous flush between filters should prevent this from happening.
  DCHECK_NE(state_, kStopped);
  if (frame.get() && !frame->IsEndOfStream())
    --pending_reads_;

  DCHECK(state_ != kUninitialized && state_ != kStopped && state_ != kError);

  if (state_ == kPaused || state_ == kFlushing) {
    // Decoder are flushing rubbish video frame, we will not display them.
    if (frame.get() && !frame->IsEndOfStream())
      frames_queue_done_.push_back(frame);
    DCHECK_LE(frames_queue_done_.size(),
              static_cast<size_t>(Limits::kMaxVideoFrames));

    // Excluding kPause here, because in pause state, we will never
    // transfer out-bounding buffer. We do not flush buffer when Compositor
    // hold reference to our video frame either.
    if (state_ == kFlushing && pending_paint_ == false)
      FlushBuffers();

    return;
  }

  // Discard frames until we reach our desired seek timestamp.
  if (state_ == kSeeking && !frame->IsEndOfStream() &&
      (frame->GetTimestamp() + frame->GetDuration()) <= seek_timestamp_) {
    frames_queue_done_.push_back(frame);
    ScheduleRead_Locked();
  } else {
    frames_queue_ready_.push_back(frame);
    DCHECK_LE(frames_queue_ready_.size(),
              static_cast<size_t>(Limits::kMaxVideoFrames));
    frame_available_.Signal();
  }

  // Check for our preroll complete condition.
  bool new_frame_available = false;
  if (state_ == kSeeking) {
    if (frames_queue_ready_.size() == Limits::kMaxVideoFrames ||
        frame->IsEndOfStream()) {
      // We're paused, so make sure we update |current_frame_| to represent
      // our new location.
      state_ = kPrerolled;

      // Because we might remain paused (i.e., we were not playing before we
      // received a seek), we can't rely on ThreadMain() to notify the subclass
      // the frame has been updated.
      scoped_refptr<VideoFrame> first_frame;
      first_frame = frames_queue_ready_.front();
      if (!first_frame->IsEndOfStream()) {
        frames_queue_ready_.pop_front();
        current_frame_ = first_frame;
      }
      new_frame_available = true;

      // If we reach prerolled state before Seek() is called by pipeline,
      // |seek_callback_| is not set, we will return immediately during
      // when Seek() is eventually called.
      if ((seek_callback_.get())) {
        seek_callback_->Run();
        seek_callback_.reset();
      }
    }
  } else if (state_ == kFlushing && pending_reads_ == 0 && !pending_paint_) {
    OnFlushDone();
  }

  if (new_frame_available) {
    base::AutoUnlock auto_unlock(lock_);
    OnFrameAvailable();
  }
}

VideoDecoder* VideoRendererBase::GetDecoder() {
  return decoder_.get();
}

void VideoRendererBase::ReadInput(scoped_refptr<VideoFrame> frame) {
  // We should never return empty frames or EOS frame.
  DCHECK(frame.get() && !frame->IsEndOfStream());

  decoder_->ProduceVideoFrame(frame);
  ++pending_reads_;
}

void VideoRendererBase::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  DCHECK_NE(kEnded, state_);
  // TODO(jiesun): We use dummy buffer to feed decoder to let decoder to
  // provide buffer pools. In the future, we may want to implement real
  // buffer pool to recycle buffers.
  while (!frames_queue_done_.empty()) {
    scoped_refptr<VideoFrame> video_frame = frames_queue_done_.front();
    frames_queue_done_.pop_front();
    ReadInput(video_frame);
  }
}

void VideoRendererBase::FlushBuffers() {
  DCHECK(!pending_paint_);

  // We should never put EOF frame into "done queue".
  while (!frames_queue_ready_.empty()) {
    scoped_refptr<VideoFrame> video_frame = frames_queue_ready_.front();
    if (!video_frame->IsEndOfStream()) {
      frames_queue_done_.push_back(video_frame);
    }
    frames_queue_ready_.pop_front();
  }
  if (current_frame_.get() && !current_frame_->IsEndOfStream()) {
    frames_queue_done_.push_back(current_frame_);
  }
  current_frame_ = NULL;

  if (decoder_->ProvidesBuffer()) {
    // Flush all buffers out to decoder;
    ScheduleRead_Locked();
  }

  if (pending_reads_ == 0)
    OnFlushDone();
}

void VideoRendererBase::OnFlushDone() {
  // Check all buffers are return to owners.
  if (decoder_->ProvidesBuffer()) {
    DCHECK_EQ(frames_queue_done_.size(), 0u);
  } else {
    DCHECK_EQ(frames_queue_done_.size(),
              static_cast<size_t>(Limits::kMaxVideoFrames));
  }
  DCHECK(!current_frame_.get());
  DCHECK(frames_queue_ready_.empty());

  if (flush_callback_.get()) {  // This ensures callback is invoked once.
    flush_callback_->Run();
    flush_callback_.reset();
  }
  state_ = kFlushed;
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

}  // namespace media
