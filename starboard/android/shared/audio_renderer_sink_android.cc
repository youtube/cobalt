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

#include "starboard/android/shared/audio_renderer_sink_android.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "starboard/android/shared/audio_output_manager.h"
#include "starboard/android/shared/audio_sink_android.h"
#include "starboard/android/shared/audio_track_audio_sink_type.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/pointer_arithmetic.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
namespace {

using jni_zero::AttachCurrentThread;

AudioRendererSinkImpl::CreateAudioSinkFunc GetDefaultCreateAudioSinkFunc(
    std::optional<int> tunnel_mode_audio_session_id,
    bool allow_audio_writing_on_pause,
    bool pause_using_audio_track_state) {
  return [=](int64_t start_media_time, int channels, int sampling_frequency_hz,
             SbMediaAudioSampleType audio_sample_type,
             SbAudioSinkFrameBuffers frame_buffers,
             int frame_buffers_size_in_frames,
             SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
             SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
             SbAudioSinkPrivate::ErrorFunc error_func, void* context) {
    auto type = static_cast<AudioTrackAudioSinkType*>(
        SbAudioSinkImpl::GetPreferredType());
    SB_CHECK(type);

    return type->Create(
        channels, sampling_frequency_hz, audio_sample_type, frame_buffers,
        frame_buffers_size_in_frames,
        {update_source_status_func, consume_frames_func, error_func},
        start_media_time, tunnel_mode_audio_session_id,
        /*is_web_audio=*/false, allow_audio_writing_on_pause,
        pause_using_audio_track_state, context);
  };
}

}  // namespace

AudioRendererSinkAndroid::AudioRendererSinkAndroid(
    std::optional<int> tunnel_mode_audio_session_id,
    bool allow_audio_writing_on_pause,
    bool enable_video_renderer_vsp_adjustment,
    bool allow_flush_during_seek,
    bool pause_using_audio_track_state,
    CreateAudioSinkFunc create_audio_sink_func)
    : AudioRendererSinkImpl(
          create_audio_sink_func
              ? std::move(create_audio_sink_func)
              : GetDefaultCreateAudioSinkFunc(tunnel_mode_audio_session_id,
                                              allow_audio_writing_on_pause,
                                              pause_using_audio_track_state)),
      is_tunnel_mode_enabled_(tunnel_mode_audio_session_id.has_value()),
      enable_video_renderer_vsp_adjustment_(
          enable_video_renderer_vsp_adjustment),
      allow_flush_during_seek_(allow_flush_during_seek) {}

bool AudioRendererSinkAndroid::AllowOverflowAudioSamples() const {
  return is_tunnel_mode_enabled_;
}

bool AudioRendererSinkAndroid::AllowDirectPlaybackRateSetting() const {
  return is_tunnel_mode_enabled_ && !enable_video_renderer_vsp_adjustment_;
}

bool AudioRendererSinkAndroid::HasStarted() const {
  return !is_flushed_ && AudioRendererSinkImpl::HasStarted();
}

