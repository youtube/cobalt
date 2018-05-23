// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/clock.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#if defined(__LB_SHELL__) || defined(COBALT)
#include "media/base/shell_media_platform.h"
#include "media/base/shell_media_statistics.h"
#include "media/base/shell_video_frame_provider.h"
#endif  // defined(__LB_SHELL__) || defined(COBALT)
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_renderer.h"

using base::TimeDelta;

namespace media {

PipelineStatusNotification::PipelineStatusNotification()
    : cv_(&lock_), status_(PIPELINE_OK), notified_(false) {
}

PipelineStatusNotification::~PipelineStatusNotification() {
  DCHECK(notified_);
}

PipelineStatusCB PipelineStatusNotification::Callback() {
  return base::Bind(&PipelineStatusNotification::Notify,
                    base::Unretained(this));
}

void PipelineStatusNotification::Notify(media::PipelineStatus status) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!notified_);
  notified_ = true;
  status_ = status;
  cv_.Signal();
}

void PipelineStatusNotification::Wait() {
  base::AutoLock auto_lock(lock_);
  while (!notified_)
    cv_.Wait();
}

media::PipelineStatus PipelineStatusNotification::status() {
  base::AutoLock auto_lock(lock_);
  DCHECK(notified_);
  return status_;
}

scoped_refptr<Pipeline> Pipeline::Create(
    PipelineWindow window,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    MediaLog* media_log) {
  UNREFERENCED_PARAMETER(window);
  return new PipelineImpl(message_loop, media_log);
}

PipelineImpl::PipelineImpl(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    MediaLog* media_log)
    : message_loop_(message_loop),
      media_log_(media_log),
      running_(false),
      did_loading_progress_(false),
      total_bytes_(0),
      natural_size_(0, 0),
      volume_(1.0f),
      playback_rate_(0.0f),
      clock_(new Clock(&base::Time::Now)),
      waiting_for_clock_update_(false),
      status_(PIPELINE_OK),
      has_audio_(false),
      has_video_(false),
      state_(kCreated),
      audio_ended_(false),
      video_ended_(false),
      audio_disabled_(false),
      creation_time_(base::Time::Now()) {
  media_log_->AddEvent(
      media_log_->CreatePipelineStateChangedEvent(GetStateString(kCreated)));
  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::PIPELINE_CREATED));
}

PipelineImpl::~PipelineImpl() {
  // TODO(scherkus): Reenable after figuring out why this is firing, see
  // http://crbug.com/148405
#if 0
  DCHECK(thread_checker_.CalledOnValidThread())
      << "PipelineImpl must be destroyed on same thread that created it";
#endif
  DCHECK(!running_) << "Stop() must complete before destroying object";
  DCHECK(stop_cb_.is_null());
  DCHECK(seek_cb_.is_null());

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::PIPELINE_DESTROYED));
}

void PipelineImpl::Suspend() {
  // PipelineImpl::Suspend() is only called during quitting.  It is blocking
  // and may take a long time.
  base::WaitableEvent waiter(false, false);
  DLOG(INFO) << "Trying to stop media pipeline.";
  Stop(base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
  waiter.Wait();
  DLOG(INFO) << "Media pipeline suspended.";
}

void PipelineImpl::Resume() {
  // PipelineImpl doesn't support Resume().
  NOTREACHED();
}

void PipelineImpl::Start(scoped_ptr<FilterCollection> collection,
                         const SetDecryptorReadyCB& decryptor_ready_cb,
                         const PipelineStatusCB& ended_cb,
                         const PipelineStatusCB& error_cb,
                         const PipelineStatusCB& seek_cb,
                         const BufferingStateCB& buffering_state_cb,
                         const base::Closure& duration_change_cb) {
  DCHECK_EQ(collection->GetAudioDecoders()->size(), 1);
  DCHECK_EQ(collection->GetVideoDecoders()->size(), 1);

  base::AutoLock auto_lock(lock_);
  CHECK(!running_) << "Media pipeline is already running";
  DCHECK(!buffering_state_cb.is_null());

  running_ = true;
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::StartTask, this,
                            base::Passed(&collection), ended_cb, error_cb,
                            seek_cb, buffering_state_cb, duration_change_cb));
}

