// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_renderer_base.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/limits.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/video_decoder_selector.h"

#if defined(__LB_SHELL__) || defined(COBALT)
#include "base/debug/trace_event.h"
#include "base/stringprintf.h"
#include "media/base/shell_media_platform.h"
#include "media/base/shell_media_statistics.h"
#endif

namespace media {

#if !defined(__LB_SHELL__FOR_RELEASE__)
int VideoRendererBase::videos_played_;
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)

base::TimeDelta VideoRendererBase::kMaxLastFrameDuration() {
  return base::TimeDelta::FromMilliseconds(250);
}

VideoRendererBase::VideoRendererBase(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const SetDecryptorReadyCB& set_decryptor_ready_cb,
    const base::Closure& paint_cb,
    const SetOpaqueCB& set_opaque_cb,
    bool drop_frames)
    : message_loop_(message_loop),
      set_decryptor_ready_cb_(set_decryptor_ready_cb),
      frame_available_(&lock_),
      state_(kUninitialized),
      thread_(base::kNullThreadHandle),
      pending_read_(false),
      pending_paint_(false),
      pending_paint_with_last_available_(false),
      drop_frames_(drop_frames),
      playback_rate_(0),
      paint_cb_(paint_cb),
      set_opaque_cb_(set_opaque_cb),
      maximum_frames_cached_(0) {
  DCHECK(!paint_cb_.is_null());
#if defined(__LB_SHELL__) || defined(COBALT)
  frame_provider_ = ShellMediaPlatform::Instance()->GetVideoFrameProvider();

#if !defined(__LB_SHELL__FOR_RELEASE__)
  late_frames_ = 0;
  DLOG(INFO) << "Start playing back video " << videos_played_;
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)
#endif  // defined(__LB_SHELL__) || defined(COBALT)
}

VideoRendererBase::~VideoRendererBase() {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kStopped || state_ == kUninitialized) << state_;
  DCHECK_EQ(thread_, base::kNullThreadHandle);
#if !defined(__LB_SHELL__FOR_RELEASE__)
  ++videos_played_;
  DLOG_IF(INFO, late_frames_ != 0) << "Finished playing back with "
                                   << late_frames_ << " late frames.";
  DLOG_IF(INFO, late_frames_ == 0)
      << "Finished playing back with no late frame.";
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)
}

void VideoRendererBase::Play(const base::Closure& callback) {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT0("media_stack", "VideoRendererBase::Play()");
#endif
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPrerolled, state_);
  state_ = kPlaying;
  callback.Run();
}

void VideoRendererBase::Pause(const base::Closure& callback) {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT0("media_stack", "VideoRendererBase::Pause()");
#endif
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized || state_ == kError);
  state_ = kPaused;
  callback.Run();
}

void VideoRendererBase::Flush(const base::Closure& callback) {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT0("media_stack", "VideoRendererBase::Flush()");
#endif
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPaused);
  flush_cb_ = callback;
  state_ = kFlushingDecoder;

  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(base::Bind(
        &VideoRendererBase::ResetDecoder, this));
    return;
  }

  decoder_->Reset(base::Bind(&VideoRendererBase::OnDecoderResetDone, this));
}

void VideoRendererBase::ResetDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  decoder_->Reset(base::Bind(&VideoRendererBase::OnDecoderResetDone, this));
}

void VideoRendererBase::Stop(const base::Closure& callback) {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT0("media_stack", "VideoRendererBase::Stop()");
#endif
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (state_ == kUninitialized || state_ == kStopped) {
    callback.Run();
    return;
  }

  base::PlatformThreadHandle thread_to_join = base::kNullThreadHandle;
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;

    statistics_cb_.Reset();
    max_time_cb_.Reset();
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

#if defined(__LB_SHELL__) || defined(COBALT)
  if (frame_provider_) { frame_provider_->Stop(); }
#endif  // defined(__LB_SHELL__) || defined(COBALT)

  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(base::Bind(
        &VideoRendererBase::StopDecoder, this, callback));
    return;
  }

  if (decoder_) {
    decoder_->Stop(callback);
    return;
  }

  callback.Run();
}

void VideoRendererBase::StopDecoder(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  decoder_->Stop(callback);
}

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Preroll(base::TimeDelta time,
                                const PipelineStatusCB& cb) {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT1("media_stack", "VideoRendererBase::Preroll()",
               "timestamp", time.InMicroseconds());
#endif
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kFlushed) << "Must flush prior to prerolling.";
  DCHECK(!cb.is_null());
  DCHECK(preroll_cb_.is_null());

  state_ = kPrerolling;
  preroll_cb_ = cb;
  preroll_timestamp_ = time;
  prerolling_delayed_frame_ = NULL;
  AttemptRead_Locked();
}