void AudioRendererSinkAndroid::GetAudioRendererParams(
    const AudioStreamInfo& audio_stream_info,
    int* max_cached_frames,
    int* min_frames_per_append) const {
  SB_CHECK(max_cached_frames);
  SB_CHECK(min_frames_per_append);
  *min_frames_per_append =
      AudioRendererSink::kDefaultAudioSinkMinFramesPerAppend;

  // AudioRenderer prefers to use kSbMediaAudioSampleTypeFloat32 and only uses
  // kSbMediaAudioSampleTypeInt16Deprecated when float32 is not supported.
  const auto sample_type =
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)
          ? kSbMediaAudioSampleTypeFloat32
          : kSbMediaAudioSampleTypeInt16Deprecated;

  int min_frames_required = SbAudioSinkGetMinBufferSizeInFrames(
      audio_stream_info.number_of_channels, sample_type,
      audio_stream_info.samples_per_second);

  if (is_tunnel_mode_enabled_) {
    // AudioTrack.setPlaybackParams() might need extra buffer to support
    // playback speed greater than 1.0x.
    const double kMaxPlaybackSpeed = 2.0;
    JNIEnv* env = AttachCurrentThread();
    min_frames_required = std::max<int>(
        min_frames_required,
        AudioOutputManager::GetInstance()->GetMinBufferSizeInFrames(
            env, sample_type, audio_stream_info.number_of_channels,
            audio_stream_info.samples_per_second) *
            kMaxPlaybackSpeed);
  }

  // On Android 5.0, the size of audio renderer sink buffer need to be two
  // times larger than AudioTrack minBufferSize. Otherwise, AudioTrack may
  // stop working after pause.
  *max_cached_frames = min_frames_required * 2 +
                       AudioRendererSink::kDefaultAudioSinkMinFramesPerAppend;
  *max_cached_frames =
      AlignUp(*max_cached_frames, AudioRendererSink::kAudioSinkFramesAlignment);
}

void AudioRendererSinkAndroid::Start(int64_t media_start_time,
                                     int channels,
                                     int sampling_frequency_hz,
                                     SbMediaAudioSampleType audio_sample_type,
                                     SbAudioSinkFrameBuffers frame_buffers,
                                     int frames_per_channel,
                                     RenderCallback* render_callback) {
  is_flushed_ = false;
  // Re-use the existing audio sink if the new audio parameters match the
  // existing ones. Otherwise, fall back to the default behavior of destroying
  // and re-creating the sink.
  const bool is_android_sink =
      audio_sink_ && audio_sink_->IsType(SbAudioSinkImpl::GetPreferredType());
  if (allow_flush_during_seek_ && is_android_sink && channels == channels_ &&
      sampling_frequency_hz == sampling_frequency_hz_ &&
      audio_sample_type == audio_sample_type_) {
    SB_LOG(INFO) << "Audio sink is already started with the same config, "
                 << "skipping Start().";
    auto* android_sink = static_cast<AudioSinkAndroid*>(audio_sink_);
    android_sink->SetStartTime(media_start_time);
    // Explicitly set the playback rate and volume because HasStarted()
    // returns false while in the flushed state, causing the renderer to
    // skip updating the sink with these parameters during seek.
    android_sink->SetPlaybackRate(playback_rate_);
    android_sink->SetVolume(volume_);
    render_callback_ = render_callback;
    return;
  }

  channels_ = channels;
  sampling_frequency_hz_ = sampling_frequency_hz;
  audio_sample_type_ = audio_sample_type;

  AudioRendererSinkImpl::Start(
      media_start_time, channels, sampling_frequency_hz, audio_sample_type,
      frame_buffers, frames_per_channel, render_callback);
}

bool AudioRendererSinkAndroid::IsAudioSampleTypeSupported(
    SbMediaAudioSampleType audio_sample_type) const {
  if (is_tunnel_mode_enabled_) {
    // Currently the implementation only supports tunnel mode with int16 audio
    // samples.
    return audio_sample_type == kSbMediaAudioSampleTypeInt16Deprecated;
  }

  return SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type);
}

void AudioRendererSinkAndroid::Reset() {
  bool is_android_sink =
      audio_sink_ && audio_sink_->IsType(SbAudioSinkImpl::GetPreferredType());
  if (allow_flush_during_seek_ && is_android_sink) {
    auto* android_sink = static_cast<AudioSinkAndroid*>(audio_sink_);
    if (android_sink->Flush()) {
      SB_LOG(INFO) << "Flushing audio sink.";
      is_flushed_ = true;
      return;
    }
  }
  SB_LOG(INFO) << "Resetting audio sink.";
  is_flushed_ = false;
  AudioRendererSink::Reset();
}

void AudioRendererSinkAndroid::Stop() {
  is_flushed_ = false;
  AudioRendererSinkImpl::Stop();
}

}  // namespace starboard