void PipelineImpl::Stop(const base::Closure& stop_cb) {
  base::AutoLock auto_lock(lock_);
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&PipelineImpl::StopTask, this, stop_cb));
}

void PipelineImpl::Seek(TimeDelta time, const PipelineStatusCB& seek_cb) {
  base::AutoLock auto_lock(lock_);
  if (running_) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&PipelineImpl::SeekTask, this, time, seek_cb));
  } else {
    // This callback will be silently ignored if there is a reload as the
    // PipelineImpl will be killed in that case. This is acceptable since the
    // app
    // needn't rely on this callback.
    message_loop_->PostTask(
        FROM_HERE, base::Bind(seek_cb, PIPELINE_ERROR_INVALID_STATE));
  }
}

bool PipelineImpl::IsRunning() const {
  base::AutoLock auto_lock(lock_);
  return running_;
}

bool PipelineImpl::HasAudio() const {
  base::AutoLock auto_lock(lock_);
  return has_audio_;
}

bool PipelineImpl::HasVideo() const {
  base::AutoLock auto_lock(lock_);
  return has_video_;
}

float PipelineImpl::GetPlaybackRate() const {
  base::AutoLock auto_lock(lock_);
  return playback_rate_;
}

void PipelineImpl::SetPlaybackRate(float playback_rate) {
  if (playback_rate < 0.0f)
    return;

  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
  if (running_) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&PipelineImpl::PlaybackRateChangedTask, this,
                              playback_rate));
  }
}

float PipelineImpl::GetVolume() const {
  base::AutoLock auto_lock(lock_);
  return volume_;
}

void PipelineImpl::SetVolume(float volume) {
  if (volume < 0.0f || volume > 1.0f)
    return;

  base::AutoLock auto_lock(lock_);
  volume_ = volume;
  if (running_) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&PipelineImpl::VolumeChangedTask, this, volume));
  }
}

TimeDelta PipelineImpl::GetMediaTime() const {
  base::AutoLock auto_lock(lock_);
  return clock_->Elapsed();
}

Ranges<TimeDelta> PipelineImpl::GetBufferedTimeRanges() {
  base::AutoLock auto_lock(lock_);
  Ranges<TimeDelta> time_ranges;
  for (size_t i = 0; i < buffered_time_ranges_.size(); ++i) {
    time_ranges.Add(buffered_time_ranges_.start(i),
                    buffered_time_ranges_.end(i));
  }
  if (clock_->Duration() == TimeDelta() || total_bytes_ == 0)
    return time_ranges;
  for (size_t i = 0; i < buffered_byte_ranges_.size(); ++i) {
    TimeDelta start = TimeForByteOffset_Locked(buffered_byte_ranges_.start(i));
    TimeDelta end = TimeForByteOffset_Locked(buffered_byte_ranges_.end(i));
    // Cap approximated buffered time at the length of the video.
    end = std::min(end, clock_->Duration());
    time_ranges.Add(start, end);
  }

  return time_ranges;
}

TimeDelta PipelineImpl::GetMediaDuration() const {
  base::AutoLock auto_lock(lock_);
  return clock_->Duration();
}

int64 PipelineImpl::GetTotalBytes() const {
  base::AutoLock auto_lock(lock_);
  return total_bytes_;
}

void PipelineImpl::GetNaturalVideoSize(gfx::Size* out_size) const {
  CHECK(out_size);
  base::AutoLock auto_lock(lock_);
  *out_size = natural_size_;
}

