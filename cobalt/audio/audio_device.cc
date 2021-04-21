// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <memory>

#include "cobalt/audio/audio_device.h"

#include "starboard/configuration.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/audio/audio_helpers.h"

#include "starboard/audio_sink.h"

namespace cobalt {
namespace audio {

typedef media::AudioBus AudioBus;

namespace {
const int kRenderBufferSizeFrames = 1024;
const int kDefaultFramesPerChannel = 8 * kRenderBufferSizeFrames;
}  // namespace

class AudioDevice::Impl {
 public:
  Impl(int number_of_channels, RenderCallback* callback);
  ~Impl();

 private:
  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames, bool* is_playing,
                                     bool* is_eos_reached, void* context);

#if SB_API_VERSION >= 12 || !SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  static void ConsumeFramesFunc(int frames_consumed, void* context);
#else   // SB_API_VERSION >=  12 || !SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  static void ConsumeFramesFunc(int frames_consumed,
                                SbTime frames_consumed_at,
                                void* context);
#endif  // SB_API_VERSION >=  12 || !SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)

  void UpdateSourceStatus(int* frames_in_buffer, int* offset_in_frames,
                          bool* is_playing, bool* is_eos_reached);
  void ConsumeFrames(int frames_consumed);

  void FillOutputAudioBus();

  template <typename InputType, typename OutputType>
  inline void FillOutputAudioBusForType();

  int number_of_channels_;
  SbMediaAudioSampleType output_sample_type_;
  RenderCallback* render_callback_;
  int frames_per_channel_;

  // The |render_callback_| returns audio data in planar form.  So we read it
  // into |input_audio_bus_| and convert it into interleaved form and store in
  // |output_frame_buffer_|.
  AudioBus input_audio_bus_;

  std::unique_ptr<uint8[]> output_frame_buffer_;

  void* frame_buffers_[1];
  int64 frames_rendered_ = 0;  // Frames retrieved from |render_callback_|.
  int64 frames_consumed_ = 0;  // Accumulated frames consumed by the sink.
  int64 silence_written_ = 0;  // Silence frames written after all nodes are
                               // finished.

  bool was_silence_last_update_ = false;

