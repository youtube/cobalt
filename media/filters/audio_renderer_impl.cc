// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_impl.h"

#include <math.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "media/base/filter_host.h"
#include "media/audio/audio_util.h"

namespace media {

AudioRendererImpl::AudioRendererImpl(media::AudioRendererSink* sink)
    : state_(kUninitialized),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      audio_time_buffered_(kNoTimestamp()),
      bytes_per_frame_(0),
      bytes_per_second_(0),
      stopped_(false),
      sink_(sink),
      is_initialized_(false),
      read_cb_(base::Bind(&AudioRendererImpl::DecodedAudioReady,
                          base::Unretained(this))) {
}

AudioRendererImpl::~AudioRendererImpl() {
  // Stop() should have been called and |algorithm_| should have been destroyed.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(!algorithm_.get());
}

void AudioRendererImpl::Play(const base::Closure& callback) {
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(kPaused, state_);
    state_ = kPlaying;
    callback.Run();
  }

  if (stopped_)
    return;

  if (GetPlaybackRate() != 0.0f) {
    DoPlay();
  } else {
    DoPause();
  }
}

void AudioRendererImpl::DoPlay() {
  earliest_end_time_ = base::Time::Now();
  DCHECK(sink_.get());
  sink_->Play();
}

void AudioRendererImpl::Pause(const base::Closure& callback) {
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(state_ == kPlaying || state_ == kUnderflow ||
           state_ == kRebuffering);
    pause_cb_ = callback;
    state_ = kPaused;

    // Pause only when we've completed our pending read.
    if (!pending_read_) {
      pause_cb_.Run();
      pause_cb_.Reset();
    }
  }

  if (stopped_)
    return;

  DoPause();
}

void AudioRendererImpl::DoPause() {
  DCHECK(sink_.get());
  sink_->Pause(false);
}

void AudioRendererImpl::Flush(const base::Closure& callback) {
  decoder_->Reset(callback);
}

void AudioRendererImpl::Stop(const base::Closure& callback) {
  if (!stopped_) {
    DCHECK(sink_.get());
    sink_->Stop();

    stopped_ = true;
  }
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;
    algorithm_.reset(NULL);
    time_cb_.Reset();
    underflow_cb_.Reset();
  }
  if (!callback.is_null()) {
    callback.Run();
  }
}

void AudioRendererImpl::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";
  DCHECK(pause_cb_.is_null());
  DCHECK(seek_cb_.is_null());
  state_ = kSeeking;
  seek_cb_ = cb;
  seek_timestamp_ = time;

  // Throw away everything and schedule our reads.
  audio_time_buffered_ = kNoTimestamp();
  received_end_of_stream_ = false;
  rendered_end_of_stream_ = false;

  // |algorithm_| will request more reads.
  algorithm_->FlushBuffers();

  if (stopped_)
    return;

  DoSeek();
}

void AudioRendererImpl::DoSeek() {
  earliest_end_time_ = base::Time::Now();

  // Pause and flush the stream when we seek to a new location.
  sink_->Pause(true);
}