bool PipelineImpl::DidLoadingProgress() const {
  base::AutoLock auto_lock(lock_);
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

PipelineStatistics PipelineImpl::GetStatistics() const {
  base::AutoLock auto_lock(lock_);
  return statistics_;
}

void PipelineImpl::SetClockForTesting(Clock* clock) {
  clock_.reset(clock);
}

void PipelineImpl::SetErrorForTesting(PipelineStatus status) {
  SetError(status);
}

void PipelineImpl::SetState(State next_state) {
  if (state_ != kStarted && next_state == kStarted &&
      !creation_time_.is_null()) {
    UMA_HISTOGRAM_TIMES(
        "Media.TimeToPipelineStarted", base::Time::Now() - creation_time_);
    creation_time_ = base::Time();
  }

  DVLOG(2) << GetStateString(state_) << " -> " << GetStateString(next_state);

  state_ = next_state;
  media_log_->AddEvent(
      media_log_->CreatePipelineStateChangedEvent(GetStateString(next_state)));
}

#define RETURN_STRING(state) case state: return #state;

const char* PipelineImpl::GetStateString(State state) {
  switch (state) {
    RETURN_STRING(kCreated);
    RETURN_STRING(kInitDemuxer);
    RETURN_STRING(kInitAudioRenderer);
    RETURN_STRING(kInitVideoRenderer);
    RETURN_STRING(kInitPrerolling);
    RETURN_STRING(kSeeking);
    RETURN_STRING(kStarting);
    RETURN_STRING(kStarted);
    RETURN_STRING(kStopping);
    RETURN_STRING(kStopped);
  }
  NOTREACHED();
  return "INVALID";
}

#undef RETURN_STRING

PipelineImpl::State PipelineImpl::GetNextState() const {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null())
      << "State transitions don't happen when stopping";
  DCHECK_EQ(status_, PIPELINE_OK)
      << "State transitions don't happen when there's an error: " << status_;

  switch (state_) {
    case kCreated:
      return kInitDemuxer;

    case kInitDemuxer:
      if (demuxer_->GetStream(DemuxerStream::AUDIO))
        return kInitAudioRenderer;
      if (demuxer_->GetStream(DemuxerStream::VIDEO))
        return kInitVideoRenderer;
      return kInitPrerolling;

    case kInitAudioRenderer:
      if (demuxer_->GetStream(DemuxerStream::VIDEO))
        return kInitVideoRenderer;
      return kInitPrerolling;

    case kInitVideoRenderer:
      return kInitPrerolling;

    case kInitPrerolling:
      return kStarting;

    case kSeeking:
      return kStarting;

    case kStarting:
      return kStarted;

    case kStarted:
    case kStopping:
    case kStopped:
      break;
  }
  NOTREACHED() << "State has no transition: " << state_;
  return state_;
}

void PipelineImpl::OnDemuxerError(PipelineStatus error) {
  SetError(error);
}

void PipelineImpl::SetError(PipelineStatus error) {
  DCHECK(IsRunning());
  DCHECK_NE(PIPELINE_OK, error);
  VLOG(1) << "Media pipeline error: " << error;

  message_loop_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::ErrorChangedTask, this, error));

  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(error));
}

void PipelineImpl::OnAudioDisabled() {
  DCHECK(IsRunning());
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&PipelineImpl::AudioDisabledTask, this));
  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::AUDIO_RENDERER_DISABLED));
}

void PipelineImpl::OnAudioTimeUpdate(TimeDelta time, TimeDelta max_time) {
  DCHECK_LE(time.InMicroseconds(), max_time.InMicroseconds());
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);

  if (!has_audio_)
    return;
  if (waiting_for_clock_update_ && time < clock_->Elapsed())
    return;

  // TODO(scherkus): |state_| should only be accessed on pipeline thread, see
  // http://crbug.com/137973
  if (state_ == kSeeking)
    return;

  clock_->SetTime(time, max_time);
  StartClockIfWaitingForTimeUpdate_Locked();
}

