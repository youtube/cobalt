// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/android/shared/audio_renderer_sink_android.h"
#include "starboard/android/shared/media_agency.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

using ::starboard::android::shared::MediaAgency;

AudioRendererSinkAndroid::AudioRendererSinkAndroid()
    : audio_sink_(kSbAudioSinkInvalid),
      render_callback_(NULL),
      playback_rate_(1.0),
      volume_(1.0) {}

AudioRendererSinkAndroid::~AudioRendererSinkAndroid() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  Stop();
}

bool AudioRendererSinkAndroid::IsAudioSampleTypeSupported(
    SbMediaAudioSampleType audio_sample_type) const {
  return SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type);
}

bool AudioRendererSinkAndroid::IsAudioFrameStorageTypeSupported(
    SbMediaAudioFrameStorageType audio_frame_storage_type) const {
  return SbAudioSinkIsAudioFrameStorageTypeSupported(audio_frame_storage_type);
}

int AudioRendererSinkAndroid::GetNearestSupportedSampleFrequency(
    int sampling_frequency_hz) const {
  return SbAudioSinkGetNearestSupportedSampleFrequency(sampling_frequency_hz);
}

bool AudioRendererSinkAndroid::HasStarted() const {
  return SbAudioSinkIsValid(audio_sink_);
}

void AudioRendererSinkAndroid::Start(
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

  render_callback_ = render_callback;
  Stop();
  UpdatePlaybackStatusIfPossible();
  audio_sink_ = kSbAudioSinkInvalid;
  SbAudioSinkPrivate::Type* audio_sink_type =
      SbAudioSinkPrivate::GetPreferredType();
  if (audio_sink_type) {
    audio_sink_ = audio_sink_type->Create(
        channels, sampling_frequency_hz, audio_sample_type,
        audio_frame_storage_type, frame_buffers, frames_per_channel,
        &AudioRendererSinkAndroid::UpdateSourceStatusFunc,
        &AudioRendererSinkAndroid::ConsumeFramesFunc,
        &AudioRendererSinkAndroid::ErrorFunc, this);
    if (!audio_sink_type->IsValid(audio_sink_)) {
      SB_LOG(WARNING) << "Created invalid SbAudioSink from "
                         "SbAudioSinkPrivate::Type. Destroying and "
                         "resetting.";
      audio_sink_type->Destroy(audio_sink_);
      audio_sink_ = kSbAudioSinkInvalid;
      auto fallback_type = SbAudioSinkPrivate::GetFallbackType();
      if (fallback_type) {
        audio_sink_ = fallback_type->Create(
            channels, sampling_frequency_hz, audio_sample_type,
            audio_frame_storage_type, frame_buffers, frames_per_channel,
            &AudioRendererSinkAndroid::UpdateSourceStatusFunc,
            &AudioRendererSinkAndroid::ConsumeFramesFunc,
            &AudioRendererSinkAndroid::ErrorFunc, this);
        if (!fallback_type->IsValid(audio_sink_)) {
          SB_LOG(ERROR) << "Failed to create SbAudioSink from Fallback type.";
          fallback_type->Destroy(audio_sink_);
          audio_sink_ = kSbAudioSinkInvalid;
        }
      }
    }
  }
  if (!SbAudioSinkIsValid(audio_sink_)) {
    return;
  }
  // TODO: Remove SetPlaybackRate() support from audio sink as it only need to
  // support play/pause.
  audio_sink_->SetPlaybackRate(playback_rate_);
  audio_sink_->SetVolume(volume_);
}

void AudioRendererSinkAndroid::Stop() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (HasStarted()) {
    SbAudioSinkDestroy(audio_sink_);
    audio_sink_ = kSbAudioSinkInvalid;
    render_callback_ = NULL;
  }
}

void AudioRendererSinkAndroid::SetVolume(double volume) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  volume_ = volume;
  if (HasStarted()) {
    audio_sink_->SetVolume(volume);
  }
}

void AudioRendererSinkAndroid::UpdatePlaybackStatusIfPossible() {
  if (render_callback_ == NULL) {
    return;
  }

  int frames_in_buffer;
  int offset_in_frames;
  bool is_playing;
  bool is_eos_reached;
  SbTime seek_to_time;
  SbTime frames_sent_to_sink_in_time;
  double real_playback_rate;

  render_callback_->GetSourceStatus(
      &frames_in_buffer, &offset_in_frames, &is_playing, &is_eos_reached,
      &seek_to_time, &frames_sent_to_sink_in_time, &real_playback_rate);

  int tunneling_audio_session_id =
      MediaAgency::GetInstance()->GetAudioConfigByAudioSinkContext(this);
  if (tunneling_audio_session_id == -1) {
    return;
  }

  MediaAgency::GetInstance()->SetPlaybackStatus(
      tunneling_audio_session_id, seek_to_time, frames_sent_to_sink_in_time,
      real_playback_rate);
}

void AudioRendererSinkAndroid::SetPlaybackRate(double playback_rate) {
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
  UpdatePlaybackStatusIfPossible();
}

// static
void AudioRendererSinkAndroid::UpdateSourceStatusFunc(int* frames_in_buffer,
                                                      int* offset_in_frames,
                                                      bool* is_playing,
                                                      bool* is_eos_reached,
                                                      void* context) {
  AudioRendererSinkAndroid* audio_renderer_sink =
      static_cast<AudioRendererSinkAndroid*>(context);
  SB_DCHECK(audio_renderer_sink);
  SB_DCHECK(audio_renderer_sink->render_callback_);
  SB_DCHECK(frames_in_buffer);
  SB_DCHECK(offset_in_frames);
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_reached);

  SbTime seek_time_us;
  SbTime frames_sent_to_sink_in_time;
  double real_playback_rate;
  audio_renderer_sink->render_callback_->GetSourceStatus(
      frames_in_buffer, offset_in_frames, is_playing, is_eos_reached,
      &seek_time_us, &frames_sent_to_sink_in_time, &real_playback_rate);
}

// static
void AudioRendererSinkAndroid::ConsumeFramesFunc(int frames_consumed,
                                                 SbTime frames_consumed_at,
                                                 void* context) {
  AudioRendererSinkAndroid* audio_renderer_sink =
      static_cast<AudioRendererSinkAndroid*>(context);
  SB_DCHECK(audio_renderer_sink);
  SB_DCHECK(audio_renderer_sink->render_callback_);

  audio_renderer_sink->render_callback_->ConsumeFrames(
      frames_consumed,
#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
      kSbHasAsyncAudioFramesReporting ? frames_consumed_at
                                      : (SbTime)kSbTimeMax);
#else
      frames_consumed_at);
#endif
}

// static
void AudioRendererSinkAndroid::ErrorFunc(bool capability_changed,
                                         void* context) {
  AudioRendererSinkAndroid* audio_renderer_sink =
      static_cast<AudioRendererSinkAndroid*>(context);
  SB_DCHECK(audio_renderer_sink);
  SB_DCHECK(audio_renderer_sink->render_callback_);

  audio_renderer_sink->render_callback_->OnError(capability_changed);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