void AudioRendererImpl::Initialize(const scoped_refptr<AudioDecoder>& decoder,
                                   const PipelineStatusCB& init_cb,
                                   const base::Closure& underflow_cb,
                                   const TimeCB& time_cb) {
  DCHECK(decoder);
  DCHECK(!init_cb.is_null());
  DCHECK(!underflow_cb.is_null());
  DCHECK(!time_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;
  underflow_cb_ = underflow_cb;
  time_cb_ = time_cb;

  // Create a callback so our algorithm can request more reads.
  base::Closure cb = base::Bind(&AudioRendererImpl::ScheduleRead_Locked, this);

  // Construct the algorithm.
  algorithm_.reset(new AudioRendererAlgorithm());

  // Initialize our algorithm with media properties, initial playback rate,
  // and a callback to request more reads from the data source.
  ChannelLayout channel_layout = decoder_->channel_layout();
  int channels = ChannelLayoutToChannelCount(channel_layout);
  int bits_per_channel = decoder_->bits_per_channel();
  int sample_rate = decoder_->samples_per_second();
  // TODO(vrk): Add method to AudioDecoder to compute bytes per frame.
  bytes_per_frame_ = channels * bits_per_channel / 8;

  bool config_ok = algorithm_->ValidateConfig(channels, sample_rate,
                                              bits_per_channel);
  if (!config_ok || is_initialized_) {
    init_cb.Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  if (config_ok)
    algorithm_->Initialize(channels, sample_rate, bits_per_channel, 0.0f, cb);

  // We use the AUDIO_PCM_LINEAR flag because AUDIO_PCM_LOW_LATENCY
  // does not currently support all the sample-rates that we require.
  // Please see: http://code.google.com/p/chromium/issues/detail?id=103627
  // for more details.
  audio_parameters_ = AudioParameters(
      AudioParameters::AUDIO_PCM_LINEAR, channel_layout, sample_rate,
      bits_per_channel, GetHighLatencyOutputBufferSize(sample_rate));

  bytes_per_second_ = audio_parameters_.GetBytesPerSecond();

  DCHECK(sink_.get());
  DCHECK(!is_initialized_);

  sink_->Initialize(audio_parameters_, this);

  sink_->Start();
  is_initialized_ = true;

  // Finally, execute the start callback.
  state_ = kPaused;
  init_cb.Run(PIPELINE_OK);
}

bool AudioRendererImpl::HasEnded() {
  base::AutoLock auto_lock(lock_);
  DCHECK(!rendered_end_of_stream_ || algorithm_->NeedsMoreData());

  return received_end_of_stream_ && rendered_end_of_stream_;
}

void AudioRendererImpl::ResumeAfterUnderflow(bool buffer_more_audio) {
  base::AutoLock auto_lock(lock_);
  if (state_ == kUnderflow) {
    if (buffer_more_audio)
      algorithm_->IncreaseQueueCapacity();

    state_ = kRebuffering;
  }
}

void AudioRendererImpl::SetVolume(float volume) {
  if (stopped_)
    return;
  sink_->SetVolume(volume);
}

void AudioRendererImpl::DecodedAudioReady(scoped_refptr<Buffer> buffer) {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying ||
         state_ == kUnderflow || state_ == kRebuffering || state_ == kStopped);

  CHECK(pending_read_);
  pending_read_ = false;

  if (buffer && buffer->IsEndOfStream()) {
    received_end_of_stream_ = true;

    // Transition to kPlaying if we are currently handling an underflow since
    // no more data will be arriving.
    if (state_ == kUnderflow || state_ == kRebuffering)
      state_ = kPlaying;
  }

  switch (state_) {
    case kUninitialized:
      NOTREACHED();
      return;
    case kPaused:
      if (buffer && !buffer->IsEndOfStream())
        algorithm_->EnqueueBuffer(buffer);
      DCHECK(!pending_read_);
      base::ResetAndReturn(&pause_cb_).Run();
      return;
    case kSeeking:
      if (IsBeforeSeekTime(buffer)) {
        ScheduleRead_Locked();
        return;
      }
      if (buffer && !buffer->IsEndOfStream()) {
        algorithm_->EnqueueBuffer(buffer);
        if (!algorithm_->IsQueueFull())
          return;
      }
      state_ = kPaused;
      base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
      return;
    case kPlaying:
    case kUnderflow:
    case kRebuffering:
      if (buffer && !buffer->IsEndOfStream())
        algorithm_->EnqueueBuffer(buffer);
      return;
    case kStopped:
      return;
  }
}

void AudioRendererImpl::SignalEndOfStream() {
  DCHECK(received_end_of_stream_);
  if (!rendered_end_of_stream_) {
    rendered_end_of_stream_ = true;
    host()->NotifyEnded();
  }
}

void AudioRendererImpl::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  if (pending_read_ || state_ == kPaused)
    return;
  pending_read_ = true;
  decoder_->Read(read_cb_);
}

void AudioRendererImpl::SetPlaybackRate(float playback_rate) {
  DCHECK_LE(0.0f, playback_rate);

  if (!stopped_) {
    // Notify sink of new playback rate.
    sink_->SetPlaybackRate(playback_rate);

    // We have two cases here:
    // Play: GetPlaybackRate() == 0.0 && playback_rate != 0.0
    // Pause: GetPlaybackRate() != 0.0 && playback_rate == 0.0
    if (GetPlaybackRate() == 0.0f && playback_rate != 0.0f) {
      DoPlay();
    } else if (GetPlaybackRate() != 0.0f && playback_rate == 0.0f) {
      // Pause is easy, we can always pause.
      DoPause();
    }
  }

  base::AutoLock auto_lock(lock_);
  algorithm_->SetPlaybackRate(playback_rate);
}

float AudioRendererImpl::GetPlaybackRate() {
  base::AutoLock auto_lock(lock_);
  return algorithm_->playback_rate();
}

bool AudioRendererImpl::IsBeforeSeekTime(const scoped_refptr<Buffer>& buffer) {
  return (state_ == kSeeking) && buffer && !buffer->IsEndOfStream() &&
      (buffer->GetTimestamp() + buffer->GetDuration()) < seek_timestamp_;
}