void VideoRendererBase::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                   const VideoDecoderList& decoders,
                                   const PipelineStatusCB& init_cb,
                                   const StatisticsCB& statistics_cb,
                                   const TimeCB& max_time_cb,
                                   const NaturalSizeChangedCB& size_changed_cb,
                                   const base::Closure& ended_cb,
                                   const PipelineStatusCB& error_cb,
                                   const TimeDeltaCB& get_time_cb,
                                   const TimeDeltaCB& get_duration_cb) {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT0("media_stack", "VideoRendererBase::Initialize()");
#endif
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(stream);
  DCHECK(!decoders.empty());
  DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!max_time_cb.is_null());
  DCHECK(!size_changed_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!get_time_cb.is_null());
  DCHECK(!get_duration_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);

  init_cb_ = init_cb;
  statistics_cb_ = statistics_cb;
  max_time_cb_ = max_time_cb;
  size_changed_cb_ = size_changed_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  get_time_cb_ = get_time_cb;
  get_duration_cb_ = get_duration_cb;
  state_ = kInitializing;

  scoped_ptr<VideoDecoderSelector> decoder_selector(
      new VideoDecoderSelector(base::MessageLoopProxy::current(),
                               decoders,
                               set_decryptor_ready_cb_));

  // To avoid calling |decoder_selector| methods and passing ownership of
  // |decoder_selector| in the same line.
  VideoDecoderSelector* decoder_selector_ptr = decoder_selector.get();

  decoder_selector_ptr->SelectVideoDecoder(
      stream,
      statistics_cb,
      base::Bind(&VideoRendererBase::OnDecoderSelected, this,
                 base::Passed(&decoder_selector)));
}

void VideoRendererBase::OnDecoderSelected(
    scoped_ptr<VideoDecoderSelector> decoder_selector,
    const scoped_refptr<VideoDecoder>& selected_decoder,
    const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (state_ == kStopped)
    return;

  DCHECK_EQ(state_, kInitializing);

  if (!selected_decoder) {
    state_ = kUninitialized;
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  decoder_ = selected_decoder;
  decrypting_demuxer_stream_ = decrypting_demuxer_stream;

  // We're all good!  Consider ourselves flushed. (ThreadMain() should never
  // see us in the kUninitialized state).
  // Since we had an initial Preroll(), we consider ourself flushed, because we
  // have not populated any buffers yet.
  state_ = kFlushed;

  set_opaque_cb_.Run(!decoder_->HasAlpha());
  set_opaque_cb_.Reset();

  // Create our video thread.
  if (!base::PlatformThread::Create(0, this, &thread_)) {
    NOTREACHED() << "Video thread creation failed";
    state_ = kError;
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
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
#if defined(__LB_SHELL__) || defined(COBALT)
    frames_dropped =
        static_cast<uint32>(frame_provider_->ResetAndReturnDroppedFrames());
    if (frames_dropped > 0) {
      SCOPED_MEDIA_STATISTICS(STAT_TYPE_VIDEO_FRAME_DROP);
    }
#endif  // defined(__LB_SHELL__) || defined(COBALT)
    if (frames_dropped > 0) {
      if (!statistics_cb_.is_null()) {
        PipelineStatistics statistics;
        statistics.video_frames_dropped = frames_dropped;
        statistics_cb_.Run(statistics);
      }
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

#if defined(__LB_SHELL__) || defined(COBALT)
    if (frame_provider_ && frame_provider_->QueryAndResetHasConsumedFrames()) {
      // The consumer of the frame_provider_ has consumed frames. Post another
      // AttemptRead task to ensure that new frames are being read in to keep
      // the frame_provider_'s queue full.
      message_loop_->PostTask(FROM_HERE, base::Bind(
          &VideoRendererBase::AttemptRead, this));
    }
#endif

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
        ended_cb_.Run();
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
      ended_cb_.Run();
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
            ready_frames_.front()->GetTimestamp() - get_time_cb_.Run();

        // Still a chance we can render the frame!
        if (remaining_time.InMicroseconds() > 0)
          break;

        if (!drop_frames_)
          break;

        // Frame dropped: read again.
#if defined(__LB_SHELL__) || defined(COBALT)
        TRACE_EVENT1("media_stack", "VideoRendererBase drop frame",
                     "timestamp",
                     ready_frames_.front()->GetTimestamp().InMicroseconds());
#endif
        ++frames_dropped;
        ready_frames_.pop_front();
#if defined(__LB_SHELL__) || defined(COBALT)
        UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_RENDERER_BACKLOG,
                                ready_frames_.size());
#endif  // defined(__LB_SHELL__) || defined(COBALT)
        message_loop_->PostTask(FROM_HERE, base::Bind(
            &VideoRendererBase::AttemptRead, this));
      }
      // Continue waiting for the current paint to finish.
      frame_available_.TimedWait(kIdleTimeDelta);
      continue;
    }


    // Congratulations! You've made it past the video frame timing gauntlet.
    //
    // We can now safely update the current frame, request another frame, and
    // signal to the client that a new frame is available.
#if defined(__LB_SHELL__) || defined(COBALT)
    TRACE_EVENT0("media_stack", "VideoRendererBase proceed to next frame");
#endif
    DCHECK(!pending_paint_);
    DCHECK(!ready_frames_.empty());
    SetCurrentFrameToNextReadyFrame();
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &VideoRendererBase::AttemptRead, this));

