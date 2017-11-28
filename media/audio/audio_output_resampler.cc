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
#include "build/build_config.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"
#include "media/base/audio_converter.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

#if defined(OS_WIN)
#include "media/audio/win/core_audio_util_win.h"
#endif

namespace media {

class OnMoreDataConverter
    : public AudioOutputStream::AudioSourceCallback,
      public AudioConverter::InputCallback {
 public:
  OnMoreDataConverter(const AudioParameters& input_params,
                      const AudioParameters& output_params);
  virtual ~OnMoreDataConverter();

  // AudioSourceCallback interface.
  virtual int OnMoreData(AudioBus* dest,
                         AudioBuffersState buffers_state) override;
  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) override;
  virtual void OnError(AudioOutputStream* stream, int code) override;
  virtual void WaitTillDataReady() override;

  // Sets |source_callback_|.  If this is not a new object, then Stop() must be
  // called before Start().
  void Start(AudioOutputStream::AudioSourceCallback* callback);

  // Clears |source_callback_| and flushes the resampler.
  void Stop();

 private:
  // AudioConverter::InputCallback implementation.
  virtual double ProvideInput(AudioBus* audio_bus,
                              base::TimeDelta buffer_delay) override;

  // Ratio of input bytes to output bytes used to correct playback delay with
  // regard to buffering and resampling.
  double io_ratio_;

  // Source callback and associated lock.
  base::Lock source_lock_;
  AudioOutputStream::AudioSourceCallback* source_callback_;

  // |source| passed to OnMoreIOData() which should be passed downstream.
  AudioBus* source_bus_;

  // Last AudioBuffersState object received via OnMoreData(), used to correct
  // playback delay by ProvideInput() and passed on to |source_callback_|.
  AudioBuffersState current_buffers_state_;

  const int input_bytes_per_second_;

  // Handles resampling, buffering, and channel mixing between input and output
  // parameters.
  AudioConverter audio_converter_;

  // If we're using WaveOut on Windows' we always have to wait for DataReady()
  // before calling |source_callback_|.
  bool waveout_wait_hack_;

  DISALLOW_COPY_AND_ASSIGN(OnMoreDataConverter);
};

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
  UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", true);
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
      close_delay_(close_delay),
      output_params_(output_params),
      streams_opened_(false) {
  DCHECK(input_params.IsValid());
  DCHECK(output_params.IsValid());
  DCHECK_EQ(output_params_.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);

  // Record UMA statistics for the hardware configuration.
  RecordStats(output_params);

  Initialize();
}

AudioOutputResampler::~AudioOutputResampler() {
  DCHECK(callbacks_.empty());
}

void AudioOutputResampler::Initialize() {
  DCHECK(!streams_opened_);
  DCHECK(callbacks_.empty());
  dispatcher_ = new AudioOutputDispatcherImpl(
      audio_manager_, output_params_, close_delay_);
}

bool AudioOutputResampler::OpenStream() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (dispatcher_->OpenStream()) {
    // Only record the UMA statistic if we didn't fallback during construction
    // and only for the first stream we open.
    if (!streams_opened_ &&
        output_params_.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
      UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", false);
    }
    streams_opened_ = true;
    return true;
  }

  // If we've already tried to open the stream in high latency mode or we've
  // successfully opened a stream previously, there's nothing more to be done.
  if (output_params_.format() == AudioParameters::AUDIO_PCM_LINEAR ||
      streams_opened_ || !callbacks_.empty()) {
    return false;
  }

  DCHECK_EQ(output_params_.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioFallback)) {
    LOG(ERROR) << "Open failed and automatic fallback to high latency audio "
               << "path is disabled, aborting.";
    return false;
  }

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
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  OnMoreDataConverter* resampler_callback = NULL;
  CallbackMap::iterator it = callbacks_.find(stream_proxy);
  if (it == callbacks_.end()) {
    resampler_callback = new OnMoreDataConverter(params_, output_params_);
    callbacks_[stream_proxy] = resampler_callback;
  } else {
    resampler_callback = it->second;
  }
  resampler_callback->Start(callback);
  return dispatcher_->StartStream(resampler_callback, stream_proxy);
}

void AudioOutputResampler::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                           double volume) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  dispatcher_->StreamVolumeSet(stream_proxy, volume);
}

void AudioOutputResampler::StopStream(AudioOutputProxy* stream_proxy) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  dispatcher_->StopStream(stream_proxy);

  // Now that StopStream() has completed the underlying physical stream should
  // be stopped and no longer calling OnMoreData(), making it safe to Stop() the
  // OnMoreDataConverter.
  CallbackMap::iterator it = callbacks_.find(stream_proxy);
  if (it != callbacks_.end())
    it->second->Stop();
}

