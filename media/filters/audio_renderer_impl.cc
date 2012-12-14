// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_impl.h"

#include <math.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/audio/audio_util.h"
#include "media/base/audio_splicer.h"
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/filters/audio_decoder_selector.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

AudioRendererImpl::AudioRendererImpl(
    media::AudioRendererSink* sink,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : sink_(sink),
      set_decryptor_ready_cb_(set_decryptor_ready_cb),
      state_(kUninitialized),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      audio_time_buffered_(kNoTimestamp()),
      current_time_(kNoTimestamp()),
      underflow_disabled_(false),
      preroll_aborted_(false),
      actual_frames_per_buffer_(0) {
  // We're created on the render thread, but this thread checker is for another.
  pipeline_thread_checker_.DetachFromThread();
}

void AudioRendererImpl::Play(const base::Closure& callback) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());

  float playback_rate = 0;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(kPaused, state_);
    state_ = kPlaying;
    callback.Run();
    playback_rate = algorithm_->playback_rate();
  }

  if (playback_rate != 0.0f) {
    DoPlay();
  } else {
    DoPause();
  }
}

void AudioRendererImpl::DoPlay() {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  DCHECK(sink_);
  {
    base::AutoLock auto_lock(lock_);
    earliest_end_time_ = base::Time::Now();
  }
  sink_->Play();
}

void AudioRendererImpl::Pause(const base::Closure& callback) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(state_ == kPlaying || state_ == kUnderflow ||
           state_ == kRebuffering);
    pause_cb_ = callback;
    state_ = kPaused;

    // Pause only when we've completed our pending read.
    if (!pending_read_)
      base::ResetAndReturn(&pause_cb_).Run();
  }

  DoPause();
}

void AudioRendererImpl::DoPause() {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  DCHECK(sink_);
  sink_->Pause(false);
}

void AudioRendererImpl::Flush(const base::Closure& callback) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());

  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(base::Bind(
        &AudioRendererImpl::ResetDecoder, this, callback));
    return;
  }

  decoder_->Reset(callback);
}

void AudioRendererImpl::ResetDecoder(const base::Closure& callback) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  decoder_->Reset(callback);
}

void AudioRendererImpl::Stop(const base::Closure& callback) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  if (sink_) {
    sink_->Stop();
    sink_ = NULL;
  }

  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;
    algorithm_.reset(NULL);
    init_cb_.Reset();
    underflow_cb_.Reset();
    time_cb_.Reset();
  }

  callback.Run();
}

void AudioRendererImpl::Preroll(base::TimeDelta time,
                                const PipelineStatusCB& cb) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  DCHECK(sink_);

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(kPaused, state_);
    DCHECK(!pending_read_) << "Pending read must complete before seeking";
    DCHECK(pause_cb_.is_null());
    DCHECK(preroll_cb_.is_null());
    state_ = kPrerolling;
    preroll_cb_ = cb;
    preroll_timestamp_ = time;

    // Throw away everything and schedule our reads.
    audio_time_buffered_ = kNoTimestamp();
    current_time_ = kNoTimestamp();
    received_end_of_stream_ = false;
    rendered_end_of_stream_ = false;
    preroll_aborted_ = false;

    splicer_->Reset();

    // |algorithm_| will request more reads.
    algorithm_->FlushBuffers();
    earliest_end_time_ = base::Time::Now();
  }

  // Pause and flush the stream when we preroll to a new location.
  sink_->Pause(true);
}