void PipelineImpl::OnVideoTimeUpdate(TimeDelta max_time) {
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);

  if (has_audio_)
    return;

  // TODO(scherkus): |state_| should only be accessed on pipeline thread, see
  // http://crbug.com/137973
  if (state_ == kSeeking)
    return;

  DCHECK(!waiting_for_clock_update_);
  clock_->SetMaxTime(max_time);
}

void PipelineImpl::SetDuration(TimeDelta duration) {
  media_log_->AddEvent(
      media_log_->CreateTimeEvent(
          MediaLogEvent::DURATION_SET, "duration", duration));
  UMA_HISTOGRAM_LONG_TIMES("Media.Duration", duration);

  base::AutoLock auto_lock(lock_);
  clock_->SetDuration(duration);
  if (!duration_change_cb_.is_null())
    duration_change_cb_.Run();
}

void PipelineImpl::SetTotalBytes(int64 total_bytes) {
  DCHECK(IsRunning());
  media_log_->AddEvent(
      media_log_->CreateStringEvent(
          MediaLogEvent::TOTAL_BYTES_SET, "total_bytes",
          base::Int64ToString(total_bytes)));
  int64 total_mbytes = total_bytes >> 20;
  if (total_mbytes > kint32max)
    total_mbytes = kint32max;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Media.TotalMBytes", static_cast<int32>(total_mbytes), 1, kint32max, 50);

  base::AutoLock auto_lock(lock_);
  total_bytes_ = total_bytes;
}

TimeDelta PipelineImpl::TimeForByteOffset_Locked(int64 byte_offset) const {
  lock_.AssertAcquired();
  TimeDelta time_offset = byte_offset * clock_->Duration() / total_bytes_;
  // Since the byte->time calculation is approximate, fudge the beginning &
  // ending areas to look better.
  TimeDelta epsilon = clock_->Duration() / 100;
  if (time_offset < epsilon)
    return TimeDelta();
  if (time_offset + epsilon > clock_->Duration())
    return clock_->Duration();
  return time_offset;
}

void PipelineImpl::OnStateTransition(PipelineStatus status) {
  // Force post to process state transitions after current execution frame.
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::StateTransitionTask, this, status));
}

void PipelineImpl::StateTransitionTask(PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // No-op any state transitions if we're stopping.
  if (state_ == kStopping || state_ == kStopped)
    return;

  // Preserve existing abnormal status, otherwise update based on the result of
  // the previous operation.
  status_ = (status_ != PIPELINE_OK ? status_ : status);

  if (status_ != PIPELINE_OK) {
    ErrorChangedTask(status_);
    return;
  }

  // Guard against accidentally clearing |pending_callbacks_| for states that
  // use it as well as states that should not be using it.
  //
  // TODO(scherkus): Make every state transition use |pending_callbacks_|.
  DCHECK_EQ(pending_callbacks_.get() != NULL,
            (state_ == kInitPrerolling || state_ == kStarting ||
             state_ == kSeeking));
  pending_callbacks_.reset();

  PipelineStatusCB done_cb = base::Bind(&PipelineImpl::OnStateTransition, this);

  // Switch states, performing any entrance actions for the new state as well.
  SetState(GetNextState());
  switch (state_) {
    case kInitDemuxer:
      return InitializeDemuxer(done_cb);

    case kInitAudioRenderer:
      return InitializeAudioRenderer(done_cb);

    case kInitVideoRenderer:
      return InitializeVideoRenderer(done_cb);

    case kInitPrerolling:
      filter_collection_.reset();
      {
        base::AutoLock l(lock_);
        // We do not want to start the clock running. We only want to set the
        // base media time so our timestamp calculations will be correct.
        clock_->SetTime(demuxer_->GetStartTime(), demuxer_->GetStartTime());

        // TODO(scherkus): |has_audio_| should be true no matter what --
        // otherwise people with muted/disabled sound cards will make our
        // default controls look as if every video doesn't contain an audio
        // track.
        has_audio_ = audio_renderer_ != NULL && !audio_disabled_;
        has_video_ = video_renderer_ != NULL;
      }
      if (!audio_renderer_ && !video_renderer_) {
        done_cb.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
        return;
      }

      buffering_state_cb_.Run(kHaveMetadata);

      return DoInitialPreroll(done_cb);

    case kStarting:
      return DoPlay(done_cb);

    case kStarted:
#if defined(__LB_SHELL__) || defined(COBALT)
      ShellMediaStatistics::Instance().OnPlaybackBegin();
#endif  // defined(__LB_SHELL__) || defined(COBALT)
      {
        base::AutoLock l(lock_);
        // We use audio stream to update the clock. So if there is such a
        // stream, we pause the clock until we receive a valid timestamp.
        waiting_for_clock_update_ = true;
        if (!has_audio_) {
          clock_->SetMaxTime(clock_->Duration());
          StartClockIfWaitingForTimeUpdate_Locked();
        }
      }

      DCHECK(!seek_cb_.is_null());
      DCHECK_EQ(status_, PIPELINE_OK);

      // Fire canplaythrough immediately after playback begins because of
      // crbug.com/106480.
      // TODO(vrk): set ready state to HaveFutureData when bug above is fixed.
      buffering_state_cb_.Run(kPrerollCompleted);
      return base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);

    case kStopping:
    case kStopped:
    case kCreated:
    case kSeeking:
      NOTREACHED() << "State has no transition: " << state_;
      return;
  }
}

