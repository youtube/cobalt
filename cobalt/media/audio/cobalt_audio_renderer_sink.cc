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

#include "cobalt/media/audio/cobalt_audio_renderer_sink.h"

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "cobalt/media/audio/audio_helpers.h"
#include "media/base/audio_glitch_info.h"

namespace media {

namespace {
const int kDefaultFramesPerRenderBufferPadding = 2;
const int kDefaultFramesPerRenderBufferPaddingMax = 8;

int AlignUp(int value, int alignment) {
  int decremented_value = value - 1;
  return decremented_value + alignment - (decremented_value % alignment);
}
}  // namespace

void SbAudioSinkDeleter::operator()(SbAudioSink sink) const {
  if (SbAudioSinkIsValid(sink)) {
    SbAudioSinkDestroy(sink);
  }
}

void CobaltAudioRendererSink::Initialize(const AudioParameters& params,
                                         RenderCallback* callback) {
  LOG(INFO) << "CobaltAudioRendererSink::Initialize - called with following "
               "parameters:"
            << params.AsHumanReadableString();
  params_ = params;
  output_sample_type_ = GetPreferredOutputStarboardSampleType();

  DCHECK(callback);
  DCHECK(!callback_);
  callback_ = callback;
}

void CobaltAudioRendererSink::Start() {
  DCHECK(callback_);
  int frames_per_render_buffer = params_.frames_per_buffer();

  frames_per_channel_ = std::max(
      AlignUp(
          SbAudioSinkGetMinBufferSizeInFrames(
              params_.channels(), output_sample_type_, params_.sample_rate()) +
              frames_per_render_buffer * kDefaultFramesPerRenderBufferPadding,
          frames_per_render_buffer),
      frames_per_render_buffer * kDefaultFramesPerRenderBufferPaddingMax);

  input_audio_bus_ = AudioBus::Create(params_);

  output_frame_buffer_ = std::make_unique<uint8_t[]>(
      frames_per_channel_ * params_.channels() *
      GetStarboardSampleTypeSize(output_sample_type_));

  DCHECK(frames_per_channel_ % frames_per_render_buffer == 0);

  int nearest_supported_sample_rate_ =
      SbAudioSinkGetNearestSupportedSampleFrequency(params_.sample_rate());
  if (nearest_supported_sample_rate_ != params_.sample_rate()) {
    resample_ratio_ = static_cast<double>(params_.sample_rate() /
                                          nearest_supported_sample_rate_);
    resampler_ = std::make_unique<MultiChannelResampler>(
        params_.channels(), resample_ratio_, frames_per_render_buffer,
        base::BindRepeating(&CobaltAudioRendererSink::ResamplerReadCallback,
                            base::Unretained(this)));
    resampler_->PrimeWithSilence();
    resampled_audio_bus_ = AudioBus::Create(params_);
  }

  output_frame_buffers_[0] = output_frame_buffer_.get();
  audio_sink_ = AudioSinkUniquePtr(SbAudioSinkCreate(
      params_.channels(), nearest_supported_sample_rate_, output_sample_type_,
      kSbMediaAudioFrameStorageTypeInterleaved, &output_frame_buffers_[0],
      frames_per_channel_, &CobaltAudioRendererSink::UpdateSourceStatusFunc,
      &CobaltAudioRendererSink::ConsumeFramesFunc, /*context=*/this));
  CHECK(audio_sink_);
}

void CobaltAudioRendererSink::Stop() {
  is_eos_reached_.store(true);
  audio_sink_.reset();
  callback_ = nullptr;
}

void CobaltAudioRendererSink::Play() {
  is_playing_.store(true);
}

void CobaltAudioRendererSink::Pause() {
  is_playing_.store(false);
}

void CobaltAudioRendererSink::Flush() {
  if (!audio_sink_) {
    return;
  }
  // TODO(b/390468794) consolidate this recreation as it duplicates code from
  // Stop() and Start()
  audio_sink_.reset();
  frames_rendered_ = 0;
  frames_consumed_ = 0;
  audio_sink_ = AudioSinkUniquePtr(SbAudioSinkCreate(
      params_.channels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(params_.sample_rate()),
      output_sample_type_, kSbMediaAudioFrameStorageTypeInterleaved,
      &output_frame_buffers_[0], frames_per_channel_,
      &CobaltAudioRendererSink::UpdateSourceStatusFunc,
      &CobaltAudioRendererSink::ConsumeFramesFunc, /*context=*/this));
  CHECK(audio_sink_);
}

bool CobaltAudioRendererSink::SetVolume(double volume) {
  LOG(INFO) << "SetVolume - NOT IMPL";
  NOTIMPLEMENTED();
  return false;
}

OutputDeviceInfo CobaltAudioRendererSink::GetOutputDeviceInfo() {
  return GetPreferredOutputParameters();
}

void CobaltAudioRendererSink::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(info_cb), GetOutputDeviceInfo()))
      .Run();
}

