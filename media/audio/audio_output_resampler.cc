// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_resampler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/limits.h"
#include "media/base/multi_channel_resampler.h"

namespace media {

// Record UMA statistics for hardware output configuration.
static void RecordStats(const AudioParameters& output_params) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioBitsPerChannel", output_params.bits_per_sample(),
      limits::kMaxBitsPerSample);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioChannelLayout", output_params.channel_layout(),
      CHANNEL_LAYOUT_MAX);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioChannelCount", output_params.channels(),
      limits::kMaxChannels);

  AudioSampleRate asr = media::AsAudioSampleRate(output_params.sample_rate());
  if (asr != kUnexpectedAudioSampleRate) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.HardwareAudioSamplesPerSecond", asr, kUnexpectedAudioSampleRate);
  } else {
    UMA_HISTOGRAM_COUNTS(
        "Media.HardwareAudioSamplesPerSecondUnexpected",
        output_params.sample_rate());
  }
}

// Record UMA statistics for hardware output configuration after fallback.
static void RecordFallbackStats(const AudioParameters& output_params) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioBitsPerChannel",
      output_params.bits_per_sample(), limits::kMaxBitsPerSample);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioChannelLayout",
      output_params.channel_layout(), CHANNEL_LAYOUT_MAX);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioChannelCount",
      output_params.channels(), limits::kMaxChannels);

  AudioSampleRate asr = media::AsAudioSampleRate(output_params.sample_rate());
  if (asr != kUnexpectedAudioSampleRate) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.FallbackHardwareAudioSamplesPerSecond",
        asr, kUnexpectedAudioSampleRate);
  } else {
    UMA_HISTOGRAM_COUNTS(
        "Media.FallbackHardwareAudioSamplesPerSecondUnexpected",
        output_params.sample_rate());
  }
}

AudioOutputResampler::AudioOutputResampler(AudioManager* audio_manager,
                                           const AudioParameters& input_params,
                                           const AudioParameters& output_params,
                                           const base::TimeDelta& close_delay)
    : AudioOutputDispatcher(audio_manager, input_params),
      source_callback_(NULL),
      io_ratio_(1),
      close_delay_(close_delay),
      outstanding_audio_bytes_(0),
      output_params_(output_params) {
  Initialize();
  // Record UMA statistics for the hardware configuration.
  RecordStats(output_params_);
}

AudioOutputResampler::~AudioOutputResampler() {}

void AudioOutputResampler::Initialize() {
  io_ratio_ = 1;

  // TODO(dalecurtis): Add channel remixing.  http://crbug.com/138762
  DCHECK_EQ(params_.channels(), output_params_.channels());
  // Only resample or rebuffer if the input parameters don't match the output
  // parameters to avoid any unnecessary work.
  if (params_.channels() != output_params_.channels() ||
      params_.sample_rate() != output_params_.sample_rate() ||
      params_.bits_per_sample() != output_params_.bits_per_sample() ||
      params_.frames_per_buffer() != output_params_.frames_per_buffer()) {
    // Only resample if necessary since it's expensive.
    if (params_.sample_rate() != output_params_.sample_rate()) {
      DVLOG(1) << "Resampling from " << params_.sample_rate() << " to "
               << output_params_.sample_rate();
      double io_sample_rate_ratio = params_.sample_rate() /
          static_cast<double>(output_params_.sample_rate());
      // Include the I/O resampling ratio in our global I/O ratio.
      io_ratio_ *= io_sample_rate_ratio;
      resampler_.reset(new MultiChannelResampler(
          output_params_.channels(), io_sample_rate_ratio, base::Bind(
              &AudioOutputResampler::ProvideInput, base::Unretained(this))));
    }

    // Include bits per channel differences.
    io_ratio_ *= static_cast<double>(params_.bits_per_sample()) /
        output_params_.bits_per_sample();

    // Include channel count differences.
    io_ratio_ *= static_cast<double>(params_.channels()) /
        output_params_.channels();

    // Since the resampler / output device may want a different buffer size than
    // the caller asked for, we need to use a FIFO to ensure that both sides
    // read in chunk sizes they're configured for.
    if (params_.sample_rate() != output_params_.sample_rate() ||
        params_.frames_per_buffer() != output_params_.frames_per_buffer()) {
      DVLOG(1) << "Rebuffering from " << params_.frames_per_buffer()
               << " to " << output_params_.frames_per_buffer();
      audio_fifo_.reset(new AudioPullFifo(
          params_.channels(), params_.frames_per_buffer(), base::Bind(
              &AudioOutputResampler::SourceCallback_Locked,
              base::Unretained(this))));
    }

    DVLOG(1) << "I/O ratio is " << io_ratio_;
  }

  // TODO(dalecurtis): All this code should be merged into AudioOutputMixer once
  // we've stabilized the issues there.
  dispatcher_ = new AudioOutputDispatcherImpl(
      audio_manager_, output_params_, close_delay_);
}

