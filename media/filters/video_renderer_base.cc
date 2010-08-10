// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "media/base/buffers.h"
#include "media/base/callback.h"
#include "media/base/filter_host.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"

namespace media {

// Limit our read ahead to at least 3 frames.  One frame is typically in flux at
// all times, as in frame n is discarded at the top of ThreadMain() while frame
// (n + kMaxFrames) is being asynchronously fetched.  The remaining two frames
// allow us to advance the current frame as well as read the timestamp of the
// following frame for more accurate timing.
//
// Increasing this number beyond 3 simply creates a larger buffer to work with
// at the expense of memory (~0.5MB and ~1.3MB per frame for 480p and 720p
// resolutions, respectively).  This can help on lower-end systems if there are
// difficult sections in the movie and decoding slows down.
//
// Set to 4 because some vendor's driver doesn't allow buffer count to go below
// preset limit, e.g., EGLImage path.
static const size_t kMaxFrames = 4;

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
      thread_(kNullThreadHandle),
      pending_reads_(0),
      pending_paint_(false),
      playback_rate_(0) {
}

VideoRendererBase::~VideoRendererBase() {
  AutoLock auto_lock(lock_);
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
  AutoLock auto_lock(lock_);
  DCHECK(kPaused == state_ || kFlushing == state_);
  scoped_ptr<FilterCallback> c(callback);
  state_ = kPlaying;
  callback->Run();
}

void VideoRendererBase::Pause(FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK(state_ == kPlaying || state_ == kEnded);
  AutoCallbackRunner done_runner(callback);
  state_ = kPaused;
}

void VideoRendererBase::Flush(FilterCallback* callback) {
  DCHECK(state_ == kPaused);

  AutoLock auto_lock(lock_);
  flush_callback_.reset(callback);
  state_ = kFlushing;

  // Filter is considered paused when we've finished all pending reads, which
  // implies all buffers are returned to owner in Decoder/Renderer. Renderer
  // is considered paused with one more contingency that |pending_paint_| is
  // false, such that no client of us is holding any reference to VideoFrame.
  if (pending_reads_ == 0 && pending_paint_ == false) {
    flush_callback_->Run();
    flush_callback_.reset();
    FlushBuffers();
  }
}

void VideoRendererBase::Stop(FilterCallback* callback) {
  {
    AutoLock auto_lock(lock_);
    state_ = kStopped;

    // TODO(jiesun): move this to flush.
    // TODO(jiesun): we should wait until pending_paint_ is false;
    FlushBuffers();

    // Clean up our thread if present.
    if (thread_) {
      // Signal the thread since it's possible to get stopped with the video
      // thread waiting for a read to complete.
      frame_available_.Signal();
      {
        AutoUnlock auto_unlock(lock_);
        PlatformThread::Join(thread_);
      }
      thread_ = kNullThreadHandle;
    }

  }
  // Signal the subclass we're stopping.
  OnStop(callback);
}

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Seek(base::TimeDelta time, FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK(kPaused == state_ || kFlushing == state_);
  DCHECK_EQ(0u, pending_reads_) << "Pending reads should have completed";
  state_ = kSeeking;
  seek_callback_.reset(callback);
  seek_timestamp_ = time;

  // Throw away everything and schedule our reads.
  // TODO(jiesun): this should be guaranteed by pause/flush before seek happen.
  frames_queue_ready_.clear();
  frames_queue_done_.clear();
  for (size_t i = 0; i < kMaxFrames; ++i) {
    // TODO(jiesun): this is dummy read for ffmpeg path until we truely recycle
    // in that path.
    scoped_refptr<VideoFrame> null_frame;
    frames_queue_done_.push_back(null_frame);
  }

  // TODO(jiesun): if EGL image path make sure those video frames are already in
  // frames_queue_done_, we could remove FillThisBuffer call from derived class.
  // But currently that is trigger by first paint(), which is bad.
  ScheduleRead_Locked();
}

void VideoRendererBase::Initialize(VideoDecoder* decoder,
                                   FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK(decoder);
  DCHECK(callback);
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;
  scoped_ptr<FilterCallback> c(callback);

  decoder_->set_fill_buffer_done_callback(
      NewCallback(this, &VideoRendererBase::OnFillBufferDone));
  // Notify the pipeline of the video dimensions.
  if (!ParseMediaFormat(decoder->media_format(),
                        &surface_type_,
                        &surface_format_,
                        &width_, &height_)) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback->Run();
    return;
  }
  host()->SetVideoSize(width_, height_);

  // Initialize the subclass.
  // TODO(scherkus): do we trust subclasses not to do something silly while
  // we're holding the lock?
  if (!OnInitialize(decoder)) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback->Run();
    return;
  }

  // We're all good!  Consider ourselves paused (ThreadMain() should never
  // see us in the kUninitialized state).
  state_ = kPaused;

  // Create our video thread.
  if (!PlatformThread::Create(0, this, &thread_)) {
    NOTREACHED() << "Video thread creation failed";
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback->Run();
    return;
  }

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)

  // Finally, execute the start callback.
  callback->Run();
}

