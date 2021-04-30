// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"

#include <string>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

AudioRendererSinkImpl::AudioRendererSinkImpl()
    : create_audio_sink_func_(
          [](SbTime start_media_time,
             int channels,
             int sampling_frequency_hz,
             SbMediaAudioSampleType audio_sample_type,
             SbMediaAudioFrameStorageType audio_frame_storage_type,
             SbAudioSinkFrameBuffers frame_buffers,
             int frame_buffers_size_in_frames,
             SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
             SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
#if SB_API_VERSION >= 12
             SbAudioSinkPrivate::ErrorFunc error_func,
#endif  // SB_API_VERSION >= 12
             void* context) {
            return SbAudioSinkPrivate::Create(
                channels, sampling_frequency_hz, audio_sample_type,
                audio_frame_storage_type, frame_buffers,
                frame_buffers_size_in_frames, update_source_status_func,
                consume_frames_func,
#if SB_API_VERSION >= 12
                error_func,
#endif  // SB_API_VERSION >= 12
                context);
          }) {
}

AudioRendererSinkImpl::AudioRendererSinkImpl(
    CreateAudioSinkFunc create_audio_sink_func)
    : create_audio_sink_func_(create_audio_sink_func) {
  SB_DCHECK(create_audio_sink_func_);
}

AudioRendererSinkImpl::~AudioRendererSinkImpl() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  Stop();
}

bool AudioRendererSinkImpl::IsAudioSampleTypeSupported(
    SbMediaAudioSampleType audio_sample_type) const {
  return SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type);
}

bool AudioRendererSinkImpl::IsAudioFrameStorageTypeSupported(
    SbMediaAudioFrameStorageType audio_frame_storage_type) const {
  return SbAudioSinkIsAudioFrameStorageTypeSupported(audio_frame_storage_type);
}

int AudioRendererSinkImpl::GetNearestSupportedSampleFrequency(
    int sampling_frequency_hz) const {
  return SbAudioSinkGetNearestSupportedSampleFrequency(sampling_frequency_hz);
}

bool AudioRendererSinkImpl::HasStarted() const {
  return SbAudioSinkIsValid(audio_sink_);
}

void AudioRendererSinkImpl::Start(
    SbTime media_start_time,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    RenderCallback* render_callback) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(!HasStarted());
  SB_DCHECK(channels > 0 && channels <= SbAudioSinkGetMaxChannels());
  SB_DCHECK(sampling_frequency_hz > 0);
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type));
  SB_DCHECK(
      SbAudioSinkIsAudioFrameStorageTypeSupported(audio_frame_storage_type));
  SB_DCHECK(frame_buffers);
  SB_DCHECK(frames_per_channel > 0);

  Stop();
  render_callback_ = render_callback;
  audio_sink_ = create_audio_sink_func_(
      media_start_time, channels, sampling_frequency_hz, audio_sample_type,
      audio_frame_storage_type, frame_buffers, frames_per_channel,
      &AudioRendererSinkImpl::UpdateSourceStatusFunc,
      &AudioRendererSinkImpl::ConsumeFramesFunc,
#if SB_API_VERSION >= 12
      &AudioRendererSinkImpl::ErrorFunc,
#endif  // SB_API_VERSION >= 12
      this);
  if (!SbAudioSinkIsValid(audio_sink_)) {
    return;
  }
  // TODO: Remove SetPlaybackRate() support from audio sink as it only need to
  // support play/pause.
  audio_sink_->SetPlaybackRate(playback_rate_);
  audio_sink_->SetVolume(volume_);
}

void AudioRendererSinkImpl::Stop() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (HasStarted()) {
    SbAudioSinkDestroy(audio_sink_);
    audio_sink_ = kSbAudioSinkInvalid;
    render_callback_ = NULL;
  }
}

void AudioRendererSinkImpl::SetVolume(double volume) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  volume_ = volume;
  if (HasStarted()) {
    audio_sink_->SetVolume(volume);
  }
}

void AudioRendererSinkImpl::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(playback_rate == 0.0 || playback_rate == 1.0)
      << "Playback rate on audio sink can only be set to 0 or 1, "
      << "but is now set to " << playback_rate;

  playback_rate_ = playback_rate;
  if (HasStarted()) {
    // TODO: Remove SetPlaybackRate() support from audio sink as it only need to
    // support play/pause.
    audio_sink_->SetPlaybackRate(playback_rate);
  }
}

// static
void AudioRendererSinkImpl::UpdateSourceStatusFunc(int* frames_in_buffer,
                                                   int* offset_in_frames,
                                                   bool* is_playing,
                                                   bool* is_eos_reached,
                                                   void* context) {
  AudioRendererSinkImpl* audio_renderer_sink =
      static_cast<AudioRendererSinkImpl*>(context);
  SB_DCHECK(audio_renderer_sink);
  SB_DCHECK(audio_renderer_sink->render_callback_);
  SB_DCHECK(frames_in_buffer);
  SB_DCHECK(offset_in_frames);
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_reached);

  audio_renderer_sink->render_callback_->GetSourceStatus(
      frames_in_buffer, offset_in_frames, is_playing, is_eos_reached);
}

// static
void AudioRendererSinkImpl::ConsumeFramesFunc(int frames_consumed,
                                              SbTime frames_consumed_at,
                                              void* context) {
  AudioRendererSinkImpl* audio_renderer_sink =
      static_cast<AudioRendererSinkImpl*>(context);
  SB_DCHECK(audio_renderer_sink);
  SB_DCHECK(audio_renderer_sink->render_callback_);

  audio_renderer_sink->render_callback_->ConsumeFrames(frames_consumed,
                                                       frames_consumed_at);
}

// static
void AudioRendererSinkImpl::ErrorFunc(bool capability_changed,
                                      const std::string& error_message,
                                      void* context) {
  AudioRendererSinkImpl* audio_renderer_sink =
      static_cast<AudioRendererSinkImpl*>(context);
  SB_DCHECK(audio_renderer_sink);
  SB_DCHECK(audio_renderer_sink->render_callback_);

  audio_renderer_sink->render_callback_->OnError(capability_changed,
                                                 error_message);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