void PipelineImpl::DoInitialPreroll(const PipelineStatusCB& done_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  base::TimeDelta seek_timestamp = demuxer_->GetStartTime();

  // Preroll renderers.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Preroll, audio_renderer_, seek_timestamp));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Preroll, video_renderer_, seek_timestamp));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void PipelineImpl::DoSeek(base::TimeDelta seek_timestamp,
                          const PipelineStatusCB& done_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  // Pause.
  if (audio_renderer_)
    bound_fns.Push(base::Bind(&AudioRenderer::Pause, audio_renderer_));
  if (video_renderer_)
    bound_fns.Push(base::Bind(&VideoRenderer::Pause, video_renderer_));

  // Flush.
  if (audio_renderer_)
    bound_fns.Push(base::Bind(&AudioRenderer::Flush, audio_renderer_));
  if (video_renderer_)
    bound_fns.Push(base::Bind(&VideoRenderer::Flush, video_renderer_));

  // Seek demuxer.
  bound_fns.Push(base::Bind(
      &Demuxer::Seek, demuxer_, seek_timestamp));

  // Preroll renderers.
  if (audio_renderer_) {
    bound_fns.Push(base::Bind(
        &AudioRenderer::Preroll, audio_renderer_, seek_timestamp));
  }

  if (video_renderer_) {
    bound_fns.Push(base::Bind(
        &VideoRenderer::Preroll, video_renderer_, seek_timestamp));
  }

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void PipelineImpl::DoPlay(const PipelineStatusCB& done_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  PlaybackRateChangedTask(GetPlaybackRate());
  VolumeChangedTask(GetVolume());

  if (audio_renderer_)
    bound_fns.Push(base::Bind(&AudioRenderer::Play, audio_renderer_));

  if (video_renderer_)
    bound_fns.Push(base::Bind(&VideoRenderer::Play, video_renderer_));

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void PipelineImpl::DoStop(const PipelineStatusCB& done_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!pending_callbacks_.get());
  SerialRunner::Queue bound_fns;

  if (demuxer_)
    bound_fns.Push(base::Bind(&Demuxer::Stop, demuxer_));

  if (audio_renderer_)
    bound_fns.Push(base::Bind(&AudioRenderer::Stop, audio_renderer_));

  if (video_renderer_)
    bound_fns.Push(base::Bind(&VideoRenderer::Stop, video_renderer_));

  pending_callbacks_ = SerialRunner::Run(bound_fns, done_cb);
}

void PipelineImpl::OnStopCompleted(PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);
  {
    base::AutoLock l(lock_);
    running_ = false;
  }

  SetState(kStopped);
  pending_callbacks_.reset();
  filter_collection_.reset();
  audio_renderer_ = NULL;
  video_renderer_ = NULL;
  demuxer_ = NULL;

  // If we stop during initialization/seeking we want to run |seek_cb_|
  // followed by |stop_cb_| so we don't leave outstanding callbacks around.
  if (!seek_cb_.is_null()) {
    base::ResetAndReturn(&seek_cb_).Run(status_);
    error_cb_.Reset();
  }
  if (!stop_cb_.is_null()) {
    base::ResetAndReturn(&stop_cb_).Run();
    error_cb_.Reset();
  }
  if (!error_cb_.is_null()) {
    DCHECK_NE(status_, PIPELINE_OK);
    base::ResetAndReturn(&error_cb_).Run(status_);
  }
}

