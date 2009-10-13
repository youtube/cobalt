// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/buffers.h"
#include "media/base/filter_host.h"
#include "media/base/video_frame_impl.h"
#include "media/filters/video_renderer_base.h"

namespace media {

// Limit our read ahead to three frames.  One frame is typically in flux at all
// times, as in frame n is discarded at the top of ThreadMain() while frame
// (n + kMaxFrames) is being asynchronously fetched.  The remaining two frames
// allow us to advance the current frame as well as read the timestamp of the
// following frame for more accurate timing.
//
// Increasing this number beyond 3 simply creates a larger buffer to work with
// at the expense of memory (~0.5MB and ~1.3MB per frame for 480p and 720p
// resolutions, respectively).  This can help on lower-end systems if there are
// difficult sections in the movie and decoding slows down.
static const size_t kMaxFrames = 3;

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
      playback_rate_(0) {
}

VideoRendererBase::~VideoRendererBase() {
  AutoLock auto_lock(lock_);
  DCHECK(state_ == kUninitialized || state_ == kStopped);
}

// static
bool VideoRendererBase::ParseMediaFormat(const MediaFormat& media_format,
                                         int* width_out, int* height_out) {
  std::string mime_type;
  if (!media_format.GetAsString(MediaFormat::kMimeType, &mime_type))
    return false;
  if (mime_type.compare(mime_type::kUncompressedVideo) != 0)
    return false;
  if (!media_format.GetAsInteger(MediaFormat::kWidth, width_out))
    return false;
  if (!media_format.GetAsInteger(MediaFormat::kHeight, height_out))
    return false;
  return true;
}

void VideoRendererBase::Play(FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  scoped_ptr<FilterCallback> c(callback);
  state_ = kPlaying;
  callback->Run();
}

void VideoRendererBase::Pause(FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK(state_ == kPlaying || state_ == kEnded);
  pause_callback_.reset(callback);
  state_ = kPaused;

  // We'll only pause when we've finished all pending reads.
  if (pending_reads_ == 0) {
    pause_callback_->Run();
    pause_callback_.reset();
  } else {
    state_ = kPaused;
  }
}

void VideoRendererBase::Stop() {
  AutoLock auto_lock(lock_);
  state_ = kStopped;

  // Signal the subclass we're stopping.
  // TODO(scherkus): do we trust subclasses not to do something silly while
  // we're holding the lock?
  OnStop();

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

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Seek(base::TimeDelta time, FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  DCHECK_EQ(0u, pending_reads_) << "Pending reads should have completed";
  state_ = kSeeking;
  seek_callback_.reset(callback);

  // Throw away everything and schedule our reads.
  frames_.clear();
  for (size_t i = 0; i < kMaxFrames; ++i) {
    ScheduleRead_Locked();
  }
}

void VideoRendererBase::Initialize(VideoDecoder* decoder,
                                   FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK(decoder);
  DCHECK(callback);
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;
  scoped_ptr<FilterCallback> c(callback);

  // Notify the pipeline of the video dimensions.
  if (!ParseMediaFormat(decoder->media_format(), &width_, &height_)) {
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

  // Create a black frame so clients have something to render before we finish
  // prerolling.
  VideoFrameImpl::CreateBlackFrame(width_, height_, &current_frame_);

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
  PlatformThread::SetName("VideoThread");
  for (;;) {
    // State and playback rate to assume for this iteration of the loop.
    State state;
    float playback_rate;
    base::TimeDelta remaining_time;
    {
      AutoLock auto_lock(lock_);
      state = state_;
      playback_rate = playback_rate_;

      // Calculate how long until we should advance the frame, which is
      // typically negative but for playback rates < 1.0f may be long enough
      // that it makes more sense to idle and check again.
      remaining_time = current_frame_->GetTimestamp() - host()->GetTime();
    }
    if (state == kStopped) {
      return;
    }

    // Idle if we shouldn't be playing or advancing the frame yet.
    if (state == kPaused || state == kSeeking || state == kEnded ||
        remaining_time.InMilliseconds() > kIdleMilliseconds ||
        playback_rate == 0) {
      PlatformThread::Sleep(kIdleMilliseconds);
      continue;
    }

    // Advance |current_frame_| and try to determine |next_frame|.  Note that
    // this loop executes our "playing" logic.
    DCHECK_EQ(kPlaying, state);
    scoped_refptr<VideoFrame> next_frame;
    {
      AutoLock auto_lock(lock_);
      // Check the actual state to see if we're trying to stop playing.
      if (state_ != kPlaying) {
        continue;
      }

      // Otherwise we're playing, so advance the frame and keep reading from the
      // decoder if we haven't reach end of stream.
      if (!frames_.empty() && !frames_.front()->IsEndOfStream()) {
        DCHECK_EQ(current_frame_, frames_.front());
        frames_.pop_front();
        ScheduleRead_Locked();
      }

      // While playing, we'll wait until a new frame arrives before updating
      // |current_frame_|.
      while (frames_.empty() && state_ == kPlaying) {
        frame_available_.Wait();
      }

      // If we ended up transitioning out of playing while waiting for a new
      // frame, restart the iteration.
      if (state_ != kPlaying) {
        continue;
      }

      // If the new front frame is end of stream, we've officially ended.
      if (frames_.front()->IsEndOfStream()) {
        state_ = kEnded;
        host()->NotifyEnded();
        continue;
      }

      // Update our current frame and attempt to grab the next frame.
      current_frame_ = frames_.front();
      if (frames_.size() >= 2 && !frames_[1]->IsEndOfStream()) {
        next_frame = frames_[1];
      }
    }

    // Calculate our sleep duration.
    base::TimeDelta sleep = CalculateSleepDuration(next_frame, playback_rate);
    int sleep_ms = static_cast<int>(sleep.InMilliseconds());

    // If we're too far behind to catch up, simply drop the frame.
    //
    // This has the effect of potentially dropping a few frames when playback
    // resumes after being paused.  The alternative (sleeping for 0 milliseconds
    // and trying to catch up) looks worse.
    if (sleep_ms < 0)
      continue;

    // To be safe, limit our sleep duration.
    // TODO(scherkus): handle seeking gracefully.. right now we tend to hit
    // kMaxSleepMilliseconds a lot when we seek backwards.
    if (sleep_ms > kMaxSleepMilliseconds)
      sleep_ms = kMaxSleepMilliseconds;

    // Notify subclass that |current_frame_| has been updated.
    OnFrameAvailable();

    PlatformThread::Sleep(sleep_ms);
  }
}

void VideoRendererBase::GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out) {
  AutoLock auto_lock(lock_);
  // We should have initialized and have the current frame.
  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying ||
         state_ == kEnded);
  DCHECK(current_frame_);
  *frame_out = current_frame_;
}

void VideoRendererBase::OnReadComplete(VideoFrame* frame) {
  AutoLock auto_lock(lock_);

  // TODO(ajwong): Work around cause we don't synchronize on stop. Correct
  // fix is to resolve http://crbug.com/16059.
  if (state_ == kStopped) {
    return;
  }

  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying ||
         state_ == kEnded);
  DCHECK_GT(pending_reads_, 0u);
  --pending_reads_;