#if defined(__LB_SHELL__) || defined(COBALT)
    TRACE_EVENT0("media_stack", "VideoRendererBase paint_cb_");
#endif
    base::AutoUnlock auto_unlock(lock_);
    paint_cb_.Run();
  }
}

void VideoRendererBase::SetCurrentFrameToNextReadyFrame() {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT1("media_stack",
               "VideoRendererBase::SetCurrentFrameToNextReadyFrame()",
               "timestamp",
               ready_frames_.front()->GetTimestamp().InMicroseconds());
#endif
  current_frame_ = ready_frames_.front();
  ready_frames_.pop_front();
#if defined(__LB_SHELL__) || defined(COBALT)
  UPDATE_MEDIA_STATISTICS(STAT_TYPE_VIDEO_RENDERER_BACKLOG,
                          ready_frames_.size());
#endif  // defined(__LB_SHELL__) || defined(COBALT)

  // Notify the pipeline of natural_size() changes.
  const gfx::Size& natural_size = current_frame_->natural_size();
  if (natural_size != last_natural_size_) {
    size_changed_cb_.Run(natural_size);
    last_natural_size_ = natural_size;
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

#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT0("media_stack", "VideoRendererBase::GetCurrentFrame()");
#endif

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
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT0("media_stack", "VideoRendererBase::PutCurrentFrame()");
#endif

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

void VideoRendererBase::FrameReady(
    VideoDecoder::Status status,
    const scoped_refptr<VideoFrame>& incoming_frame) {
  // Make a copy as we may assign newly created punch out frame to `frame` when
  // frame provider is available.
  scoped_refptr<VideoFrame> frame(incoming_frame);

  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, kUninitialized);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status != VideoDecoder::kOk) {
    DCHECK(!frame);
    PipelineStatus error = PIPELINE_ERROR_DECODE;
    if (status == VideoDecoder::kDecryptError)
      error = PIPELINE_ERROR_DECRYPT;

    if (!preroll_cb_.is_null()) {
      base::ResetAndReturn(&preroll_cb_).Run(error);
      return;
    }

    error_cb_.Run(error);
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
    if (state_ != kPrerolling)
      return;

    // Abort preroll early for a NULL frame because we won't get more frames.
    // A new preroll will be requested after this one completes so there is no
    // point trying to collect more frames.
    state_ = kPrerolled;
    base::ResetAndReturn(&preroll_cb_).Run(PIPELINE_OK);
    return;
  }

#if defined(__LB_SHELL__) || defined(COBALT)
  scoped_refptr<VideoFrame> original_frame;
  if (frame_provider_ && !frame->IsEndOfStream()) {
    // Save the frame to original_frame so it can be added into frame_provider
    // later.
    original_frame = frame;
    frame = VideoFrame::CreatePunchOutFrame(
        original_frame->visible_rect().size());
    frame->SetTimestamp(original_frame->GetTimestamp());
  }
#endif  // defined(__LB_SHELL__) || defined(COBALT)

  // Discard frames until we reach our desired preroll timestamp.
  if (state_ == kPrerolling && !frame->IsEndOfStream() &&
      frame->GetTimestamp() <= preroll_timestamp_) {
    prerolling_delayed_frame_ = frame;
    AttemptRead_Locked();
    return;
  }

#if defined(__LB_SHELL__) || defined(COBALT)
  if (frame_provider_ && original_frame) {
    frame_provider_->AddFrame(original_frame);
  }
#endif  // defined(__LB_SHELL__) || defined(COBALT)

  if (prerolling_delayed_frame_) {
    DCHECK_EQ(state_, kPrerolling);
    AddReadyFrame(prerolling_delayed_frame_);
    prerolling_delayed_frame_ = NULL;
  }

  AddReadyFrame(frame);
  PipelineStatistics statistics;
  statistics.video_frames_decoded = 1;
  statistics_cb_.Run(statistics);

  // Always request more decoded video if we have capacity. This serves two
  // purposes:
  //   1) Prerolling while paused
  //   2) Keeps decoding going if video rendering thread starts falling behind
#if defined(__LB_SHELL__) || defined(COBALT)
  bool is_prerolling = state_ == kPrerolling;
  int max_video_frames =
      is_prerolling ? ShellMediaPlatform::Instance()->GetMaxVideoPrerollFrames()
                    : ShellMediaPlatform::Instance()->GetMaxVideoFrames();
  if (NumFrames_Locked() < max_video_frames && !frame->IsEndOfStream()) {
#else  // defined(__LB_SHELL__) || defined(COBALT)
  if (NumFrames_Locked() < limits::kMaxVideoFrames && !frame->IsEndOfStream()) {
#endif  // defined(__LB_SHELL__) || defined(COBALT)
    AttemptRead_Locked();
    return;
  }

  // If we're at capacity or end of stream while prerolling we need to
  // transition to prerolled.
  if (state_ == kPrerolling) {
    DCHECK(!current_frame_);
    state_ = kPrerolled;

    // Because we might remain in the prerolled state for an undetermined amount
    // of time (i.e., we were not playing before we started prerolling), we'll
    // manually update the current frame and notify the subclass below.
    if (!ready_frames_.front()->IsEndOfStream())
      SetCurrentFrameToNextReadyFrame();

    // ...and we're done prerolling!
    DCHECK(!preroll_cb_.is_null());
    base::ResetAndReturn(&preroll_cb_).Run(PIPELINE_OK);

#if defined(__LB_SHELL__) || defined(COBALT)
    TRACE_EVENT0("media_stack", "VideoRendererBase paint_cb_");
#endif
    base::AutoUnlock ul(lock_);
    paint_cb_.Run();
  }
}

void VideoRendererBase::AddReadyFrame(const scoped_refptr<VideoFrame>& frame) {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT1("media_stack", "VideoRendererBase::AddReadyFrame()",
               "timestamp", frame->GetTimestamp().InMicroseconds());
#endif
  // Adjust the incoming frame if its rendering stop time is past the duration
  // of the video itself. This is typically the last frame of the video and
  // occurs if the container specifies a duration that isn't a multiple of the
  // frame rate.  Another way for this to happen is for the container to state a
  // smaller duration than the largest packet timestamp.
  base::TimeDelta duration = get_duration_cb_.Run();
  if (frame->IsEndOfStream()) {
    base::TimeDelta end_timestamp = kNoTimestamp();
    if (!ready_frames_.empty()) {
      end_timestamp = std::min(
          duration,
          ready_frames_.back()->GetTimestamp() + kMaxLastFrameDuration());
    } else if (current_frame_) {
      end_timestamp =
          std::min(duration,
                   current_frame_->GetTimestamp() + kMaxLastFrameDuration());
    }
    frame->SetTimestamp(end_timestamp);
  } else if (frame->GetTimestamp() > duration) {
    frame->SetTimestamp(duration);
  }

#if !defined(__LB_SHELL__FOR_RELEASE__)
  if (frame->GetTimestamp() < get_time_cb_.Run()) {
    SCOPED_MEDIA_STATISTICS(STAT_TYPE_VIDEO_FRAME_LATE);
    ++late_frames_;
  }
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)

  ready_frames_.push_back(frame);
#if defined(__LB_SHELL__) || defined(COBALT)
  DCHECK_LE(NumFrames_Locked(),
            ShellMediaPlatform::Instance()->GetMaxVideoFrames());
#else  // defined(__LB_SHELL__) || defined(COBALT)
  DCHECK_LE(NumFrames_Locked(), limits::kMaxVideoFrames);
#endif  // defined(__LB_SHELL__) || defined(COBALT)

  base::TimeDelta max_clock_time =
      frame->IsEndOfStream() ? duration : frame->GetTimestamp();
  DCHECK(max_clock_time != kNoTimestamp());
  max_time_cb_.Run(max_clock_time);

  frame_available_.Signal();
}