void PipelineImpl::AddBufferedByteRange(int64 start, int64 end) {
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);
  buffered_byte_ranges_.Add(start, end);
  did_loading_progress_ = true;
}

void PipelineImpl::AddBufferedTimeRange(base::TimeDelta start,
                                        base::TimeDelta end) {
  DCHECK(IsRunning());
  base::AutoLock auto_lock(lock_);
  buffered_time_ranges_.Add(start, end);
  did_loading_progress_ = true;
}

void PipelineImpl::OnNaturalVideoSizeChanged(const gfx::Size& size) {
  DCHECK(IsRunning());
  media_log_->AddEvent(media_log_->CreateVideoSizeSetEvent(
      size.width(), size.height()));

  base::AutoLock auto_lock(lock_);
  natural_size_ = size;
}

void PipelineImpl::OnAudioRendererEnded() {
  // Force post to process ended messages after current execution frame.
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::DoAudioRendererEnded, this));
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::AUDIO_ENDED));
}

void PipelineImpl::OnVideoRendererEnded() {
  // Force post to process ended messages after current execution frame.
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&PipelineImpl::DoVideoRendererEnded, this));
  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::VIDEO_ENDED));
}

// Called from any thread.
void PipelineImpl::OnUpdateStatistics(const PipelineStatistics& stats) {
  base::AutoLock auto_lock(lock_);
  statistics_.audio_bytes_decoded += stats.audio_bytes_decoded;
  statistics_.video_bytes_decoded += stats.video_bytes_decoded;
  statistics_.video_frames_decoded += stats.video_frames_decoded;
  statistics_.video_frames_dropped += stats.video_frames_dropped;
}

void PipelineImpl::StartTask(scoped_ptr<FilterCollection> filter_collection,
                             const PipelineStatusCB& ended_cb,
                             const PipelineStatusCB& error_cb,
                             const PipelineStatusCB& seek_cb,
                             const BufferingStateCB& buffering_state_cb,
                             const base::Closure& duration_change_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Suspend() can be called from a thread other than the WebModule thread.  In
  // such case it is possible that Stop() is called before Start() and |state_|
  // will be set to |kStopping| or |kStopped|.
  if (state_ == kStopping || state_ == kStopped) {
    return;
  }

  // Check for individual state so we have more clue when one of them is hit.
  CHECK_NE(kInitDemuxer, state_) << "state shouldn't be kInitDemuxer";
  CHECK_NE(kInitAudioRenderer, state_)
      << "state shouldn't be kInitAudioRenderer";
  CHECK_NE(kInitVideoRenderer, state_)
      << "state shouldn't be kInitVideoRenderer";
  CHECK_NE(kInitPrerolling, state_) << "state shouldn't be kInitPrerolling";
  CHECK_NE(kSeeking, state_) << "state shouldn't be kSeeking";
  CHECK_NE(kStarting, state_) << "state shouldn't be kStarting";
  CHECK_NE(kStarted, state_) << "state shouldn't be kStarted";

  CHECK_EQ(kCreated, state_)
      << "Media pipeline cannot be started more than once";

  filter_collection_ = filter_collection.Pass();
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;
  seek_cb_ = seek_cb;
  buffering_state_cb_ = buffering_state_cb;
  duration_change_cb_ = duration_change_cb;

  StateTransitionTask(PIPELINE_OK);
}

