// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/audio_sink.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

AudioRendererSinkImpl::AudioRendererSinkImpl()
    : audio_sink_(kSbAudioSinkInvalid),
      render_callback_(NULL),
      playback_rate_(1.0),
      volume_(1.0) {}

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
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    RenderCallback* render_callback) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(!HasStarted());

  Stop();
  render_callback_ = render_callback;
  audio_sink_ = SbAudioSinkCreate(
      channels, sampling_frequency_hz, audio_sample_type,
      audio_frame_storage_type, frame_buffers, frames_per_channel,
      &AudioRendererSinkImpl::UpdateSourceStatusFunc,
      &AudioRendererSinkImpl::ConsumeFramesFunc, this);
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
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                              SbTime frames_consumed_at,
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                              void* context) {
  AudioRendererSinkImpl* audio_renderer_sink =
      static_cast<AudioRendererSinkImpl*>(context);
  SB_DCHECK(audio_renderer_sink);
  SB_DCHECK(audio_renderer_sink->render_callback_);

#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  audio_renderer_sink->render_callback_->ConsumeFrames(frames_consumed,
                                                       frames_consumed_at);
#else   // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  audio_renderer_sink->render_callback_->ConsumeFrames(frames_consumed);
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