void VideoRendererBase::AttemptRead() {
  base::AutoLock auto_lock(lock_);
  AttemptRead_Locked();
}

void VideoRendererBase::AttemptRead_Locked() {
#if defined(__LB_SHELL__) || defined(COBALT)
  TRACE_EVENT0("media_stack", "VideoRendererBase::AttemptRead_Locked()");
#endif
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (pending_read_ ||
#if defined(__LB_SHELL__) || defined(COBALT)
      NumFrames_Locked() ==
          ShellMediaPlatform::Instance()->GetMaxVideoFrames() ||
#else  // defined(__LB_SHELL__) || defined(COBALT)
      NumFrames_Locked() == limits::kMaxVideoFrames ||
#endif  // defined(__LB_SHELL__) || defined(COBALT)
      (!ready_frames_.empty() && ready_frames_.back()->IsEndOfStream())) {
    return;
  }

  UpdateUnderflowStatusToDecoder_Locked();

  switch (state_) {
    case kPaused:
    case kFlushing:
    case kPrerolling:
    case kPlaying:
      pending_read_ = true;
      decoder_->Read(base::Bind(&VideoRendererBase::FrameReady, this));
      return;

    case kUninitialized:
    case kInitializing:
    case kPrerolled:
    case kFlushingDecoder:
    case kFlushed:
    case kEnded:
    case kStopped:
    case kError:
      return;
  }
}