int AudioRendererImpl::Render(const std::vector<float*>& audio_data,
                              int number_of_frames,
                              int audio_delay_milliseconds) {
  if (stopped_ || GetPlaybackRate() == 0.0f) {
    // Output silence if stopped.
    for (size_t i = 0; i < audio_data.size(); ++i)
      memset(audio_data[i], 0, sizeof(float) * number_of_frames);
    return 0;
  }

  // Adjust the playback delay.
  base::TimeDelta request_delay =
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds);

  // Finally we need to adjust the delay according to playback rate.
  if (GetPlaybackRate() != 1.0f) {
    request_delay = base::TimeDelta::FromMicroseconds(
        static_cast<int64>(ceil(request_delay.InMicroseconds() *
                                GetPlaybackRate())));
  }

  int bytes_per_frame = audio_parameters_.GetBytesPerFrame();

  const int buf_size = number_of_frames * bytes_per_frame;
  scoped_array<uint8> buf(new uint8[buf_size]);

  int frames_filled = FillBuffer(buf.get(), number_of_frames, request_delay);
  int bytes_filled = frames_filled * bytes_per_frame;
  DCHECK_LE(bytes_filled, buf_size);
  UpdateEarliestEndTime(bytes_filled, request_delay, base::Time::Now());

  // Deinterleave each audio channel.
  int channels = audio_data.size();
  for (int channel_index = 0; channel_index < channels; ++channel_index) {
    media::DeinterleaveAudioChannel(buf.get(),
                                    audio_data[channel_index],
                                    channels,
                                    channel_index,
                                    bytes_per_frame / channels,
                                    frames_filled);

    // If FillBuffer() didn't give us enough data then zero out the remainder.
    if (frames_filled < number_of_frames) {
      int frames_to_zero = number_of_frames - frames_filled;
      memset(audio_data[channel_index] + frames_filled,
             0,
             sizeof(float) * frames_to_zero);
    }
  }
  return frames_filled;
}

uint32 AudioRendererImpl::FillBuffer(uint8* dest,
                                     uint32 requested_frames,
                                     const base::TimeDelta& playback_delay) {
  base::TimeDelta current_time = kNoTimestamp();
  base::TimeDelta max_time = kNoTimestamp();

  size_t frames_written = 0;
  base::Closure underflow_cb;
  {
    base::AutoLock auto_lock(lock_);

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
      size_t zeros_to_write =
          std::min(kZeroLength, requested_frames * bytes_per_frame_);
      memset(dest, 0, zeros_to_write);
      return zeros_to_write / bytes_per_frame_;
    }

    // Use three conditions to determine the end of playback:
    // 1. Algorithm needs more audio data.
    // 2. We've received an end of stream buffer.
    //    (received_end_of_stream_ == true)
    // 3. Browser process has no audio data being played.
    //    There is no way to check that condition that would work for all
    //    derived classes, so call virtual method that would either render
    //    end of stream or schedule such rendering.
    //
    // Three conditions determine when an underflow occurs:
    // 1. Algorithm has no audio data.
    // 2. Currently in the kPlaying state.
    // 3. Have not received an end of stream buffer.
    if (algorithm_->NeedsMoreData()) {
      if (received_end_of_stream_) {
        // TODO(enal): schedule callback instead of polling.
        if (base::Time::Now() >= earliest_end_time_)
          SignalEndOfStream();
      } else if (state_ == kPlaying) {
        state_ = kUnderflow;
        underflow_cb = underflow_cb_;
      }
    } else {
      // Otherwise fill the buffer.
      frames_written = algorithm_->FillBuffer(dest, requested_frames);
    }

    // The |audio_time_buffered_| is the ending timestamp of the last frame
    // buffered at the audio device. |playback_delay| is the amount of time
    // buffered at the audio device. The current time can be computed by their
    // difference.
    if (audio_time_buffered_ != kNoTimestamp()) {
      current_time = audio_time_buffered_ - playback_delay;
    }

    // The call to FillBuffer() on |algorithm_| has increased the amount of
    // buffered audio data. Update the new amount of time buffered.
    max_time = audio_time_buffered_ = algorithm_->GetTime();
  }

  if (current_time != kNoTimestamp() &&
      current_time > host()->GetTime() &&
      max_time != kNoTimestamp()) {
    time_cb_.Run(current_time, max_time);
  }

  if (!underflow_cb.is_null())
    underflow_cb.Run();

  return frames_written;
}

void AudioRendererImpl::UpdateEarliestEndTime(int bytes_filled,
                                              base::TimeDelta request_delay,
                                              base::Time time_now) {
  if (bytes_filled != 0) {
    base::TimeDelta predicted_play_time = ConvertToDuration(bytes_filled);
    float playback_rate = GetPlaybackRate();
    if (playback_rate != 1.0f) {
      predicted_play_time = base::TimeDelta::FromMicroseconds(
          static_cast<int64>(ceil(predicted_play_time.InMicroseconds() *
                                  playback_rate)));
    }
    earliest_end_time_ =
        std::max(earliest_end_time_,
                 time_now + request_delay + predicted_play_time);
  }
}

base::TimeDelta AudioRendererImpl::ConvertToDuration(int bytes) {
  if (bytes_per_second_) {
    return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * bytes / bytes_per_second_);
  }
  return base::TimeDelta();
}

void AudioRendererImpl::OnRenderError() {
  host()->DisableAudioRenderer();
}

}  // namespace media