void AudioOutputResampler::CloseStream(AudioOutputProxy* stream_proxy) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  dispatcher_->CloseStream(stream_proxy);

  // We assume that StopStream() is always called prior to CloseStream(), so
  // that it is safe to delete the OnMoreDataConverter here.
  CallbackMap::iterator it = callbacks_.find(stream_proxy);
  if (it != callbacks_.end()) {
    delete it->second;
    callbacks_.erase(it);
  }
}

void AudioOutputResampler::Shutdown() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // No AudioOutputProxy objects should hold a reference to us when we get
  // to this stage.
  DCHECK(HasOneRef()) << "Only the AudioManager should hold a reference";

  dispatcher_->Shutdown();
  DCHECK(callbacks_.empty());
}

OnMoreDataConverter::OnMoreDataConverter(const AudioParameters& input_params,
                                         const AudioParameters& output_params)
    : source_callback_(NULL),
      source_bus_(NULL),
      input_bytes_per_second_(input_params.GetBytesPerSecond()),
      audio_converter_(input_params, output_params, false),
      waveout_wait_hack_(false) {
  io_ratio_ =
      static_cast<double>(input_params.GetBytesPerSecond()) /
      output_params.GetBytesPerSecond();

  // TODO(dalecurtis): We should require all render side clients to use a
  // buffer size that's a multiple of the hardware buffer size scaled by the
  // request_sample_rate / hw_sample_rate.  Doing so ensures each hardware
  // request for audio data results in only a single render side callback and
  // would allow us to remove this hack.  See http://crbug.com/162207.
#if defined(OS_WIN)
  waveout_wait_hack_ =
      output_params.format() == AudioParameters::AUDIO_PCM_LINEAR ||
      !CoreAudioUtil::IsSupported();
#endif
}

OnMoreDataConverter::~OnMoreDataConverter() {}

void OnMoreDataConverter::Start(
    AudioOutputStream::AudioSourceCallback* callback) {
  base::AutoLock auto_lock(source_lock_);
  DCHECK(!source_callback_);
  source_callback_ = callback;

  // While AudioConverter can handle multiple inputs, we're using it only with
  // a single input currently.  Eventually this may be the basis for a browser
  // side mixer.
  audio_converter_.AddInput(this);
}

void OnMoreDataConverter::Stop() {
  base::AutoLock auto_lock(source_lock_);
  source_callback_ = NULL;
  audio_converter_.RemoveInput(this);
}

int OnMoreDataConverter::OnMoreData(AudioBus* dest,
                                    AudioBuffersState buffers_state) {
  return OnMoreIOData(NULL, dest, buffers_state);
}

int OnMoreDataConverter::OnMoreIOData(AudioBus* source,
                                      AudioBus* dest,
                                      AudioBuffersState buffers_state) {
  base::AutoLock auto_lock(source_lock_);
  // While we waited for |source_lock_| the callback might have been cleared.
  if (!source_callback_) {
    dest->Zero();
    return dest->frames();
  }

  source_bus_ = source;
  current_buffers_state_ = buffers_state;
  audio_converter_.Convert(dest);

  // Always return the full number of frames requested, ProvideInput_Locked()
  // will pad with silence if it wasn't able to acquire enough data.
  return dest->frames();
}

double OnMoreDataConverter::ProvideInput(AudioBus* dest,
                                         base::TimeDelta buffer_delay) {
  source_lock_.AssertAcquired();

  // Adjust playback delay to include |buffer_delay|.
  // TODO(dalecurtis): Stop passing bytes around, it doesn't make sense since
  // AudioBus is just float data.  Use TimeDelta instead.
  AudioBuffersState new_buffers_state;
  new_buffers_state.pending_bytes =
      io_ratio_ * (current_buffers_state_.total_bytes() +
                   buffer_delay.InSecondsF() * input_bytes_per_second_);

  if (waveout_wait_hack_)
    source_callback_->WaitTillDataReady();

  // Retrieve data from the original callback.
  int frames = source_callback_->OnMoreIOData(
      source_bus_, dest, new_buffers_state);

  // |source_bus_| should only be provided once.
  // TODO(dalecurtis, crogers): This is not a complete fix.  If ProvideInput()
  // is called multiple times, we need to do something more clever here.
  source_bus_ = NULL;

  // Zero any unfilled frames if anything was filled, otherwise we'll just
  // return a volume of zero and let AudioConverter drop the output.
  if (frames > 0 && frames < dest->frames())
    dest->ZeroFramesPartial(frames, dest->frames() - frames);

  // TODO(dalecurtis): Return the correct volume here.
  return frames > 0 ? 1 : 0;
}

void OnMoreDataConverter::OnError(AudioOutputStream* stream, int code) {
  base::AutoLock auto_lock(source_lock_);
  if (source_callback_)
    source_callback_->OnError(stream, code);
}

void OnMoreDataConverter::WaitTillDataReady() {
  base::AutoLock auto_lock(source_lock_);
  if (source_callback_)
    source_callback_->WaitTillDataReady();
}

}  // namespace media