bool VideoRendererBase::HasEnded() {
  AutoLock auto_lock(lock_);
  return state_ == kEnded;
}

// PlatformThread::Delegate implementation.
void VideoRendererBase::ThreadMain() {
  PlatformThread::SetName("CrVideoRenderer");
  base::TimeDelta remaining_time;

  for (;;) {
    AutoLock auto_lock(lock_);

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

    if (remaining_time > kIdleTimeDelta)
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
      DLOG(INFO) << "Video render gets EOS";
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
        }
      }
      if (timeout_frame.get()) {
        // TODO(jiesun): we should really merge the following branch. That way
        // we will remove the last EGLImage hack in this class. but the
        // |pending_reads_| prevents use to do so (until we had implemented
        // flush logic and get rid of pending reads.)
        if (uses_egl_image() &&
            media::VideoFrame::TYPE_EGL_IMAGE == timeout_frame->type()) {
          decoder_->FillThisBuffer(timeout_frame);
        } else {
          // TODO(jiesun): could this be merged with EGLimage path?
          frames_queue_done_.push_back(timeout_frame);
          ScheduleRead_Locked();
        }
      }
      if (new_frame_available) {
        AutoUnlock auto_unlock(lock_);
        // Notify subclass that |current_frame_| has been updated.
        OnFrameAvailable();
      }
    }
  }
}

void VideoRendererBase::GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out) {
  AutoLock auto_lock(lock_);
  DCHECK(!pending_paint_);

  if (state_ == kStopped || !current_frame_.get() ||
      current_frame_->IsEndOfStream()) {
    *frame_out = NULL;
    return;
  }

  // We should have initialized and have the current frame.
  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying ||
         state_ == kFlushing || state_ == kEnded);
  *frame_out = current_frame_;
  pending_paint_ = true;
}

void VideoRendererBase::PutCurrentFrame(scoped_refptr<VideoFrame> frame) {
  AutoLock auto_lock(lock_);

  // Note that we do not claim |pending_paint_| when we return NULL frame, in
  // that case, |current_frame_| could be changed before PutCurrentFrame.
  DCHECK(pending_paint_ || frame.get() == NULL);
  DCHECK(current_frame_.get() == frame.get() || frame.get() == NULL);

  pending_paint_ = false;
  // We had cleared the |pending_paint_| flag, there are chances that current
  // frame is timed-out. We will wake up our main thread to advance the current
  // frame when this is true.
  frame_available_.Signal();
  if (state_ == kFlushing && pending_reads_ == 0 && flush_callback_.get()) {
    // No more pending reads!  We're now officially "paused".
    FlushBuffers();
    flush_callback_->Run();
    flush_callback_.reset();
  }
}

void VideoRendererBase::OnFillBufferDone(scoped_refptr<VideoFrame> frame) {
  AutoLock auto_lock(lock_);

  // TODO(ajwong): Work around cause we don't synchronize on stop. Correct
  // fix is to resolve http://crbug.com/16059.
  if (state_ == kStopped) {
    // TODO(jiesun): Remove this when flush before stop landed!
    return;
  }

  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying ||
         state_ == kFlushing || state_ == kEnded);
  DCHECK_GT(pending_reads_, 0u);
  --pending_reads_;

  // Discard frames until we reach our desired seek timestamp.
  if (state_ == kSeeking && !frame->IsEndOfStream() &&
      (frame->GetTimestamp() + frame->GetDuration()) < seek_timestamp_) {
    frames_queue_done_.push_back(frame);
    ScheduleRead_Locked();
  } else {
    frames_queue_ready_.push_back(frame);
    DCHECK_LE(frames_queue_ready_.size(), kMaxFrames);
    frame_available_.Signal();
  }

  // Check for our preroll complete condition.
  if (state_ == kSeeking) {
    DCHECK(seek_callback_.get());
    if (frames_queue_ready_.size() == kMaxFrames || frame->IsEndOfStream()) {
      // We're paused, so make sure we update |current_frame_| to represent
      // our new location.
      state_ = kPaused;

      // Because we might remain paused (i.e., we were not playing before we
      // received a seek), we can't rely on ThreadMain() to notify the subclass
      // the frame has been updated.
      scoped_refptr<VideoFrame> first_frame;
      first_frame = frames_queue_ready_.front();
      if (!first_frame->IsEndOfStream()) {
        frames_queue_ready_.pop_front();
        current_frame_ = first_frame;
      }
      OnFrameAvailable();

      seek_callback_->Run();
      seek_callback_.reset();
    }
  } else if (state_ == kFlushing && pending_reads_ == 0 && !pending_paint_) {
    // No more pending reads!  We're now officially "paused".
    if (flush_callback_.get()) {
      flush_callback_->Run();
      flush_callback_.reset();
    }
  }
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
    decoder_->FillThisBuffer(video_frame);
    DCHECK_LT(pending_reads_, kMaxFrames);
    ++pending_reads_;
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