bool AudioOutputResampler::OpenStream() {
  if (dispatcher_->OpenStream()) {
    UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", false);
    return true;
  }

  // If we've already tried to open the stream in high latency mode, there's
  // nothing more to be done.
  if (output_params_.format() == AudioParameters::AUDIO_PCM_LINEAR)
    return false;

  DLOG(ERROR) << "Unable to open audio device in low latency mode.  Falling "
              << "back to high latency audio output.";

  // Record UMA statistics about the hardware which triggered the failure so we
  // can debug and triage later.
  UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", true);
  RecordFallbackStats(output_params_);

  // Open failed!  Attempt to open the output device in high latency mode using
  // a new high latency appropriate buffer size.  |kMinLowLatencyFrameSize| is
  // arbitrarily based on Pepper Flash's MAXIMUM frame size for low latency.
  static const int kMinLowLatencyFrameSize = 2048;
  int frames_per_buffer = std::max(
      std::min(params_.frames_per_buffer(), kMinLowLatencyFrameSize),
      static_cast<int>(GetHighLatencyOutputBufferSize(params_.sample_rate())));

  output_params_ = AudioParameters(
      AudioParameters::AUDIO_PCM_LINEAR, params_.channel_layout(),
      params_.sample_rate(), params_.bits_per_sample(), frames_per_buffer);
  Initialize();

  // Retry, if this fails, there's nothing left to do but report the error back.
  return dispatcher_->OpenStream();
}

bool AudioOutputResampler::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  {
    base::AutoLock auto_lock(source_lock_);
    source_callback_ = callback;
  }
  return dispatcher_->StartStream(this, stream_proxy);
}

void AudioOutputResampler::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                           double volume) {
  dispatcher_->StreamVolumeSet(stream_proxy, volume);
}

void AudioOutputResampler::Reset() {
  base::AutoLock auto_lock(source_lock_);
  source_callback_ = NULL;
  outstanding_audio_bytes_ = 0;
  if (audio_fifo_.get())
    audio_fifo_->Clear();
  if (resampler_.get())
    resampler_->Flush();
}

void AudioOutputResampler::StopStream(AudioOutputProxy* stream_proxy) {
  Reset();
  dispatcher_->StopStream(stream_proxy);
}

void AudioOutputResampler::CloseStream(AudioOutputProxy* stream_proxy) {
  Reset();
  dispatcher_->CloseStream(stream_proxy);
}

void AudioOutputResampler::Shutdown() {
  Reset();
  dispatcher_->Shutdown();
}

int AudioOutputResampler::OnMoreData(AudioBus* dest,
                                     AudioBuffersState buffers_state) {
  return OnMoreIOData(NULL, dest, buffers_state);
}

int AudioOutputResampler::OnMoreIOData(AudioBus* source,
                                       AudioBus* dest,
                                       AudioBuffersState buffers_state) {
  base::AutoLock auto_lock(source_lock_);
  // While we waited for |source_lock_| the callback might have been cleared.
  if (!source_callback_) {
    dest->Zero();
    return dest->frames();
  }

  current_buffers_state_ = buffers_state;

  if (!resampler_.get() && !audio_fifo_.get()) {
    // We have no internal buffers, so clear any outstanding audio data.
    outstanding_audio_bytes_ = 0;
    SourceCallback_Locked(dest);
    return dest->frames();
  }

  if (resampler_.get())
    resampler_->Resample(dest, dest->frames());
  else
    ProvideInput(dest);

  // Calculate how much data is left in the internal FIFO and resampler buffers.
  outstanding_audio_bytes_ -=
      dest->frames() * output_params_.GetBytesPerFrame();

  // Due to rounding errors while multiplying against |io_ratio_|,
  // |outstanding_audio_bytes_| might (rarely) slip below zero.
  if (outstanding_audio_bytes_ < 0) {
    DLOG(ERROR) << "Outstanding audio bytes went negative! Value: "
                << outstanding_audio_bytes_;
    outstanding_audio_bytes_ = 0;
  }

  // Always return the full number of frames requested, ProvideInput() will pad
  // with silence if it wasn't able to acquire enough data.
  return dest->frames();
}

void AudioOutputResampler::SourceCallback_Locked(AudioBus* audio_bus) {
  source_lock_.AssertAcquired();

  // Adjust playback delay to include the state of the internal buffers used by
  // the resampler and/or the FIFO.  Since the sample rate and bits per channel
  // may be different, we need to scale this value appropriately.
  AudioBuffersState new_buffers_state;
  new_buffers_state.pending_bytes = io_ratio_ *
      (current_buffers_state_.total_bytes() + outstanding_audio_bytes_);

  // Retrieve data from the original callback.  Zero any unfilled frames.
  int frames = source_callback_->OnMoreData(audio_bus, new_buffers_state);
  if (frames < audio_bus->frames())
    audio_bus->ZeroFramesPartial(frames, audio_bus->frames() - frames);

  // Scale the number of frames we got back in terms of input bytes to output
  // bytes accordingly.
  outstanding_audio_bytes_ +=
      (audio_bus->frames() * params_.GetBytesPerFrame()) / io_ratio_;
}

void AudioOutputResampler::ProvideInput(AudioBus* audio_bus) {
  audio_fifo_->Consume(audio_bus, audio_bus->frames());
}

void AudioOutputResampler::OnError(AudioOutputStream* stream, int code) {
  base::AutoLock auto_lock(source_lock_);
  if (source_callback_)
    source_callback_->OnError(stream, code);
}

void AudioOutputResampler::WaitTillDataReady() {
  base::AutoLock auto_lock(source_lock_);
  if (source_callback_ && !outstanding_audio_bytes_)
    source_callback_->WaitTillDataReady();
}

}  // namespace media