void AudioRendererImpl::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                   const AudioDecoderList& decoders,
                                   const PipelineStatusCB& init_cb,
                                   const StatisticsCB& statistics_cb,
                                   const base::Closure& underflow_cb,
                                   const TimeCB& time_cb,
                                   const base::Closure& ended_cb,
                                   const base::Closure& disabled_cb,
                                   const PipelineStatusCB& error_cb) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  DCHECK(stream);
  DCHECK(!decoders.empty());
  DCHECK_EQ(stream->type(), DemuxerStream::AUDIO);
  DCHECK(!init_cb.is_null());
  DCHECK(!statistics_cb.is_null());
  DCHECK(!underflow_cb.is_null());
  DCHECK(!time_cb.is_null());
  DCHECK(!ended_cb.is_null());
  DCHECK(!disabled_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  DCHECK(sink_);

  init_cb_ = init_cb;
  statistics_cb_ = statistics_cb;
  underflow_cb_ = underflow_cb;
  time_cb_ = time_cb;
  ended_cb_ = ended_cb;
  disabled_cb_ = disabled_cb;
  error_cb_ = error_cb;

  scoped_ptr<AudioDecoderSelector> decoder_selector(
      new AudioDecoderSelector(base::MessageLoopProxy::current(),
                               decoders,
                               set_decryptor_ready_cb_));

  // To avoid calling |decoder_selector| methods and passing ownership of
  // |decoder_selector| in the same line.
  AudioDecoderSelector* decoder_selector_ptr = decoder_selector.get();

  decoder_selector_ptr->SelectAudioDecoder(
      stream,
      statistics_cb,
      base::Bind(&AudioRendererImpl::OnDecoderSelected, this,
                 base::Passed(&decoder_selector)));
}

void AudioRendererImpl::OnDecoderSelected(
    scoped_ptr<AudioDecoderSelector> decoder_selector,
    const scoped_refptr<AudioDecoder>& selected_decoder,
    const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());

  if (state_ == kStopped) {
    DCHECK(!sink_);
    return;
  }

  if (!selected_decoder) {
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  decoder_ = selected_decoder;
  decrypting_demuxer_stream_ = decrypting_demuxer_stream;

  int sample_rate = decoder_->samples_per_second();
  int buffer_size = GetHighLatencyOutputBufferSize(sample_rate);
  AudioParameters::Format format = AudioParameters::AUDIO_PCM_LINEAR;

  // On Windows and Mac we can use the low latency pipeline because they provide
  // accurate and smooth delay information.  On other platforms like Linux there
  // are jitter issues.
  // TODO(dalecurtis): Fix bugs: http://crbug.com/138098 http://crbug.com/32757
#if defined(OS_WIN) || defined(OS_MAC)
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  // Either AudioOutputResampler or renderer side mixing must be enabled to use
  // the low latency pipeline.
  if (!cmd_line->HasSwitch(switches::kDisableRendererSideMixing) ||
      !cmd_line->HasSwitch(switches::kDisableAudioOutputResampler)) {
    // There are two cases here:
    //
    // 1. Renderer side mixing is enabled and the buffer size is actually
    //    controlled by the size of the AudioBus provided to Render().  In this
    //    case the buffer size below is ignored.
    //
    // 2. Renderer side mixing is disabled and AudioOutputResampler on the
    //    browser side is rebuffering to the hardware size on the fly.
    //
    // In the second case we need to choose a a buffer size small enough that
    // the decoder can fulfill the high frequency low latency audio callbacks,
    // but not so small that it's less than the hardware buffer size (or we'll
    // run into issues since the shared memory sync is non-blocking).
    //
    // The buffer size below is arbitrarily the same size used by Pepper Flash
    // for consistency.  Since renderer side mixing is only disabled for debug
    // purposes it's okay that this buffer size might lead to jitter since it's
    // not a multiple of the hardware buffer size.
    format = AudioParameters::AUDIO_PCM_LOW_LATENCY;
    buffer_size = 2048;
  }
#endif

  audio_parameters_ = AudioParameters(
      format, decoder_->channel_layout(), sample_rate,
      decoder_->bits_per_channel(), buffer_size);
  if (!audio_parameters_.IsValid()) {
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  int channels = ChannelLayoutToChannelCount(decoder_->channel_layout());
  int bytes_per_frame = channels * decoder_->bits_per_channel() / 8;
  splicer_.reset(new AudioSplicer(bytes_per_frame, sample_rate));

  // We're all good! Continue initializing the rest of the audio renderer based
  // on the decoder format.
  algorithm_.reset(new AudioRendererAlgorithm());
  algorithm_->Initialize(0, audio_parameters_, base::Bind(
      &AudioRendererImpl::ScheduleRead_Locked, this));

  state_ = kPaused;

  sink_->Initialize(audio_parameters_, this);
  sink_->Start();

  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void AudioRendererImpl::ResumeAfterUnderflow(bool buffer_more_audio) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  if (state_ == kUnderflow) {
    // The "&& preroll_aborted_" is a hack. If preroll is aborted, then we
    // shouldn't even reach the kUnderflow state to begin with. But for now
    // we're just making sure that the audio buffer capacity (i.e. the
    // number of bytes that need to be buffered for preroll to complete)
    // does not increase due to an aborted preroll.
    // TODO(vrk): Fix this bug correctly! (crbug.com/151352)
    if (buffer_more_audio && !preroll_aborted_)
      algorithm_->IncreaseQueueCapacity();

    state_ = kRebuffering;
  }
}

void AudioRendererImpl::SetVolume(float volume) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  DCHECK(sink_);
  sink_->SetVolume(volume);
}

