// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/starboard_audio_renderer_sink.h"

#include "base/logging.h"

#include "media/base/audio_glitch_info.h"

namespace media {

namespace {
int AlignUp(int value, int alignment) {
  int decremented_value = value - 1;
  return decremented_value + alignment - (decremented_value % alignment);
}
}  // namespace

void StarboardAudioRendererSink::Initialize(const AudioParameters& params,
                                            RenderCallback* callback) {
  LOG(INFO) << "StarboardAudioRendererSink::Initialize - called with following "
               "parameters:"
            << params.AsHumanReadableString();
  params_ = params;

  DCHECK(callback);
  {
    base::AutoLock auto_lock(callback_lock_);
    DCHECK(!callback_);
    callback_ = callback;
  }
}

void StarboardAudioRendererSink::Start() {
  CreateStreamSink();
}

void StarboardAudioRendererSink::Stop() {
  is_eos_reached_ = true;
  SbAudioSinkDestroy(audio_sink_);
  {
    base::AutoLock auto_lock(callback_lock_);
    callback_ = nullptr;
  }
}

void StarboardAudioRendererSink::Play() {
  is_playing_ = true;
}

void StarboardAudioRendererSink::Pause() {
  is_playing_ = false;
}
void StarboardAudioRendererSink::Flush() {
  LOG(INFO) << "Flush - NOT IMPL";
}
bool StarboardAudioRendererSink::SetVolume(double volume) {
  LOG(INFO) << "SetVolume - NOT IMPL";
  return true;
}
OutputDeviceInfo StarboardAudioRendererSink::GetOutputDeviceInfo() {
  LOG(INFO) << "GetOutputDeviceInfo - NOT IMPL";
  return media::OutputDeviceInfo(
      std::string(), media::OUTPUT_DEVICE_STATUS_OK,
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(), 48000, 480));
}
void StarboardAudioRendererSink::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  LOG(INFO) << "GetOutputDeviceInfoAsync - NOT IMPL";
}
bool StarboardAudioRendererSink::IsOptimizedForHardwareParameters() {
  LOG(INFO) << "IsOptimizedForHardwareParameters - NOT IMPL";
  return true;
}
bool StarboardAudioRendererSink::CurrentThreadIsRenderingThread() {
  LOG(INFO) << "CurrentThreadIsRenderingThread - NOT IMPL";
  return true;
}

// static
void StarboardAudioRendererSink::UpdateSourceStatusFunc(int* frames_in_buffer,
                                                        int* offset_in_frames,
                                                        bool* is_playing,
                                                        bool* is_eos_reached,
                                                        void* context) {
  StarboardAudioRendererSink* sink =
      static_cast<StarboardAudioRendererSink*>(context);
  DCHECK(sink);
  DCHECK(frames_in_buffer);
  DCHECK(offset_in_frames);
  DCHECK(is_playing);
  DCHECK(is_eos_reached);
  sink->UpdateSourceStatus(frames_in_buffer, offset_in_frames, is_playing,
                           is_eos_reached);
}

// static
void StarboardAudioRendererSink::ConsumeFramesFunc(int frames_consumed,
                                                   void* context) {
  StarboardAudioRendererSink* sink =
      static_cast<StarboardAudioRendererSink*>(context);
  DCHECK(sink);
  sink->ConsumeFrames(frames_consumed);
}

void StarboardAudioRendererSink::UpdateSourceStatus(int* frames_in_buffer,
                                                    int* offset_in_frames,
                                                    bool* is_playing,
                                                    bool* is_eos_reached) {
  *is_playing = is_playing_;
  *is_eos_reached = is_eos_reached_;

  // Assert that we never consume more than we've rendered.
  DCHECK_GE(frames_rendered_, frames_consumed_);
  *frames_in_buffer = static_cast<int>(frames_rendered_ - frames_consumed_);

  DCHECK(input_audio_bus_);
  base::AutoLock lock(callback_lock_);
  if (!callback_) {
    return;
  }

  while ((frames_per_channel_ - *frames_in_buffer) >=
         params_.frames_per_buffer()) {
    int frames_filled =
        callback_->Render(base::TimeDelta(), base::TimeTicks(),
                          /*glitch_info=*/{}, input_audio_bus_.get());
    FillOutputAudioBuffer(frames_filled);

    frames_rendered_ += frames_filled;
    *frames_in_buffer += frames_filled;
  }
  *offset_in_frames = frames_consumed_ % frames_per_channel_;
}

void StarboardAudioRendererSink::ConsumeFrames(int frames_consumed) {
  frames_consumed_ += frames_consumed;
}

void StarboardAudioRendererSink::CreateStreamSink() {
  int frames_per_render_buffer = params_.frames_per_buffer();
  frames_per_channel_ =
      std::max(AlignUp(SbAudioSinkGetMinBufferSizeInFrames(
                           params_.channels(), kSbMediaAudioSampleTypeFloat32,
                           params_.sample_rate()) +
                           frames_per_render_buffer * 2,
                       frames_per_render_buffer),
               frames_per_render_buffer * 8);

  input_audio_bus_ = AudioBus::Create(params_);
  output_frame_buffer_.resize(frames_per_channel_ * params_.channels() *
                              sizeof(float));

  output_frame_buffers_.resize(1);
  output_frame_buffers_[0] = &output_frame_buffer_[0];

  audio_sink_ = SbAudioSinkCreate(
      params_.channels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(params_.sample_rate()),
      kSbMediaAudioSampleTypeFloat32, kSbMediaAudioFrameStorageTypeInterleaved,
      &output_frame_buffers_[0], frames_per_channel_,
      &StarboardAudioRendererSink::UpdateSourceStatusFunc,
      &StarboardAudioRendererSink::ConsumeFramesFunc,
      reinterpret_cast<void*>(this));
  DCHECK(SbAudioSinkIsValid(audio_sink_));
}

void StarboardAudioRendererSink::FillOutputAudioBuffer(int num_frames) {
  uint64_t channel_offset = frames_rendered_ % frames_per_channel_;
  float* output_buffer = reinterpret_cast<float*>(output_frame_buffer_.data());
  output_buffer += channel_offset * params_.channels();
  input_audio_bus_->ToInterleaved<media::Float32SampleTypeTraitsNoClip>(
      num_frames, output_buffer);
}

}  // namespace media