void PipelineImpl::StopTask(const base::Closure& stop_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  if (state_ == kStopped) {
    stop_cb.Run();
    return;
  }

  stop_cb_ = stop_cb;

  // We may already be stopping due to a runtime error.
  if (state_ == kStopping) {
    return;
  }

  SetState(kStopping);
  pending_callbacks_.reset();
  DoStop(base::Bind(&PipelineImpl::OnStopCompleted, this));
}

void PipelineImpl::ErrorChangedTask(PipelineStatus error) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(PIPELINE_OK, error) << "PIPELINE_OK isn't an error!";

  if (state_ == kStopping || state_ == kStopped)
    return;

  SetState(kStopping);
  pending_callbacks_.reset();
  status_ = error;

  DoStop(base::Bind(&PipelineImpl::OnStopCompleted, this));
}

void PipelineImpl::PlaybackRateChangedTask(float playback_rate) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Playback rate changes are only carried out while playing.
  if (state_ != kStarting && state_ != kStarted)
    return;

  {
    base::AutoLock auto_lock(lock_);
    clock_->SetPlaybackRate(playback_rate);
  }

  if (demuxer_)
    demuxer_->SetPlaybackRate(playback_rate);
  if (audio_renderer_)
    audio_renderer_->SetPlaybackRate(playback_rate_);
  if (video_renderer_)
    video_renderer_->SetPlaybackRate(playback_rate_);
}

void PipelineImpl::VolumeChangedTask(float volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Volume changes are only carried out while playing.
  if (state_ != kStarting && state_ != kStarted)
    return;

  if (audio_renderer_)
    audio_renderer_->SetVolume(volume);
}

void PipelineImpl::SeekTask(TimeDelta time, const PipelineStatusCB& seek_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stop_cb_.is_null());

  // Suppress seeking if we're not fully started.
  if (state_ != kStarted) {
    DCHECK(state_ == kStopping || state_ == kStopped)
        << "Receive extra seek in unexpected state: " << state_;

    // TODO(scherkus): should we run the callback?  I'm tempted to say the API
    // will only execute the first Seek() request.
    DVLOG(1) << "Media pipeline has not started, ignoring seek to "
             << time.InMicroseconds() << " (current state: " << state_ << ")";
    return;
  }

  DCHECK(seek_cb_.is_null());

  SetState(kSeeking);
  base::TimeDelta seek_timestamp = std::max(time, demuxer_->GetStartTime());
  seek_cb_ = seek_cb;
  audio_ended_ = false;
  video_ended_ = false;

  // Kick off seeking!
  {
    base::AutoLock auto_lock(lock_);
    if (clock_->IsPlaying())
      clock_->Pause();
    waiting_for_clock_update_ = false;
    clock_->SetTime(seek_timestamp, seek_timestamp);
  }
  DoSeek(seek_timestamp, base::Bind(&PipelineImpl::OnStateTransition, this));
}

void PipelineImpl::DoAudioRendererEnded() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ != kStarted)
    return;

  DCHECK(!audio_ended_);
  audio_ended_ = true;

  // Start clock since there is no more audio to trigger clock updates.
  if (!audio_disabled_) {
    base::AutoLock auto_lock(lock_);
    clock_->SetMaxTime(clock_->Duration());
    StartClockIfWaitingForTimeUpdate_Locked();
  }

  RunEndedCallbackIfNeeded();
}