AudioRendererImpl::~AudioRendererImpl() {
  // Stop() should have been called and |algorithm_| should have been destroyed.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(!algorithm_.get());
}

void AudioRendererImpl::DecodedAudioReady(AudioDecoder::Status status,
                                          const scoped_refptr<Buffer>& buffer) {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kPaused || state_ == kPrerolling || state_ == kPlaying ||
         state_ == kUnderflow || state_ == kRebuffering || state_ == kStopped);

  CHECK(pending_read_);
  pending_read_ = false;

  if (status == AudioDecoder::kAborted) {
    HandleAbortedReadOrDecodeError(false);
    return;
  }

  if (status == AudioDecoder::kDecodeError) {
    HandleAbortedReadOrDecodeError(true);
    return;
  }

  DCHECK_EQ(status, AudioDecoder::kOk);
  DCHECK(buffer);

  if (!splicer_->AddInput(buffer)) {
    HandleAbortedReadOrDecodeError(true);
    return;
  }

  if (!splicer_->HasNextBuffer()) {
    ScheduleRead_Locked();
    return;
  }

  bool need_another_buffer = false;
  while (splicer_->HasNextBuffer())
    need_another_buffer = HandleSplicerBuffer(splicer_->GetNextBuffer());

  if (!need_another_buffer)
    return;

  ScheduleRead_Locked();
}

bool AudioRendererImpl::HandleSplicerBuffer(
    const scoped_refptr<Buffer>& buffer) {
  if (buffer->IsEndOfStream()) {
    received_end_of_stream_ = true;

    // Transition to kPlaying if we are currently handling an underflow since
    // no more data will be arriving.
    if (state_ == kUnderflow || state_ == kRebuffering)
      state_ = kPlaying;
  }

  switch (state_) {
    case kUninitialized:
      NOTREACHED();
      return false;
    case kPaused:
      if (!buffer->IsEndOfStream())
        algorithm_->EnqueueBuffer(buffer);
      DCHECK(!pending_read_);
      base::ResetAndReturn(&pause_cb_).Run();
      return false;
    case kPrerolling:
      if (IsBeforePrerollTime(buffer))
        return true;

      if (!buffer->IsEndOfStream()) {
        algorithm_->EnqueueBuffer(buffer);
        if (!algorithm_->IsQueueFull())
          return false;
      }
      state_ = kPaused;
      base::ResetAndReturn(&preroll_cb_).Run(PIPELINE_OK);
      return false;
    case kPlaying:
    case kUnderflow:
    case kRebuffering:
      if (!buffer->IsEndOfStream())
        algorithm_->EnqueueBuffer(buffer);
      return false;
    case kStopped:
      return false;
  }
  return false;
}

void AudioRendererImpl::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  if (pending_read_ || state_ == kPaused)
    return;
  pending_read_ = true;
  decoder_->Read(base::Bind(&AudioRendererImpl::DecodedAudioReady, this));
}

