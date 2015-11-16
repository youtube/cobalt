/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/audio/audio_device.h"

#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/shell_audio_streamer.h"
#include "media/base/audio_bus.h"

namespace cobalt {
namespace audio {

namespace {
const int kRenderBufferSizeFrames = 1024;
const int kFramesPerChannel = kRenderBufferSizeFrames * 4;
const int kStandardOutputSampleRate = 48000;
}  // namespace

class AudioDevice::Impl : public ::media::ShellAudioStream {
 public:
  typedef ::media::AudioBus AudioBus;
  typedef ::media::AudioParameters AudioParameters;

  Impl(int32 number_of_channels, RenderCallback* callback);
  virtual ~Impl();

  // ShellAudioStream implementation.
  bool PauseRequested() const OVERRIDE;
  bool PullFrames(uint32* offset_in_frame, uint32* total_frames) OVERRIDE;
  void ConsumeFrames(uint32 frame_played) OVERRIDE;
  const AudioParameters& GetAudioParameters() const OVERRIDE;
  AudioBus* GetAudioBus() OVERRIDE;

 private:
  int GetAudioHardwareSampleRate();

  void FillOutputAudioBus();

  AudioParameters audio_parameters_;
  scoped_ptr<AudioBus> output_audio_bus_;

  uint64 rendered_frame_cursor_;
  uint64 buffered_frame_cursor_;
  bool needs_data_;

  // Buffer the audio data which is pulled from upper level.
  std::vector<std::vector<float> > render_buffers_;

  RenderCallback* render_callback_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

// AudioDevice::Impl.
AudioDevice::Impl::Impl(int32 number_of_channels, RenderCallback* callback)
    : rendered_frame_cursor_(0),
      buffered_frame_cursor_(0),
      needs_data_(true),
      render_callback_(callback) {
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

  // Create 1 channel audio bus due to we only support interleaved.
  output_audio_bus_ =
      AudioBus::Create(1, kFramesPerChannel * number_of_channels);

  render_buffers_.resize(static_cast<size_t>(number_of_channels));
  for (size_t i = 0; i < static_cast<size_t>(number_of_channels); ++i) {
    render_buffers_[i].resize(kRenderBufferSizeFrames);
  }

  media::ShellAudioStreamer::Instance()->AddStream(this);
}

AudioDevice::Impl::~Impl() {
  media::ShellAudioStreamer::Instance()->RemoveStream(this);
}

bool AudioDevice::Impl::PauseRequested() const { return needs_data_; }

bool AudioDevice::Impl::PullFrames(uint32* offset_in_frame,
                                   uint32* total_frames) {
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
    bool silence = false;
    render_callback_->FillAudioBuffer(kRenderBufferSizeFrames, &render_buffers_,
                                      &silence);

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
  // Determine the offset into the audio bus that represents the tail of
  // buffered data.
  uint64 channel_offset = buffered_frame_cursor_ % kFramesPerChannel;

  float* output_buffer = output_audio_bus_->channel(0);
  output_buffer += channel_offset * audio_parameters_.channels();

  for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
    for (size_t c = 0; c < static_cast<size_t>(audio_parameters_.channels());
         ++c) {
      *output_buffer = render_buffers_[c][i];
      ++output_buffer;
    }
  }

  // Clear the data in the render buffers.
  for (size_t c = 0; c < static_cast<size_t>(audio_parameters_.channels());
       ++c) {
    memset(&render_buffers_[c][0], 0,
           render_buffers_[c].size() * sizeof(float));
  }
}

// AudioDevice.
AudioDevice::AudioDevice(int32 number_of_channels, RenderCallback* callback)
    : impl_(new Impl(number_of_channels, callback)) {}

AudioDevice::~AudioDevice() {}

}  // namespace audio
}  // namespace cobalt