bool CobaltAudioRendererSink::IsOptimizedForHardwareParameters() {
  LOG(INFO) << "IsOptimizedForHardwareParameters - NOT IMPL";
  NOTIMPLEMENTED();
  return false;
}

bool CobaltAudioRendererSink::CurrentThreadIsRenderingThread() {
  LOG(INFO) << "CurrentThreadIsRenderingThread - NOT IMPL";
  NOTIMPLEMENTED();
  return true;
}

// static
void CobaltAudioRendererSink::UpdateSourceStatusFunc(int* frames_in_buffer,
                                                     int* offset_in_frames,
                                                     bool* is_playing,
                                                     bool* is_eos_reached,
                                                     void* context) {
  CobaltAudioRendererSink* sink =
      static_cast<CobaltAudioRendererSink*>(context);
  DCHECK(sink);
  DCHECK(frames_in_buffer);
  DCHECK(offset_in_frames);
  DCHECK(is_playing);
  DCHECK(is_eos_reached);
  sink->UpdateSourceStatus(frames_in_buffer, offset_in_frames, is_playing,
                           is_eos_reached);
}

// static
void CobaltAudioRendererSink::ConsumeFramesFunc(int frames_consumed,
                                                void* context) {
  CobaltAudioRendererSink* sink =
      static_cast<CobaltAudioRendererSink*>(context);
  DCHECK(sink);
  sink->ConsumeFrames(frames_consumed);
}

void CobaltAudioRendererSink::UpdateSourceStatus(int* frames_in_buffer,
                                                 int* offset_in_frames,
                                                 bool* is_playing,
                                                 bool* is_eos_reached) {
  // Assert that we never consume more than we've rendered.
  DCHECK_GE(frames_rendered_, frames_consumed_);
  DCHECK(input_audio_bus_);

  *is_playing = is_playing_.load();
  *is_eos_reached = is_eos_reached_.load();

  *frames_in_buffer = static_cast<int>(frames_rendered_ - frames_consumed_);

  while ((frames_per_channel_ - *frames_in_buffer) >=
         params_.frames_per_buffer()) {
    int frames_filled =
        callback_->Render(base::TimeDelta(), base::TimeTicks(),
                          /*glitch_info=*/{}, input_audio_bus_.get());

    if (frames_filled == 0) {
      break;
    }

    DCHECK_EQ(params_.frames_per_buffer(), frames_filled);
    FillOutputAudioBuffer(frames_filled);

    frames_rendered_ += frames_filled;
    *frames_in_buffer += frames_filled;
  }
  *offset_in_frames = frames_consumed_ % frames_per_channel_;
}

void CobaltAudioRendererSink::ConsumeFrames(int frames_consumed) {
  frames_consumed_ += frames_consumed;
}

void CobaltAudioRendererSink::FillOutputAudioBuffer(int num_frames) {
  uint64_t channel_offset = frames_rendered_ % frames_per_channel_;
  AudioBus* bus = input_audio_bus_.get();

  if (resampler_) {
    bus = resampled_audio_bus_.get();
    resampler_->Resample(num_frames, bus);
  }

  if (output_sample_type_ == kSbMediaAudioSampleTypeFloat32) {
    float* output_buffer = reinterpret_cast<float*>(output_frame_buffer_.get());
    output_buffer += channel_offset * params_.channels();
    bus->ToInterleaved<Float32SampleTypeTraitsNoClip>(num_frames,
                                                      output_buffer);
  } else {
    int16_t* output_buffer =
        reinterpret_cast<int16_t*>(output_frame_buffer_.get());
    output_buffer += channel_offset * params_.channels();
    bus->ToInterleaved<SignedInt16SampleTypeTraits>(num_frames, output_buffer);
  }
}

void CobaltAudioRendererSink::ResamplerReadCallback(int frame_delay,
                                                    AudioBus* output) {
  input_audio_bus_->CopyTo(output);
}

}  // namespace media