void AudioRendererImpl::SetPlaybackRate(float playback_rate) {
  DCHECK(pipeline_thread_checker_.CalledOnValidThread());
  DCHECK_LE(0.0f, playback_rate);
  DCHECK(sink_);

  // We have two cases here:
  // Play: current_playback_rate == 0.0 && playback_rate != 0.0
  // Pause: current_playback_rate != 0.0 && playback_rate == 0.0
  float current_playback_rate = algorithm_->playback_rate();
  if (current_playback_rate == 0.0f && playback_rate != 0.0f) {
    DoPlay();
  } else if (current_playback_rate != 0.0f && playback_rate == 0.0f) {
    // Pause is easy, we can always pause.
    DoPause();
  }

  base::AutoLock auto_lock(lock_);
  algorithm_->SetPlaybackRate(playback_rate);
}

bool AudioRendererImpl::IsBeforePrerollTime(
    const scoped_refptr<Buffer>& buffer) {
  return (state_ == kPrerolling) && buffer && !buffer->IsEndOfStream() &&
      (buffer->GetTimestamp() + buffer->GetDuration()) < preroll_timestamp_;
}

int AudioRendererImpl::Render(AudioBus* audio_bus,
                              int audio_delay_milliseconds) {
  if (actual_frames_per_buffer_ != audio_bus->frames()) {
    audio_buffer_.reset(
        new uint8[audio_bus->frames() * audio_parameters_.GetBytesPerFrame()]);
    actual_frames_per_buffer_ = audio_bus->frames();
  }

  int frames_filled = FillBuffer(
      audio_buffer_.get(), audio_bus->frames(), audio_delay_milliseconds);
  DCHECK_LE(frames_filled, actual_frames_per_buffer_);

  // Deinterleave audio data into the output bus.
  audio_bus->FromInterleaved(
      audio_buffer_.get(), frames_filled,
      audio_parameters_.bits_per_sample() / 8);

  return frames_filled;
}

uint32 AudioRendererImpl::FillBuffer(uint8* dest,
                                     uint32 requested_frames,
                                     int audio_delay_milliseconds) {
  base::TimeDelta current_time = kNoTimestamp();
  base::TimeDelta max_time = kNoTimestamp();

  size_t frames_written = 0;
  base::Closure underflow_cb;
  {
    base::AutoLock auto_lock(lock_);

    // Ensure Stop() hasn't destroyed our |algorithm_| on the pipeline thread.
    if (!algorithm_)
      return 0;

    float playback_rate = algorithm_->playback_rate();
    if (playback_rate == 0.0f)
      return 0;

    // Adjust the delay according to playback rate.
    base::TimeDelta playback_delay =
        base::TimeDelta::FromMilliseconds(audio_delay_milliseconds);
    if (playback_rate != 1.0f) {
      playback_delay = base::TimeDelta::FromMicroseconds(static_cast<int64>(
          ceil(playback_delay.InMicroseconds() * playback_rate)));
    }

    if (state_ == kRebuffering && algorithm_->IsQueueFull())
      state_ = kPlaying;

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      // TODO(scherkus): To keep the audio hardware busy we write at most 8k of
      // zeros.  This gets around the tricky situation of pausing and resuming
      // the audio IPC layer in Chrome.  Ideally, we should return zero and then
      // the subclass can restart the conversation.
      //
      // This should get handled by the subclass http://crbug.com/106600
      const uint32 kZeroLength = 8192;
      size_t zeros_to_write = std::min(
          kZeroLength, requested_frames * audio_parameters_.GetBytesPerFrame());
      memset(dest, 0, zeros_to_write);
      return zeros_to_write / audio_parameters_.GetBytesPerFrame();
    }

    // We use the following conditions to determine end of playback:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We received an end of stream buffer
    //   3) We haven't already signalled that we've ended
    //   4) Our estimated earliest end time has expired
    //
    // TODO(enal): we should replace (4) with a check that the browser has no
    // more audio data or at least use a delayed callback.
    //
    // We use the following conditions to determine underflow:
    //   1) Algorithm can not fill the audio callback buffer
    //   2) We have NOT received an end of stream buffer
    //   3) We are in the kPlaying state
    //
    // Otherwise fill the buffer with whatever data we can send to the device.
    if (!algorithm_->CanFillBuffer() && received_end_of_stream_ &&
        !rendered_end_of_stream_ && base::Time::Now() >= earliest_end_time_) {
      rendered_end_of_stream_ = true;
      ended_cb_.Run();
    } else if (!algorithm_->CanFillBuffer() && !received_end_of_stream_ &&
               state_ == kPlaying && !underflow_disabled_) {
      state_ = kUnderflow;
      underflow_cb = underflow_cb_;
    } else if (algorithm_->CanFillBuffer()) {
      frames_written = algorithm_->FillBuffer(dest, requested_frames);
      DCHECK_GT(frames_written, 0u);
    } else {
      // We can't write any data this cycle. For example, we may have
      // sent all available data to the audio device while not reaching
      // |earliest_end_time_|.
    }

    // The |audio_time_buffered_| is the ending timestamp of the last frame
    // buffered at the audio device. |playback_delay| is the amount of time
    // buffered at the audio device. The current time can be computed by their
    // difference.
    if (audio_time_buffered_ != kNoTimestamp()) {
      base::TimeDelta previous_time = current_time_;
      current_time_ = audio_time_buffered_ - playback_delay;

      // Time can change in one of two ways:
      //   1) The time of the audio data at the audio device changed, or
      //   2) The playback delay value has changed
      //
      // We only want to set |current_time| (and thus execute |time_cb_|) if
      // time has progressed and we haven't signaled end of stream yet.
      //
      // Why? The current latency of the system results in getting the last call
      // to FillBuffer() later than we'd like, which delays firing the 'ended'
      // event, which delays the looping/trigging performance of short sound
      // effects.
      //
      // TODO(scherkus): revisit this and switch back to relying on playback
      // delay after we've revamped our audio IPC subsystem.
      if (current_time_ > previous_time && !rendered_end_of_stream_) {
        current_time = current_time_;
      }
    }

    // The call to FillBuffer() on |algorithm_| has increased the amount of
    // buffered audio data. Update the new amount of time buffered.
    max_time = algorithm_->GetTime();
    audio_time_buffered_ = max_time;

    UpdateEarliestEndTime_Locked(
        frames_written, playback_rate, playback_delay, base::Time::Now());
  }

  if (current_time != kNoTimestamp() && max_time != kNoTimestamp()) {
    time_cb_.Run(current_time, max_time);
  }

  if (!underflow_cb.is_null())
    underflow_cb.Run();

  return frames_written;
}