  SbAudioSink audio_sink_ = kSbAudioSinkInvalid;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

// AudioDevice::Impl.
AudioDevice::Impl::Impl(int number_of_channels, RenderCallback* callback)
    : number_of_channels_(number_of_channels),
      output_sample_type_(GetPreferredOutputStarboardSampleType()),
      render_callback_(callback),
      frames_per_channel_(std::max(SbAudioSinkGetMinBufferSizeInFrames(
                                       number_of_channels, output_sample_type_,
                                       kStandardOutputSampleRate) +
                                       kRenderBufferSizeFrames * 2,
                                   kDefaultFramesPerChannel)),
      input_audio_bus_(static_cast<size_t>(number_of_channels),
                       static_cast<size_t>(kRenderBufferSizeFrames),
                       GetPreferredOutputSampleType(), AudioBus::kPlanar),
      output_frame_buffer_(
          new uint8[frames_per_channel_ * number_of_channels_ *
                    GetStarboardSampleTypeSize(output_sample_type_)]) {
  DCHECK(number_of_channels_ == 1 || number_of_channels_ == 2)
      << "Invalid number of channels: " << number_of_channels_;
  DCHECK(render_callback_);
  DCHECK(SbAudioSinkIsAudioFrameStorageTypeSupported(
      kSbMediaAudioFrameStorageTypeInterleaved))
      << "Only interleaved frame storage is supported.";
  DCHECK(SbAudioSinkIsAudioSampleTypeSupported(output_sample_type_))
      << "Output sample type " << output_sample_type_ << " is not supported.";

  frame_buffers_[0] = output_frame_buffer_.get();
  audio_sink_ = SbAudioSinkCreate(
      number_of_channels_, kStandardOutputSampleRate, output_sample_type_,
      kSbMediaAudioFrameStorageTypeInterleaved, frame_buffers_,
      frames_per_channel_, &AudioDevice::Impl::UpdateSourceStatusFunc,
      &AudioDevice::Impl::ConsumeFramesFunc, this);
  DCHECK(SbAudioSinkIsValid(audio_sink_));
}

AudioDevice::Impl::~Impl() {
  if (SbAudioSinkIsValid(audio_sink_)) {
    SbAudioSinkDestroy(audio_sink_);
  }
}

// static
void AudioDevice::Impl::UpdateSourceStatusFunc(int* frames_in_buffer,
                                               int* offset_in_frames,
                                               bool* is_playing,
                                               bool* is_eos_reached,
                                               void* context) {
  AudioDevice::Impl* impl = reinterpret_cast<AudioDevice::Impl*>(context);
  DCHECK(impl);
  DCHECK(frames_in_buffer);
  DCHECK(offset_in_frames);
  DCHECK(is_playing);
  DCHECK(is_eos_reached);

  impl->UpdateSourceStatus(frames_in_buffer, offset_in_frames, is_playing,
                           is_eos_reached);
}

#if SB_API_VERSION >= 12 || !SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
// static
void AudioDevice::Impl::ConsumeFramesFunc(int frames_consumed, void* context) {
#else   // SB_API_VERSION >= 12 || !SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
void AudioDevice::Impl::ConsumeFramesFunc(int frames_consumed,
                                          SbTime frames_consumed_at,
                                          void* context) {
  SB_UNREFERENCED_PARAMETER(frames_consumed_at);
#endif  // SB_API_VERSION >=  12 || !SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  AudioDevice::Impl* impl = reinterpret_cast<AudioDevice::Impl*>(context);
  DCHECK(impl);

  impl->ConsumeFrames(frames_consumed);
}

void AudioDevice::Impl::UpdateSourceStatus(int* frames_in_buffer,
                                           int* offset_in_frames,
                                           bool* is_playing,
                                           bool* is_eos_reached) {
  TRACE_EVENT0("cobalt::audio", "AudioDevice::Impl::UpdateSourceStatus()");
  // AudioDevice may be reused after stopped but before destroyed. Keep writing
  // silence before destroyed to let |audio_sink_| keep working without
  // underflow. It will cause latency between two sounds.
  *is_playing = true;
  *is_eos_reached = false;

  // Assert that we never consume more than we've rendered.
  DCHECK_GE(frames_rendered_, frames_consumed_);
  *frames_in_buffer = static_cast<int>(frames_rendered_ - frames_consumed_);

  while ((frames_per_channel_ - *frames_in_buffer) >= kRenderBufferSizeFrames) {
    // If there was silence last time we were called, then the buffer has
    // already been zeroed out and we don't need to do it again.
    if (!was_silence_last_update_) {
      input_audio_bus_.ZeroAllFrames();
    }

    bool silence = true;
    bool all_consumed =
        silence_written_ != 0 && *frames_in_buffer <= silence_written_;

    render_callback_->FillAudioBus(all_consumed, &input_audio_bus_, &silence);

    if (silence) {
      silence_written_ += kRenderBufferSizeFrames;
    } else {
      // Reset |silence_written_| if a new sound is played after some silence
      // frames were injected.
      silence_written_ = 0;
    }

    FillOutputAudioBus();

    frames_rendered_ += kRenderBufferSizeFrames;
    *frames_in_buffer += kRenderBufferSizeFrames;

    was_silence_last_update_ = silence;
  }

  *offset_in_frames = frames_consumed_ % frames_per_channel_;
}

void AudioDevice::Impl::ConsumeFrames(int frames_consumed) {
  frames_consumed_ += frames_consumed;
}

template <typename InputType, typename OutputType>
inline void AudioDevice::Impl::FillOutputAudioBusForType() {
  // Determine the offset into the audio bus that represents the tail of
  // buffered data.
  uint64 channel_offset = frames_rendered_ % frames_per_channel_;

  OutputType* output_buffer =
      reinterpret_cast<OutputType*>(output_frame_buffer_.get());
  output_buffer += channel_offset * number_of_channels_;
  for (size_t frame = 0; frame < kRenderBufferSizeFrames; ++frame) {
    for (size_t channel = 0; channel < input_audio_bus_.channels(); ++channel) {
      *output_buffer = ConvertSample<InputType, OutputType>(
          input_audio_bus_
              .GetSampleForType<InputType, media::AudioBus::kPlanar>(channel,
                                                                     frame));
      ++output_buffer;
    }
  }
}

void AudioDevice::Impl::FillOutputAudioBus() {
  TRACE_EVENT0("cobalt::audio", "AudioDevice::Impl::FillOutputAudioBus()");

  const bool is_input_int16 =
      input_audio_bus_.sample_type() == media::AudioBus::kInt16;
  const bool is_output_int16 =
      output_sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated;

  if (is_input_int16 && is_output_int16) {
    FillOutputAudioBusForType<int16, int16>();
  } else if (!is_input_int16 && is_output_int16) {
    FillOutputAudioBusForType<float, int16>();
  } else if (is_input_int16 && !is_output_int16) {
    FillOutputAudioBusForType<int16, float>();
  } else if (!is_input_int16 && !is_output_int16) {
    FillOutputAudioBusForType<float, float>();
  } else {
    NOTREACHED();
  }
}

// AudioDevice.
AudioDevice::AudioDevice(int32 number_of_channels, RenderCallback* callback)
    : impl_(new Impl(number_of_channels, callback)) {}

AudioDevice::~AudioDevice() {}

}  // namespace audio
}  // namespace cobalt
