// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_resampler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
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
#include "media/base/media_switches.h"
#include "media/base/multi_channel_resampler.h"

namespace media {

class OnMoreDataResampler : public AudioOutputStream::AudioSourceCallback {
 public:
  OnMoreDataResampler(double io_ratio,
                      const AudioParameters& input_params,
                      const AudioParameters& output_params);
  virtual ~OnMoreDataResampler();

  // AudioSourceCallback interface.
  virtual int OnMoreData(AudioBus* dest,
                         AudioBuffersState buffers_state) OVERRIDE;
  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) OVERRIDE;
  virtual void OnError(AudioOutputStream* stream, int code) OVERRIDE;
  virtual void WaitTillDataReady() OVERRIDE;

  // Sets |source_callback_|.  If this is not a new object, then Stop() must be
  // called before Start().
  void Start(AudioOutputStream::AudioSourceCallback* callback);

  // Clears |source_callback_| and flushes the resampler.
  void Stop();

 private:
  // Called by MultiChannelResampler when more data is necessary.
  void ProvideInput(AudioBus* audio_bus);

  // Called by AudioPullFifo when more data is necessary.  Requires
  // |source_lock_| to have been acquired.
  void SourceCallback_Locked(AudioBus* audio_bus);

  // Passes through |source| to the |source_callback_| OnMoreIOData() call.
  void SourceIOCallback_Locked(AudioBus* source, AudioBus* dest);

  // Ratio of input bytes to output bytes used to correct playback delay with
  // regard to buffering and resampling.
  double io_ratio_;

  // Source callback and associated lock.
  base::Lock source_lock_;
  AudioOutputStream::AudioSourceCallback* source_callback_;

  // Last AudioBuffersState object received via OnMoreData(), used to correct
  // playback delay by ProvideInput() and passed on to |source_callback_|.
  AudioBuffersState current_buffers_state_;

  // Total number of bytes (in terms of output parameters) stored in resampler
  // or FIFO buffers which have not been sent to the audio device.
  int outstanding_audio_bytes_;

  // Used to buffer data between the client and the output device in cases where
  // the client buffer size is not the same as the output device buffer size.
  // Bound to SourceCallback_Locked() so must only be used when |source_lock_|
  // has already been acquired.
  scoped_ptr<AudioPullFifo> audio_fifo_;

  // Handles resampling.
  scoped_ptr<MultiChannelResampler> resampler_;

  int output_bytes_per_frame_;
  int input_bytes_per_frame_;

  DISALLOW_COPY_AND_ASSIGN(OnMoreDataResampler);
};

// Record UMA statistics for hardware output configuration.
static void RecordStats(const AudioParameters& output_params) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioBitsPerChannel", output_params.bits_per_sample(),
      limits::kMaxBitsPerSample);
// WASAPIAudioOutputStream will record this information for us.
// TODO(dalecurtis): This should go away when we support channel mixing and will
// receive the actual hardware channel parameters via |output_params|.  See
// http://crbug.com/138762
#if !defined(OS_WIN)
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioChannelLayout", output_params.channel_layout(),
      CHANNEL_LAYOUT_MAX);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioChannelCount", output_params.channels(),
      limits::kMaxChannels);
#endif

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
  UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", true);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioBitsPerChannel",
      output_params.bits_per_sample(), limits::kMaxBitsPerSample);
// WASAPIAudioOutputStream will record this information for us.
// TODO(dalecurtis): This should go away when we support channel mixing and will
// receive the actual hardware channel parameters via |output_params|.  See
// http://crbug.com/138762
#if !defined(OS_WIN)
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioChannelLayout",
      output_params.channel_layout(), CHANNEL_LAYOUT_MAX);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioChannelCount",
      output_params.channels(), limits::kMaxChannels);
#endif

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

// Converts low latency based |output_params| into high latency appropriate
// output parameters in error situations.
static AudioParameters SetupFallbackParams(
    const AudioParameters& input_params, const AudioParameters& output_params) {
  // Choose AudioParameters appropriate for opening the device in high latency
  // mode.  |kMinLowLatencyFrameSize| is arbitrarily based on Pepper Flash's
  // MAXIMUM frame size for low latency.
  static const int kMinLowLatencyFrameSize = 2048;
  int frames_per_buffer = std::min(
      std::max(input_params.frames_per_buffer(), kMinLowLatencyFrameSize),
      static_cast<int>(
          GetHighLatencyOutputBufferSize(input_params.sample_rate())));

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LINEAR, input_params.channel_layout(),
      input_params.sample_rate(), input_params.bits_per_sample(),
      frames_per_buffer);
}