void PipelineImpl::DoVideoRendererEnded() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ != kStarted)
    return;

  DCHECK(!video_ended_);
  video_ended_ = true;

  RunEndedCallbackIfNeeded();
}

void PipelineImpl::RunEndedCallbackIfNeeded() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (audio_renderer_ && !audio_ended_ && !audio_disabled_)
    return;

  if (video_renderer_ && !video_ended_)
    return;

  {
    base::AutoLock auto_lock(lock_);
    clock_->EndOfStream();
  }

  DLOG(INFO) << "video playback completed successfully! :)";

  // TODO(scherkus): Change |ended_cb_| into a Closure.
  DCHECK_EQ(status_, PIPELINE_OK);
  ended_cb_.Run(status_);
}

void PipelineImpl::AudioDisabledTask() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  has_audio_ = false;
  audio_disabled_ = true;

  // Notify our demuxer that we're no longer rendering audio.
  demuxer_->OnAudioRendererDisabled();

  // Start clock since there is no more audio to trigger clock updates.
  clock_->SetMaxTime(clock_->Duration());
  StartClockIfWaitingForTimeUpdate_Locked();
}

void PipelineImpl::InitializeDemuxer(const PipelineStatusCB& done_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  demuxer_ = filter_collection_->GetDemuxer();
  demuxer_->Initialize(this, done_cb);
}

void PipelineImpl::InitializeAudioRenderer(const PipelineStatusCB& done_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  scoped_refptr<DemuxerStream> stream =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  DCHECK(stream);

  filter_collection_->SelectAudioRenderer(&audio_renderer_);
  audio_renderer_->Initialize(
      stream, *filter_collection_->GetAudioDecoders(), done_cb,
      base::Bind(&PipelineImpl::OnUpdateStatistics, this),
      base::Bind(&PipelineImpl::OnAudioUnderflow, this),
      base::Bind(&PipelineImpl::OnAudioTimeUpdate, this),
      base::Bind(&PipelineImpl::OnAudioRendererEnded, this),
      base::Bind(&PipelineImpl::OnAudioDisabled, this),
      base::Bind(&PipelineImpl::SetError, this));
  filter_collection_->GetAudioDecoders()->clear();
}

void PipelineImpl::InitializeVideoRenderer(const PipelineStatusCB& done_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  scoped_refptr<DemuxerStream> stream =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  DCHECK(stream);

  {
    // Get an initial natural size so we have something when we signal
    // the kHaveMetadata buffering state.
    base::AutoLock l(lock_);
    natural_size_ = stream->video_decoder_config().natural_size();
  }

  filter_collection_->SelectVideoRenderer(&video_renderer_);
  video_renderer_->Initialize(
      stream, *filter_collection_->GetVideoDecoders(), done_cb,
      base::Bind(&PipelineImpl::OnUpdateStatistics, this),
      base::Bind(&PipelineImpl::OnVideoTimeUpdate, this),
      base::Bind(&PipelineImpl::OnNaturalVideoSizeChanged, this),
      base::Bind(&PipelineImpl::OnVideoRendererEnded, this),
      base::Bind(&PipelineImpl::SetError, this),
      base::Bind(&PipelineImpl::GetMediaTime, this),
      base::Bind(&PipelineImpl::GetMediaDuration, this));
  filter_collection_->GetVideoDecoders()->clear();
}

void PipelineImpl::OnAudioUnderflow() {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&PipelineImpl::OnAudioUnderflow, this));
    return;
  }

  if (state_ != kStarted)
    return;

  if (audio_renderer_)
    audio_renderer_->ResumeAfterUnderflow(true);
}

void PipelineImpl::StartClockIfWaitingForTimeUpdate_Locked() {
  lock_.AssertAcquired();
  if (!waiting_for_clock_update_)
    return;

  waiting_for_clock_update_ = false;
  clock_->Play();
}

}  // namespace media
