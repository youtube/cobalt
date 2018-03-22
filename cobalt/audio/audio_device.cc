// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/audio/audio_device.h"

#include "starboard/configuration.h"

#if defined(OS_STARBOARD)
#if SB_CAN(MEDIA_USE_STARBOARD_PIPELINE)
#define SB_USE_SB_AUDIO_SINK 1
#endif  // SB_CAN(MEDIA_USE_STARBOARD_PIPELINE)
#endif  // defined(OS_STARBOARD)

#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/audio/audio_helpers.h"

#if defined(SB_USE_SB_AUDIO_SINK)
#include "starboard/audio_sink.h"
#else  // defined(SB_USE_SB_AUDIO_SINK)
#include "media/audio/audio_parameters.h"
#include "media/audio/shell_audio_streamer.h"
#include "media/base/audio_bus.h"
#endif  // defined(SB_USE_SB_AUDIO_SINK)

namespace cobalt {
namespace audio {

#if defined(COBALT_MEDIA_SOURCE_2016)
typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace {
const int kRenderBufferSizeFrames = 1024;
const int kFramesPerChannel = kRenderBufferSizeFrames * 8;
const int kStandardOutputSampleRate = 48000;
}  // namespace

#if defined(SB_USE_SB_AUDIO_SINK)

class AudioDevice::Impl {
 public:
  Impl(int number_of_channels, RenderCallback* callback);
  ~Impl();

 private:
  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames, bool* is_playing,
                                     bool* is_eos_reached, void* context);
  static void ConsumeFramesFunc(int frames_consumed, void* context);

  void UpdateSourceStatus(int* frames_in_buffer, int* offset_in_frames,
                          bool* is_playing, bool* is_eos_reached);
  void ConsumeFrames(int frames_consumed);

  void FillOutputAudioBus();

  template <typename InputType, typename OutputType>
  inline void FillOutputAudioBusForType();

  int number_of_channels_;
  SbMediaAudioSampleType output_sample_type_;
  RenderCallback* render_callback_;

  // The |render_callback_| returns audio data in planar form.  So we read it
  // into |input_audio_bus_| and convert it into interleaved form and store in
  // |output_frame_buffer_|.
  ShellAudioBus input_audio_bus_;

  scoped_array<uint8> output_frame_buffer_;

  void* frame_buffers_[1];
  int64 frames_rendered_;  // Frames retrieved from |render_callback_|.
  int64 frames_consumed_;  // Accumulated frames consumed reported by the sink.

  bool was_silence_last_update_;

