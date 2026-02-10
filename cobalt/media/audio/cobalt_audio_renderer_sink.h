// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_AUDIO_COBALT_AUDIO_RENDERER_SINK_H_
#define COBALT_MEDIA_AUDIO_COBALT_AUDIO_RENDERER_SINK_H_

#include <atomic>
#include <memory>

#include "media/base/audio_renderer_sink.h"
#include "media/base/media_export.h"
#include "media/base/multi_channel_resampler.h"
#include "starboard/audio_sink.h"

namespace media {

// Custom deleter for Starboard Audio Sink.
struct SbAudioSinkDeleter {
  using pointer = SbAudioSink;
  void operator()(SbAudioSink sink) const;
};

class MEDIA_EXPORT CobaltAudioRendererSink final : public AudioRendererSink {
 public:
  CobaltAudioRendererSink() = default;
  ~CobaltAudioRendererSink() final = default;

  CobaltAudioRendererSink(const CobaltAudioRendererSink&) = delete;
  CobaltAudioRendererSink& operator=(const CobaltAudioRendererSink&) = delete;

  // AudioRendererSink implementation.
  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) final;
  void Start() final;
  void Stop() final;
  void Play() final;
  void Pause() final;
  void Flush() final;
  bool SetVolume(double volume) final;
  OutputDeviceInfo GetOutputDeviceInfo() final;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) final;
  bool IsOptimizedForHardwareParameters() final;
  bool CurrentThreadIsRenderingThread() final;

 private:
  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames,
                                     bool* is_playing,
                                     bool* is_eos_reached,
                                     void* context);

  static void ConsumeFramesFunc(int frames_consumed, void* context);

  void UpdateSourceStatus(int* frames_in_buffer,
                          int* offset_in_frames,
                          bool* is_playing,
                          bool* is_eos_reached);
  void ConsumeFrames(int frames_consumed);

  void FillOutputAudioBuffer(int num_frames);

  // only needed if we need to resample.
  void ResamplerReadCallback(int frame_delay, AudioBus* output);

  SbMediaAudioSampleType output_sample_type_;
  int frames_per_channel_{0};

  AudioParameters params_;

  std::unique_ptr<AudioBus> input_audio_bus_ = nullptr;

  std::unique_ptr<uint8_t[]> output_frame_buffer_ = nullptr;
  void* output_frame_buffers_[1];

  RenderCallback* callback_ = nullptr;

  int64_t frames_rendered_ = 0;  // Frames retrieved from |callback_|.
  int64_t frames_consumed_ = 0;  // Accumulated frames consumed by the sink.

  std::atomic<bool> is_playing_ = false;
  std::atomic<bool> is_eos_reached_ = false;

  std::unique_ptr<SbAudioSink, SbAudioSinkDeleter> audio_sink_;

  double resample_ratio_ = 1.0;
  std::unique_ptr<MultiChannelResampler> resampler_ = nullptr;
  std::unique_ptr<AudioBus> resampled_audio_bus_ = nullptr;
};

}  // namespace media

#endif  // COBALT_MEDIA_AUDIO_COBALT_AUDIO_RENDERER_SINK_H_