AudioOutputResampler::AudioOutputResampler(AudioManager* audio_manager,
                                           const AudioParameters& input_params,
                                           const AudioParameters& output_params,
                                           const base::TimeDelta& close_delay)
    : AudioOutputDispatcher(audio_manager, input_params),
      io_ratio_(1),
      close_delay_(close_delay),
      output_params_(output_params) {
  DCHECK_EQ(output_params_.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);

  // Record UMA statistics for the hardware configuration.
  RecordStats(output_params);

  // Immediately fallback if we're given invalid output parameters.  This may
  // happen if the OS provided us junk values for the hardware configuration.
  if (!output_params_.IsValid()) {
    LOG(ERROR) << "Invalid audio output parameters received; using fallback "
               << "path. Channels: " << output_params_.channels() << ", "
               << "Sample Rate: " << output_params_.sample_rate() << ", "
               << "Bits Per Sample: " << output_params_.bits_per_sample()
               << ", Frames Per Buffer: " << output_params_.frames_per_buffer();
    // Record UMA statistics about the hardware which triggered the failure so
    // we can debug and triage later.
    RecordFallbackStats(output_params);
    output_params_ = SetupFallbackParams(input_params, output_params);
  }

  Initialize();
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
    if (params_.sample_rate() != output_params_.sample_rate()) {
      double io_sample_rate_ratio = params_.sample_rate() /
          static_cast<double>(output_params_.sample_rate());
      // Include the I/O resampling ratio in our global I/O ratio.
      io_ratio_ *= io_sample_rate_ratio;
    }

    // Include bits per channel differences.
    io_ratio_ *= static_cast<double>(params_.bits_per_sample()) /
        output_params_.bits_per_sample();

    // Include channel count differences.
    io_ratio_ *= static_cast<double>(params_.channels()) /
        output_params_.channels();

    DVLOG(1) << "I/O ratio is " << io_ratio_;
  } else {
    DVLOG(1) << "Input and output params are the same; in pass-through mode.";
  }

  // TODO(dalecurtis): All this code should be merged into AudioOutputMixer once
  // we've stabilized the issues there.
  dispatcher_ = new AudioOutputDispatcherImpl(
      audio_manager_, output_params_, close_delay_);
}

bool AudioOutputResampler::OpenStream() {
  if (dispatcher_->OpenStream()) {
    // Only record the UMA statistic if we didn't fallback during construction.
    if (output_params_.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY)
      UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", false);
    return true;
  }

  // If we've already tried to open the stream in high latency mode, there's
  // nothing more to be done.
  if (output_params_.format() == AudioParameters::AUDIO_PCM_LINEAR)
    return false;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioFallback)) {
    LOG(ERROR) << "Open failed and automatic fallback to high latency audio "
               << "path is disabled, aborting.";
    return false;
  }

  // TODO(dalecurtis): Is it better to recreate the whole |dispatcher_| ?  See
  // http://crbug.com/149815
  dispatcher_->CloseStream(NULL);

  DLOG(ERROR) << "Unable to open audio device in low latency mode.  Falling "
              << "back to high latency audio output.";

  // Record UMA statistics about the hardware which triggered the failure so
  // we can debug and triage later.
  RecordFallbackStats(output_params_);
  output_params_ = SetupFallbackParams(params_, output_params_);
  Initialize();

  // Retry, if this fails, there's nothing left to do but report the error back.
  return dispatcher_->OpenStream();
}

bool AudioOutputResampler::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  OnMoreDataResampler* resampler_callback = NULL;
  {
    base::AutoLock auto_lock(callbacks_lock_);
    CallbackMap::iterator it = callbacks_.find(stream_proxy);
    if (it == callbacks_.end()) {
      resampler_callback = new OnMoreDataResampler(
          io_ratio_, params_, output_params_);
      callbacks_[stream_proxy] = resampler_callback;
    } else {
      resampler_callback = it->second;
    }
    resampler_callback->Start(callback);
  }
  return dispatcher_->StartStream(resampler_callback, stream_proxy);
}

void AudioOutputResampler::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                           double volume) {
  dispatcher_->StreamVolumeSet(stream_proxy, volume);
}

void AudioOutputResampler::StopStream(AudioOutputProxy* stream_proxy) {
  dispatcher_->StopStream(stream_proxy);

  // Now that StopStream() has completed the underlying physical stream should
  // be stopped and no longer calling OnMoreData(), making it safe to Stop() the
  // OnMoreDataResampler.
  {
    base::AutoLock auto_lock(callbacks_lock_);
    CallbackMap::iterator it = callbacks_.find(stream_proxy);
    if (it != callbacks_.end())
      it->second->Stop();
  }
}

void AudioOutputResampler::CloseStream(AudioOutputProxy* stream_proxy) {
  // Force StopStream() before CloseStream().
  // TODO(dalecurtis): This shouldn't be necessary, but somewhere in the chain
  // CloseStream() is occurring without a StopStream() which causes the callback
  // provided by OnMoreDataResampler to go away before the output stream is
  // ready.  http://crbug.com/150619
  StopStream(stream_proxy);
  dispatcher_->CloseStream(stream_proxy);

  // We assume that StopStream() is always called prior to CloseStream(), so
  // that it is safe to delete the OnMoreDataResampler here.
  {
    base::AutoLock auto_lock(callbacks_lock_);
    CallbackMap::iterator it = callbacks_.find(stream_proxy);
    if (it != callbacks_.end()) {
      delete it->second;
      callbacks_.erase(it);
    }
  }
}