void AudioRendererImpl::UpdateEarliestEndTime_Locked(
    int frames_filled, float playback_rate, base::TimeDelta playback_delay,
    base::Time time_now) {
  if (frames_filled <= 0)
    return;

  base::TimeDelta predicted_play_time = base::TimeDelta::FromMicroseconds(
      static_cast<float>(frames_filled) * base::Time::kMicrosecondsPerSecond /
      audio_parameters_.sample_rate());

  if (playback_rate != 1.0f) {
    predicted_play_time = base::TimeDelta::FromMicroseconds(
        static_cast<int64>(ceil(predicted_play_time.InMicroseconds() *
                                playback_rate)));
  }

  lock_.AssertAcquired();
  earliest_end_time_ = std::max(
      earliest_end_time_, time_now + playback_delay + predicted_play_time);
}

void AudioRendererImpl::OnRenderError() {
  disabled_cb_.Run();
}

void AudioRendererImpl::DisableUnderflowForTesting() {
  underflow_disabled_ = true;
}

void AudioRendererImpl::HandleAbortedReadOrDecodeError(bool is_decode_error) {
  PipelineStatus status = is_decode_error ? PIPELINE_ERROR_DECODE : PIPELINE_OK;
  switch (state_) {
    case kUninitialized:
      NOTREACHED();
      return;
    case kPaused:
      if (status != PIPELINE_OK)
        error_cb_.Run(status);
      base::ResetAndReturn(&pause_cb_).Run();
      return;
    case kPrerolling:
      // This is a signal for abort if it's not an error.
      preroll_aborted_ = !is_decode_error;
      state_ = kPaused;
      base::ResetAndReturn(&preroll_cb_).Run(status);
      return;
    case kPlaying:
    case kUnderflow:
    case kRebuffering:
    case kStopped:
      if (status != PIPELINE_OK)
        error_cb_.Run(status);
      return;
  }
}

}  // namespace media
