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
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
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
    const AudioStreamInfo& audio_stream_info,
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
      allow_flush_during_seek_(allow_flush_during_seek),
      platform_required_format_(
          is_tunnel_mode_enabled_
              ? std::make_optional(GetPlatformRequiredFormat(audio_stream_info))
              : std::nullopt) {}

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
  // Note that only int16 is supported in tunnel mode.
  const auto sample_type =
      IsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)
          ? kSbMediaAudioSampleTypeFloat32
          : kSbMediaAudioSampleTypeInt16Deprecated;
  const int output_channels =
      GetOutputNumberOfChannels(audio_stream_info.number_of_channels);
  const int output_sampling_frequency_hz =
      GetNearestSupportedSampleFrequency(audio_stream_info.samples_per_second);

  int min_frames_required = SbAudioSinkGetMinBufferSizeInFrames(
      output_channels, sample_type, output_sampling_frequency_hz);

  if (is_tunnel_mode_enabled_) {
    // AudioTrack.setPlaybackParams() might need extra buffer to support
    // playback speed greater than 1.0x.
    const double kMaxPlaybackSpeed = 2.0;
    JNIEnv* env = AttachCurrentThread();
    min_frames_required = std::max<int>(
        min_frames_required,
        AudioOutputManager::GetInstance()->GetMinBufferSizeInFrames(
            env, sample_type, output_channels, output_sampling_frequency_hz) *
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

int AudioRendererSinkAndroid::GetOutputNumberOfChannels(
    int number_of_channels) const {
  if (platform_required_format_) {
    return platform_required_format_->channels;
  }
  return number_of_channels;
}

void AudioRendererSinkAndroid::Start(int64_t media_start_time,
                                     int channels,
                                     int sampling_frequency_hz,
                                     SbMediaAudioSampleType audio_sample_type,
                                     SbAudioSinkFrameBuffers frame_buffers,
                                     int frames_per_channel,
                                     RenderCallback* render_callback) {
  // Re-use the existing audio sink if the new audio parameters match the
  // existing ones. Otherwise, fall back to the default behavior of destroying
  // and re-creating the sink.
  const AudioFormat audio_format = {channels, audio_sample_type,
                                    sampling_frequency_hz};
  if (allow_flush_during_seek_ && audio_sink_ &&
      audio_format == current_audio_format_) {
    SB_LOG(INFO) << "Audio sink is already started with the same config, "
                 << "skipping Start().";
    // |audio_sink_| is always an AudioSinkAndroid (an AudioTrackAudioSink by
    // default), so the static_cast below (and in Reset()) is safe.
    auto* android_sink = static_cast<AudioSinkAndroid*>(audio_sink_);
    android_sink->SetStartTime(media_start_time);
    // Explicitly set the playback rate and volume because HasStarted()
    // returns false while in the flushed state, causing the renderer to
    // skip updating the sink with these parameters during seek.
    android_sink->SetPlaybackRate(playback_rate_);
    android_sink->SetVolume(volume_);
    render_callback_ = render_callback;
    is_flushed_ = false;
    return;
  }

  if (is_flushed_) {
    Stop();
    is_flushed_ = false;
  }

  current_audio_format_ = audio_format;

  AudioRendererSinkImpl::Start(
      media_start_time, channels, sampling_frequency_hz, audio_sample_type,
      frame_buffers, frames_per_channel, render_callback);
}

void AudioRendererSinkAndroid::Reset() {
  if (allow_flush_during_seek_ && audio_sink_) {
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

bool AudioRendererSinkAndroid::IsAudioSampleTypeSupported(
    SbMediaAudioSampleType audio_sample_type) const {
  if (platform_required_format_) {
    return audio_sample_type == platform_required_format_->sample_type;
  }

  return SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type);
}

int AudioRendererSinkAndroid::GetNearestSupportedSampleFrequency(
    int sampling_frequency_hz) const {
  if (platform_required_format_) {
    return platform_required_format_->sampling_frequency_hz;
  }

  return SbAudioSinkGetNearestSupportedSampleFrequency(sampling_frequency_hz);
}

// static
AudioRendererSinkAndroid::AudioFormat
AudioRendererSinkAndroid::GetPlatformRequiredFormat(
    const AudioStreamInfo& audio_stream_info) {
  // Currently the implementation only supports tunnel mode with int16 audio
  // samples.
  const SbMediaAudioSampleType sample_type =
      kSbMediaAudioSampleTypeInt16Deprecated;
  const int encoding =
      GetAudioFormatSampleType(kSbMediaAudioCodingTypePcm, sample_type);

  // Used when none of the formats below is reported as supported.
  const AudioFormat kFallbackFormat = {
      .channels = 2,
      .sample_type = sample_type,
      .sampling_frequency_hz = 48000,
  };

  // In non-tunnel mode, Android's AudioMixer (MixerThread) automatically
  // upmixes mono to stereo and resamples to supported sampling rates in
  // software. In tunnel mode (FLAG_HW_AV_SYNC), the software mixer is
  // bypassed, and PCM buffers are routed directly to the hardware via
  // DirectOutputThread. So the audio may have to be converted to a format that
  // the hardware supports before being written to the audio sink.
  // The formats are tried in order:
  //   1. The original number of channels and sampling rate.
  //   2. Stereo with the original sampling rate, if the original audio isn't
  //      stereo.
  // If neither is supported, |kFallbackFormat| is used.
  const int channels = audio_stream_info.number_of_channels;
  const int sampling_frequency_hz =
      static_cast<int>(audio_stream_info.samples_per_second);
  MediaCapabilitiesCache* cache = MediaCapabilitiesCache::GetInstance();

  if (cache->IsTunneledAudioSupported(encoding, sampling_frequency_hz,
                                      channels)) {
    return {
        .channels = channels,
        .sample_type = sample_type,
        .sampling_frequency_hz = sampling_frequency_hz,
    };
  }

  if (channels != kFallbackFormat.channels &&
      cache->IsTunneledAudioSupported(encoding, sampling_frequency_hz,
                                      kFallbackFormat.channels)) {
    return {
        .channels = kFallbackFormat.channels,
        .sample_type = sample_type,
        .sampling_frequency_hz = sampling_frequency_hz,
    };
  }

  return kFallbackFormat;
}

}  // namespace starboard