void AudioOutputResampler::Shutdown() {
  dispatcher_->Shutdown();
  DCHECK(callbacks_.empty());
}

OnMoreDataResampler::OnMoreDataResampler(
    double io_ratio, const AudioParameters& input_params,
    const AudioParameters& output_params)
    : io_ratio_(io_ratio),
      source_callback_(NULL),
      outstanding_audio_bytes_(0),
      output_bytes_per_frame_(output_params.GetBytesPerFrame()),
      input_bytes_per_frame_(input_params.GetBytesPerFrame()) {
  // Only resample if necessary since it's expensive.
  if (input_params.sample_rate() != output_params.sample_rate()) {
    DVLOG(1) << "Resampling from " << input_params.sample_rate() << " to "
             << output_params.sample_rate();
    double io_sample_rate_ratio = input_params.sample_rate() /
        static_cast<double>(output_params.sample_rate());
    resampler_.reset(new MultiChannelResampler(
        output_params.channels(), io_sample_rate_ratio, base::Bind(
            &OnMoreDataResampler::ProvideInput, base::Unretained(this))));
  }

  // Since the resampler / output device may want a different buffer size than
  // the caller asked for, we need to use a FIFO to ensure that both sides
  // read in chunk sizes they're configured for.
  if (input_params.sample_rate() != output_params.sample_rate() ||
      input_params.frames_per_buffer() != output_params.frames_per_buffer()) {
    DVLOG(1) << "Rebuffering from " << input_params.frames_per_buffer()
             << " to " << output_params.frames_per_buffer();
    audio_fifo_.reset(new AudioPullFifo(
        input_params.channels(), input_params.frames_per_buffer(), base::Bind(
            &OnMoreDataResampler::SourceCallback_Locked,
            base::Unretained(this))));
  }
}

OnMoreDataResampler::~OnMoreDataResampler() {}

void OnMoreDataResampler::Start(
    AudioOutputStream::AudioSourceCallback* callback) {
  base::AutoLock auto_lock(source_lock_);
  DCHECK(!source_callback_);
  source_callback_ = callback;
}

void OnMoreDataResampler::Stop() {
  base::AutoLock auto_lock(source_lock_);
  source_callback_ = NULL;
  outstanding_audio_bytes_ = 0;
  if (audio_fifo_.get())
    audio_fifo_->Clear();
  if (resampler_.get())
    resampler_->Flush();
}

int OnMoreDataResampler::OnMoreData(AudioBus* dest,
                                    AudioBuffersState buffers_state) {
  return OnMoreIOData(NULL, dest, buffers_state);
}

int OnMoreDataResampler::OnMoreIOData(AudioBus* source,
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
    SourceIOCallback_Locked(source, dest);
    return dest->frames();
  }

  if (resampler_.get())
    resampler_->Resample(dest, dest->frames());
  else
    ProvideInput(dest);

  // Calculate how much data is left in the internal FIFO and resampler buffers.
  outstanding_audio_bytes_ -= dest->frames() * output_bytes_per_frame_;

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

void OnMoreDataResampler::SourceCallback_Locked(AudioBus* dest) {
  SourceIOCallback_Locked(NULL, dest);
}

void OnMoreDataResampler::SourceIOCallback_Locked(AudioBus* source,
                                                  AudioBus* dest) {
  source_lock_.AssertAcquired();

  // Adjust playback delay to include the state of the internal buffers used by
  // the resampler and/or the FIFO.  Since the sample rate and bits per channel
  // may be different, we need to scale this value appropriately.
  AudioBuffersState new_buffers_state;
  new_buffers_state.pending_bytes = io_ratio_ *
      (current_buffers_state_.total_bytes() + outstanding_audio_bytes_);

  // Retrieve data from the original callback.  Zero any unfilled frames.
  int frames = source_callback_->OnMoreIOData(source, dest, new_buffers_state);
  if (frames < dest->frames())
    dest->ZeroFramesPartial(frames, dest->frames() - frames);

  // Scale the number of frames we got back in terms of input bytes to output
  // bytes accordingly.
  outstanding_audio_bytes_ +=
      (dest->frames() * input_bytes_per_frame_) / io_ratio_;
}

void OnMoreDataResampler::ProvideInput(AudioBus* audio_bus) {
  audio_fifo_->Consume(audio_bus, audio_bus->frames());
}

void OnMoreDataResampler::OnError(AudioOutputStream* stream, int code) {
  base::AutoLock auto_lock(source_lock_);
  if (source_callback_)
    source_callback_->OnError(stream, code);
}

void OnMoreDataResampler::WaitTillDataReady() {
  base::AutoLock auto_lock(source_lock_);
  if (source_callback_ && !outstanding_audio_bytes_)
    source_callback_->WaitTillDataReady();
}

}  // namespace media
