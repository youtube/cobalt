// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/benchmark/audio_sink/audio_sink_benchmark_helper.h"

#include <cstddef>

#include "starboard/common/check_op.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {

AudioSinkConfigSupport GetAudioSinkConfigSupport(
    const AudioSinkBenchmarkConfig& config) {
  if (config.channels > SbAudioSinkGetMaxChannels()) {
    return AudioSinkConfigSupport::kTooManyChannels;
  }
  if (!SbAudioSinkIsAudioSampleTypeSupported(config.sample_type)) {
    return AudioSinkConfigSupport::kSampleTypeUnsupported;
  }
  if (!SbAudioSinkIsAudioFrameStorageTypeSupported(
          kSbMediaAudioFrameStorageTypeInterleaved)) {
    return AudioSinkConfigSupport::kInterleavedStorageUnsupported;
  }
  if (SbAudioSinkGetNearestSupportedSampleFrequency(
          config.sampling_frequency_hz) != config.sampling_frequency_hz) {
    return AudioSinkConfigSupport::kSampleRateUnsupported;
  }
  if (SbAudioSinkGetMinBufferSizeInFrames(config.channels, config.sample_type,
                                          config.sampling_frequency_hz) <= 0) {
    return AudioSinkConfigSupport::kMinBufferSizeUnavailable;
  }
  return AudioSinkConfigSupport::kSupported;
}

const char* ToString(AudioSinkConfigSupport support) {
  switch (support) {
    case AudioSinkConfigSupport::kSupported:
      return "Supported";
    case AudioSinkConfigSupport::kTooManyChannels:
      return "Channel count exceeds SbAudioSinkGetMaxChannels()";
    case AudioSinkConfigSupport::kSampleTypeUnsupported:
      return "Sample type is not supported";
    case AudioSinkConfigSupport::kInterleavedStorageUnsupported:
      return "Interleaved frame storage is not supported";
    case AudioSinkConfigSupport::kSampleRateUnsupported:
      return "Sample rate is not supported";
    case AudioSinkConfigSupport::kMinBufferSizeUnavailable:
      return "SbAudioSinkGetMinBufferSizeInFrames() rejected the configuration";
  }
  SB_NOTREACHED();
  return "";
}

AudioSinkBenchmarkFrameBuffers::AudioSinkBenchmarkFrameBuffers(
    const AudioSinkBenchmarkConfig& config)
    : frames_per_channel_(
          SbAudioSinkGetMinBufferSizeInFrames(config.channels,
                                              config.sample_type,
                                              config.sampling_frequency_hz)) {
  SB_CHECK_GT(frames_per_channel_, 0);
  frame_buffer_.resize(static_cast<size_t>(frames_per_channel_) *
                       config.channels * GetBytesPerSample(config.sample_type));
  frame_buffers_[0] = frame_buffer_.data();
}

void AudioSinkBenchmarkUpdateSourceStatusFunc(int* frames_in_buffer,
                                              int* offset_in_frames,
                                              bool* is_playing,
                                              bool* is_eos_reached,
                                              void* context) {
  *frames_in_buffer = 0;
  *offset_in_frames = 0;
  *is_playing = false;
  *is_eos_reached = false;
}

void AudioSinkBenchmarkConsumeFramesFunc(int frames_consumed, void* context) {}

}  // namespace starboard