  // Enqueue the frame.
  frames_.push_back(frame);
  DCHECK_LE(frames_.size(), kMaxFrames);
  frame_available_.Signal();

  // Check for our preroll complete condition.
  if (state_ == kSeeking) {
    DCHECK(seek_callback_.get());
    if (frames_.size() == kMaxFrames) {
      // We're paused, so make sure we update |current_frame_| to represent
      // our new location.
      state_ = kPaused;
      if (frames_.front()->IsEndOfStream()) {
        VideoFrameImpl::CreateBlackFrame(width_, height_, &current_frame_);
      } else {
        current_frame_ = frames_.front();
      }

      // Because we might remain paused (i.e., we were not playing before we
      // received a seek), we can't rely on ThreadMain() to notify the subclass
      // the frame has been updated.
      DCHECK(current_frame_);
      OnFrameAvailable();

      seek_callback_->Run();
      seek_callback_.reset();
    }
  } else if (state_ == kPaused && pending_reads_ == 0) {
    // No more pending reads!  We're now officially "paused".
    if (pause_callback_.get()) {
      pause_callback_->Run();
      pause_callback_.reset();
    }
  }
}

void VideoRendererBase::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  DCHECK_NE(kEnded, state_);
  DCHECK_LT(pending_reads_, kMaxFrames);
  ++pending_reads_;
  decoder_->Read(NewCallback(this, &VideoRendererBase::OnReadComplete));
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