void VideoRendererBase::UpdateUnderflowStatusToDecoder_Locked() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (!decoder_) {
    return;
  }
  if (state_ != kPlaying) {
    // If we are not playing, inform the decoder that we have enough frames so
    // it will decode in high quality.
    decoder_->HaveEnoughFrames();
    return;
  }

  // There are two stages during playback: prerolling and playing.  In the
  // following example, we assume the preroll frame count is 4, the underflow
  // frame count is 16 and the maximum frame count is 32.
  // 1. Prerolling:
  // When the video starts to play back, there will be 4 frames buffered.  Then
  // it will slowly grow up to 32 frames if the decoding speed is fast enough.
  // When the cached frame count start to decrease consistently, there is an
  // overflow.  Note that we cannot simply compare the current cached frame
  // against GetVideoUnderflowFrames() because as long as we are accumulating
  // more frames, we don't want to trigger underflow during start up. So we
  // record the maximum frame ever reached and when the cached frame count
  // drops below `maximum frame count` - kUnderflowAdjustment, we trigger
  // underflow.
  // 2. Playing.
  // In this case we already have maximum frames in cache so we have more room
  // for slow decoding.  We only trigger underflow when the cached frame count
  // drops below GetVideoUnderflowFrames().
  const int kUnderflowAdjustment = 2;
  maximum_frames_cached_ = std::max(maximum_frames_cached_, NumFrames_Locked());
  int underflow_threshold = std::min(
      ShellMediaPlatform::Instance()->GetVideoUnderflowFrames(),
      std::max(maximum_frames_cached_ - kUnderflowAdjustment, 0));

  if (NumFrames_Locked() < underflow_threshold) {
    decoder_->NearlyUnderflow();
  } else {
    decoder_->HaveEnoughFrames();
  }
}

void VideoRendererBase::OnDecoderResetDone() {
  base::AutoLock auto_lock(lock_);
  if (state_ == kStopped)
    return;

  DCHECK_EQ(kFlushingDecoder, state_);
  DCHECK(!pending_read_);

  state_ = kFlushing;
  AttemptFlush_Locked();
}

void VideoRendererBase::AttemptFlush_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(kFlushing, state_);

  prerolling_delayed_frame_ = NULL;
  ready_frames_.clear();
  maximum_frames_cached_ = 0;

#if defined(__LB_SHELL__) || defined(COBALT)
  if (frame_provider_) { frame_provider_->Flush(); }
#endif  // defined(__LB_SHELL__) || defined(COBALT)

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
  base::TimeDelta now = get_time_cb_.Run();
  base::TimeDelta next_pts = next_frame->GetTimestamp();

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
  if (frame_provider_) {
    return frame_provider_->GetNumOfFramesCached();
  }

  int outstanding_frames =
      (current_frame_ ? 1 : 0) + (last_available_frame_ ? 1 : 0) +
      (current_frame_ && (current_frame_ == last_available_frame_) ? -1 : 0);
  return ready_frames_.size() + outstanding_frames;
}

}  // namespace media
