// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_resampler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_util.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/multi_channel_resampler.h"

namespace media {

AudioOutputResampler::AudioOutputResampler(AudioManager* audio_manager,
                                           const AudioParameters& input_params,
                                           const AudioParameters& output_params,
                                           const base::TimeDelta& close_delay)
    : AudioOutputDispatcher(audio_manager, input_params),
      source_callback_(NULL),
      io_ratio_(1),
      input_bytes_per_frame_(input_params.GetBytesPerFrame()),
      output_bytes_per_frame_(output_params.GetBytesPerFrame()),
      outstanding_audio_bytes_(0) {
  // TODO(dalecurtis): Add channel remixing.  http://crbug.com/138762
  DCHECK_EQ(input_params.channels(), output_params.channels());
  // Only resample or rebuffer if the input parameters don't match the output
  // parameters to avoid any unnecessary work.
  if (input_params.channels() != output_params.channels() ||
      input_params.sample_rate() != output_params.sample_rate() ||
      input_params.bits_per_sample() != output_params.bits_per_sample() ||
      input_params.frames_per_buffer() != output_params.frames_per_buffer()) {
    // Only resample if necessary since it's expensive.
    if (input_params.sample_rate() != output_params.sample_rate()) {
      DVLOG(1) << "Resampling from " << input_params.sample_rate() << " to "
               << output_params.sample_rate();
      double io_sample_rate_ratio = input_params.sample_rate() /
          static_cast<double>(output_params.sample_rate());
      // Include the I/O resampling ratio in our global I/O ratio.
      io_ratio_ *= io_sample_rate_ratio;
      resampler_.reset(new MultiChannelResampler(
          output_params.channels(), io_sample_rate_ratio, base::Bind(
              &AudioOutputResampler::ProvideInput, base::Unretained(this))));
    }

    // Include bits per channel differences.
    io_ratio_ *= static_cast<double>(input_params.bits_per_sample()) /
        output_params.bits_per_sample();

    // Include channel count differences.
    io_ratio_ *= static_cast<double>(input_params.channels()) /
        output_params.channels();

    // Since the resampler / output device may want a different buffer size than
    // the caller asked for, we need to use a FIFO to ensure that both sides
    // read in chunk sizes they're configured for.
    if (input_params.sample_rate() != output_params.sample_rate() ||
        input_params.frames_per_buffer() != output_params.frames_per_buffer()) {
      DVLOG(1) << "Rebuffering from " << input_params.frames_per_buffer()
               << " to " << output_params.frames_per_buffer();
      audio_fifo_.reset(new AudioPullFifo(
          input_params.channels(), input_params.frames_per_buffer(), base::Bind(
              &AudioOutputResampler::SourceCallback_Locked,
              base::Unretained(this))));
    }

    DVLOG(1) << "I/O ratio is " << io_ratio_;
  }

  // TODO(dalecurtis): All this code should be merged into AudioOutputMixer once
  // we've stabilized the issues there.
  dispatcher_ = new AudioOutputDispatcherImpl(
      audio_manager, output_params, close_delay);
}

AudioOutputResampler::~AudioOutputResampler() {}

bool AudioOutputResampler::OpenStream() {
  // TODO(dalecurtis): Automatically revert to high latency path if OpenStream()
  // fails; use default high latency output values + rebuffering / resampling.
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

int AudioOutputResampler::OnMoreData(AudioBus* audio_bus,
                                     AudioBuffersState buffers_state) {
  base::AutoLock auto_lock(source_lock_);
  // While we waited for |source_lock_| the callback might have been cleared.
  if (!source_callback_) {
    audio_bus->Zero();
    return audio_bus->frames();
  }

  current_buffers_state_ = buffers_state;

  if (!resampler_.get() && !audio_fifo_.get()) {
    // We have no internal buffers, so clear any outstanding audio data.
    outstanding_audio_bytes_ = 0;
    SourceCallback_Locked(audio_bus);
    return audio_bus->frames();
  }

  if (resampler_.get())
    resampler_->Resample(audio_bus, audio_bus->frames());
  else
    ProvideInput(audio_bus);

  // Calculate how much data is left in the internal FIFO and resampler buffers.
  outstanding_audio_bytes_ -= audio_bus->frames() * output_bytes_per_frame_;
  // Due to rounding errors while multiplying against |io_ratio_|,
  // |outstanding_audio_bytes_| might (rarely) slip below zero.
  if (outstanding_audio_bytes_ < 0) {
    DLOG(ERROR) << "Outstanding audio bytes went negative! Value: "
                << outstanding_audio_bytes_;
    outstanding_audio_bytes_ = 0;
  }

  // Always return the full number of frames requested, ProvideInput() will pad
  // with silence if it wasn't able to acquire enough data.
  return audio_bus->frames();
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
      (audio_bus->frames() * input_bytes_per_frame_) / io_ratio_;
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