  SbAudioSink audio_sink_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

// AudioDevice::Impl.
AudioDevice::Impl::Impl(int number_of_channels, RenderCallback* callback)
    : number_of_channels_(number_of_channels),
      output_sample_type_(GetPreferredOutputStarboardSampleType()),
      render_callback_(callback),
      input_audio_bus_(static_cast<size_t>(number_of_channels),
                       static_cast<size_t>(kRenderBufferSizeFrames),
                       GetPreferredOutputSampleType(), ShellAudioBus::kPlanar),
      output_frame_buffer_(
          new uint8[kFramesPerChannel * number_of_channels_ *
                    GetStarboardSampleTypeSize(output_sample_type_)]),
      frames_rendered_(0),
      frames_consumed_(0),
      was_silence_last_update_(false),
      audio_sink_(kSbAudioSinkInvalid) {
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
      kFramesPerChannel, &AudioDevice::Impl::UpdateSourceStatusFunc,
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

// static
void AudioDevice::Impl::ConsumeFramesFunc(int frames_consumed, void* context) {
  AudioDevice::Impl* impl = reinterpret_cast<AudioDevice::Impl*>(context);
  DCHECK(impl);

  impl->ConsumeFrames(frames_consumed);
}

void AudioDevice::Impl::UpdateSourceStatus(int* frames_in_buffer,
                                           int* offset_in_frames,
                                           bool* is_playing,
                                           bool* is_eos_reached) {
  TRACE_EVENT0("cobalt::audio", "AudioDevice::Impl::UpdateSourceStatus()");
  *is_playing = true;
  *is_eos_reached = false;

  // Assert that we never consume more than we've rendered.
  DCHECK_GE(frames_rendered_, frames_consumed_);
  *frames_in_buffer = static_cast<int>(frames_rendered_ - frames_consumed_);

  if ((kFramesPerChannel - *frames_in_buffer) >= kRenderBufferSizeFrames) {
    // If there was silence last time we were called, then the buffer has
    // already been zeroed out and we don't need to do it again.
    if (!was_silence_last_update_) {
      input_audio_bus_.ZeroAllFrames();
    }

    bool silence = true;

    // Fill our temporary buffer with planar PCM float samples.
    render_callback_->FillAudioBus(&input_audio_bus_, &silence);

    if (!silence) {
      FillOutputAudioBus();

      frames_rendered_ += kRenderBufferSizeFrames;
      *frames_in_buffer += kRenderBufferSizeFrames;
    }

    was_silence_last_update_ = silence;
  }

  *offset_in_frames = frames_consumed_ % kFramesPerChannel;
  *is_playing = (frames_rendered_ != frames_consumed_);
}

void AudioDevice::Impl::ConsumeFrames(int frames_consumed) {
  frames_consumed_ += frames_consumed;
}

template <typename InputType, typename OutputType>
inline void AudioDevice::Impl::FillOutputAudioBusForType() {
  // Determine the offset into the audio bus that represents the tail of
  // buffered data.
  uint64 channel_offset = frames_rendered_ % kFramesPerChannel;

  OutputType* output_buffer =
      reinterpret_cast<OutputType*>(output_frame_buffer_.get());
  output_buffer += channel_offset * number_of_channels_;
  for (size_t frame = 0; frame < kRenderBufferSizeFrames; ++frame) {
    for (size_t channel = 0; channel < input_audio_bus_.channels(); ++channel) {
      *output_buffer = ConvertSample<InputType, OutputType>(
          input_audio_bus_
              .GetSampleForType<InputType, media::ShellAudioBus::kPlanar>(
                  channel, frame));
      ++output_buffer;
    }
  }
}

void AudioDevice::Impl::FillOutputAudioBus() {
  TRACE_EVENT0("cobalt::audio", "AudioDevice::Impl::FillOutputAudioBus()");

  const bool is_input_int16 =
      input_audio_bus_.sample_type() == media::ShellAudioBus::kInt16;
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

#else  // defined(SB_USE_SB_AUDIO_SINK)

class AudioDevice::Impl : public ::media::ShellAudioStream {
 public:
  typedef ::media::AudioBus AudioBus;
  typedef ::media::AudioParameters AudioParameters;

  Impl(int32 number_of_channels, RenderCallback* callback);
  virtual ~Impl();

  // ShellAudioStream implementation.
  bool PauseRequested() const override;
  bool PullFrames(uint32* offset_in_frame, uint32* total_frames) override;
  void ConsumeFrames(uint32 frame_played) override;
  const AudioParameters& GetAudioParameters() const override;
  AudioBus* GetAudioBus() override;

 private:
  typedef ::media::ShellAudioBus ShellAudioBus;

  int GetAudioHardwareSampleRate();

  void FillOutputAudioBus();

  AudioParameters audio_parameters_;
  scoped_ptr<AudioBus> output_audio_bus_;

  uint64 rendered_frame_cursor_;
  uint64 buffered_frame_cursor_;
  bool needs_data_;

  // Buffer the audio data which is pulled from upper level.
  ShellAudioBus audio_bus_;

  RenderCallback* render_callback_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

// AudioDevice::Impl.
AudioDevice::Impl::Impl(int32 number_of_channels, RenderCallback* callback)
    : rendered_frame_cursor_(0),
      buffered_frame_cursor_(0),
      needs_data_(true),
      audio_bus_(static_cast<size_t>(number_of_channels),
                 static_cast<size_t>(kRenderBufferSizeFrames),
                 ShellAudioBus::kFloat32, ShellAudioBus::kPlanar),
      render_callback_(callback) {
  TRACE_EVENT0("cobalt::audio", "AudioDevice::Impl::Impl()");

  DCHECK_GT(number_of_channels, 0);
  DCHECK(media::ShellAudioStreamer::Instance()->GetConfig().interleaved())
      << "Planar audio is not supported.";

  int bytes_per_sample = static_cast<int>(
      media::ShellAudioStreamer::Instance()->GetConfig().bytes_per_sample());

  DCHECK_EQ(bytes_per_sample, sizeof(float))
      << bytes_per_sample << " bytes per sample is not supported.";

  media::ChannelLayout channel_layout = number_of_channels == 1
                                            ? media::CHANNEL_LAYOUT_MONO
                                            : media::CHANNEL_LAYOUT_STEREO;

  audio_parameters_ =
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LINEAR,
                             channel_layout, GetAudioHardwareSampleRate(),
                             bytes_per_sample * 8, kRenderBufferSizeFrames);

  // Create 1 channel audio bus since we only support interleaved.
  output_audio_bus_ =
      AudioBus::Create(1, kFramesPerChannel * number_of_channels);

  audio_bus_.ZeroAllFrames();

  media::ShellAudioStreamer::Instance()->AddStream(this);
}

AudioDevice::Impl::~Impl() {
  media::ShellAudioStreamer::Instance()->RemoveStream(this);
}

bool AudioDevice::Impl::PauseRequested() const { return needs_data_; }

bool AudioDevice::Impl::PullFrames(uint32* offset_in_frame,
                                   uint32* total_frames) {
  TRACE_EVENT0("cobalt::audio", "AudioDevice::Impl::PullFrames()");

  // In case offset_in_frame or total_frames is NULL.
  uint32 dummy_offset_in_frame;
  uint32 dummy_total_frames;
  if (!offset_in_frame) {
    offset_in_frame = &dummy_offset_in_frame;
  }
  if (!total_frames) {
    total_frames = &dummy_total_frames;
  }

  // Assert that we never render more than has been buffered.
  DCHECK_GE(buffered_frame_cursor_, rendered_frame_cursor_);
  *total_frames =
      static_cast<uint32>(buffered_frame_cursor_ - rendered_frame_cursor_);

  if ((kFramesPerChannel - *total_frames) >= kRenderBufferSizeFrames) {
    // Fill our temporary buffer with PCM float samples.
    bool silence = true;
    render_callback_->FillAudioBus(&audio_bus_, &silence);

    if (!silence) {
      FillOutputAudioBus();

      buffered_frame_cursor_ += kRenderBufferSizeFrames;
      *total_frames += kRenderBufferSizeFrames;
    }
  }

  needs_data_ = *total_frames < kRenderBufferSizeFrames;
  *offset_in_frame = rendered_frame_cursor_ % kFramesPerChannel;
  return !PauseRequested();
}

void AudioDevice::Impl::ConsumeFrames(uint32 frame_played) {
  // Increment number of frames rendered by the hardware.
  rendered_frame_cursor_ += frame_played;
}

const AudioDevice::Impl::AudioParameters&
AudioDevice::Impl::GetAudioParameters() const {
  return audio_parameters_;
}

AudioDevice::Impl::AudioBus* AudioDevice::Impl::GetAudioBus() {
  return output_audio_bus_.get();
}

int AudioDevice::Impl::GetAudioHardwareSampleRate() {
  int native_output_sample_rate =
      static_cast<int>(media::ShellAudioStreamer::Instance()
                           ->GetConfig()
                           .native_output_sample_rate());
  if (native_output_sample_rate !=
      media::ShellAudioStreamer::Config::kInvalidSampleRate) {
    return native_output_sample_rate;
  }
  return kStandardOutputSampleRate;
}

void AudioDevice::Impl::FillOutputAudioBus() {
  TRACE_EVENT0("cobalt::audio", "AudioDevice::Impl::FillOutputAudioBus()");
  // Determine the offset into the audio bus that represents the tail of
  // buffered data.
  uint64 channel_offset = buffered_frame_cursor_ % kFramesPerChannel;

  float* output_buffer = output_audio_bus_->channel(0);
  output_buffer += channel_offset * audio_parameters_.channels();

  for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
    for (size_t c = 0; c < audio_bus_.channels(); ++c) {
      *output_buffer = audio_bus_.GetFloat32Sample(c, i);
      ++output_buffer;
    }
  }

  // Clear the data in audio bus.
  audio_bus_.ZeroAllFrames();
}

#endif  // defined(SB_USE_SB_AUDIO_SINK)

// AudioDevice.
AudioDevice::AudioDevice(int32 number_of_channels, RenderCallback* callback)
    : impl_(new Impl(number_of_channels, callback)) {}

AudioDevice::~AudioDevice() {}

}  // namespace audio
}  // namespace cobalt
