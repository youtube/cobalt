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

#ifndef STARBOARD_ANDROID_SHARED_PLAYER_COMPONENTS_FACTORY_H_
#define STARBOARD_ANDROID_SHARED_PLAYER_COMPONENTS_FACTORY_H_

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starboard/android/shared/audio_decoder.h"
#include "starboard/android/shared/audio_output_manager.h"
#include "starboard/android/shared/audio_renderer_passthrough.h"
#include "starboard/android/shared/audio_track_audio_sink_type.h"
#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_decoder.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard {
namespace {

using base::android::AttachCurrentThread;
using features::FeatureList;

// On some platforms tunnel mode is only supported in the secure pipeline.  Set
// the following variable to true to force creating a secure pipeline in tunnel
// mode, even for clear content.
// TODO: Allow this to be configured per playback at run time from the web app.
constexpr bool kForceSecurePipelineInTunnelModeWhenRequired = true;

// This class allows us to force int16 sample type when tunnel mode is enabled.
class AudioRendererSinkAndroid : public AudioRendererSinkImpl {
 public:
  explicit AudioRendererSinkAndroid(int tunnel_mode_audio_session_id = -1)
      : AudioRendererSinkImpl(
            [=](int64_t start_media_time,
                int channels,
                int sampling_frequency_hz,
                SbMediaAudioSampleType audio_sample_type,
                SbMediaAudioFrameStorageType audio_frame_storage_type,
                SbAudioSinkFrameBuffers frame_buffers,
                int frame_buffers_size_in_frames,
                SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
                SbAudioSinkPrivate::ErrorFunc error_func,
                void* context) {
              auto type = static_cast<AudioTrackAudioSinkType*>(
                  SbAudioSinkImpl::GetPreferredType());

              return type->Create(
                  channels, sampling_frequency_hz, audio_sample_type,
                  audio_frame_storage_type, frame_buffers,
                  frame_buffers_size_in_frames, update_source_status_func,
                  consume_frames_func, error_func, start_media_time,
                  tunnel_mode_audio_session_id, false, /* is_web_audio */
                  context);
            }),
        tunnel_mode_audio_session_id_(tunnel_mode_audio_session_id) {}

  bool AllowOverflowAudioSamples() const override {
    return tunnel_mode_audio_session_id_ != -1;
  }

 private:
  bool IsAudioSampleTypeSupported(
      SbMediaAudioSampleType audio_sample_type) const override {
    if (tunnel_mode_audio_session_id_ != -1) {
      // Currently the implementation only supports tunnel mode with int16 audio
      // samples.
      return audio_sample_type == kSbMediaAudioSampleTypeInt16Deprecated;
    }

    return SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type);
  }

  const int tunnel_mode_audio_session_id_;
};

class AudioRendererSinkCallbackStub : public AudioRendererSink::RenderCallback {
 public:
  bool error_occurred() const { return error_occurred_.load(); }

 private:
  void GetSourceStatus(int* frames_in_buffer,
                       int* offset_in_frames,
                       bool* is_playing,
                       bool* is_eos_reached) override {
    *frames_in_buffer = *offset_in_frames = 0;
    *is_playing = true;
    *is_eos_reached = false;
  }
  void ConsumeFrames(int frames_consumed, int64_t frames_consumed_at) override {
    SB_DCHECK_EQ(frames_consumed, 0);
  }

  void OnError(bool capability_changed,
               const std::string& error_message) override {
    error_occurred_.store(true);
  }

  std::atomic_bool error_occurred_{false};
};

class PlayerComponentsPassthrough : public PlayerComponents {
 public:
  PlayerComponentsPassthrough(
      std::unique_ptr<AudioRendererPassthrough> audio_renderer,
      std::unique_ptr<VideoRenderer> video_renderer)
      : audio_renderer_(std::move(audio_renderer)),
        video_renderer_(std::move(video_renderer)) {}

 private:
  // PlayerComponents methods
  MediaTimeProvider* GetMediaTimeProvider() override {
    return audio_renderer_.get();
  }
  AudioRenderer* GetAudioRenderer() override { return audio_renderer_.get(); }
  VideoRenderer* GetVideoRenderer() override { return video_renderer_.get(); }

  std::unique_ptr<AudioRendererPassthrough> audio_renderer_;
  std::unique_ptr<VideoRenderer> video_renderer_;
};
}  // namespace

class PlayerComponentsFactory : public PlayerComponents::Factory {
  const int kAudioSinkFramesAlignment = 256;
  const int kDefaultAudioSinkMinFramesPerAppend = 1024;

  static int AlignUp(int value, int alignment) {
    return (value + alignment - 1) / alignment * alignment;
  }

  NonNullResult<std::unique_ptr<PlayerComponents>> CreateComponents(
      const CreationParameters& creation_parameters) override {
    if (creation_parameters.audio_codec() != kSbMediaAudioCodecAc3 &&
        creation_parameters.audio_codec() != kSbMediaAudioCodecEac3) {
      SB_LOG(INFO) << "Creating non-passthrough components.";
      return PlayerComponents::Factory::CreateComponents(creation_parameters);
    }

    if (!creation_parameters.audio_mime().empty()) {
      MimeType audio_mime_type(creation_parameters.audio_mime());
      if (!audio_mime_type.is_valid() ||
          !audio_mime_type.ValidateBoolParameter("audiopassthrough")) {
        return Failure("Invalid audio mime type.");
      }
      if (!audio_mime_type.GetParamBoolValue("audiopassthrough", true)) {
        SB_LOG(INFO) << "Mime attribute \"audiopassthrough\" is set to: "
                        "false. Passthrough is disabled.";
        return Failure("Passthrough disabled by mime attribute.");
      }
    }

    bool enable_flush_during_seek =
        FeatureList::IsEnabled(features::kForceFlushDecoderDuringReset);
    SB_LOG_IF(INFO, enable_flush_during_seek)
        << "`kForceFlushDecoderDuringReset` is set to true, force flushing"
        << " audio passthrough decoder during Reset().";

    SB_LOG(INFO) << "Creating passthrough components.";
    // TODO: Enable tunnel mode for passthrough
    auto audio_renderer = std::make_unique<AudioRendererPassthrough>(
        creation_parameters.job_queue(),
        creation_parameters.audio_stream_info(),
        creation_parameters.drm_system(), enable_flush_during_seek);
    if (!audio_renderer->is_valid()) {
      return Failure("Failed to create audio renderer.");
    }

    // Set max_video_input_size with a positive value to overwrite
    // MediaFormat.KEY_MAX_INPUT_SIZE. Use 0 as default value.
    int max_video_input_size = creation_parameters.max_video_input_size();
    SB_LOG_IF(INFO, max_video_input_size > 0)
        << "The maximum size in bytes of a buffer of data is "
        << max_video_input_size;

    std::unique_ptr<VideoRenderer> video_renderer;
    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      constexpr int kTunnelModeAudioSessionId = -1;
      constexpr bool kForceSecurePipelineUnderTunnelMode = false;

      auto video_decoder = CreateVideoDecoder(
          creation_parameters, kTunnelModeAudioSessionId,
          kForceSecurePipelineUnderTunnelMode, max_video_input_size);
      if (video_decoder) {
        auto video_render_algorithm = video_decoder->GetRenderAlgorithm();
        auto video_renderer_sink = video_decoder->GetSink();
        auto media_time_provider = audio_renderer.get();

        video_renderer = std::make_unique<VideoRendererImpl>(
            creation_parameters.job_queue(),
            std::unique_ptr<VideoDecoder>(std::move(video_decoder.value())),
            media_time_provider, std::move(video_render_algorithm),
            video_renderer_sink);
      } else {
        return Failure("Failed to create video decoder: " +
                       video_decoder.error());
      }
    }
    return std::make_unique<PlayerComponentsPassthrough>(
        std::move(audio_renderer), std::move(video_renderer));
  }

  Result<MediaComponents> CreateSubComponents(
      const CreationParameters& creation_parameters) override {
    const std::string audio_mime =
        creation_parameters.audio_codec() != kSbMediaAudioCodecNone
            ? creation_parameters.audio_mime()
            : "";
    MimeType audio_mime_type(audio_mime);
    if (!audio_mime.empty()) {
      if (!audio_mime_type.is_valid()) {
        return Failure("Invalid audio MIME: '" + std::string(audio_mime) + "'");
      }
    }

    const std::string video_mime =
        creation_parameters.video_codec() != kSbMediaVideoCodecNone
            ? creation_parameters.video_mime()
            : "";
    MimeType video_mime_type(video_mime);
    if (!video_mime.empty()) {
      if (!video_mime_type.is_valid() ||
          !video_mime_type.ValidateBoolParameter("tunnelmode")) {
        return Failure("Invalid video MIME: '" + std::string(video_mime) + "'");
      }
    }

    int tunnel_mode_audio_session_id = -1;
    bool enable_tunnel_mode = false;
    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone &&
        creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      enable_tunnel_mode =
          video_mime_type.GetParamBoolValue("tunnelmode", false);

      SB_LOG(INFO) << "Tunnel mode is "
                   << (enable_tunnel_mode ? "enabled. " : "disabled. ")
                   << "Video mime parameter \"tunnelmode\" value: "
                   << video_mime_type.GetParamStringValue("tunnelmode",
                                                          "<not provided>")
                   << ".";
    } else {
      SB_LOG(INFO) << "Tunnel mode requires both an audio and video stream. "
                   << "Audio codec: "
                   << GetMediaAudioCodecName(creation_parameters.audio_codec())
                   << ", Video codec: "
                   << GetMediaVideoCodecName(creation_parameters.video_codec())
                   << ". Tunnel mode is disabled.";
    }

    const bool force_tunnel_mode =
        FeatureList::IsEnabled(features::kForceTunnelMode);

    if (force_tunnel_mode && !enable_tunnel_mode) {
      SB_LOG(INFO)
          << "`force_tunnel_mode` is set to true, force enabling tunnel"
          << " mode.";
      enable_tunnel_mode = true;
    }

    bool force_secure_pipeline_under_tunnel_mode = false;
    if (enable_tunnel_mode) {
      if (IsTunnelModeSupported(creation_parameters,
                                &force_secure_pipeline_under_tunnel_mode)) {
        tunnel_mode_audio_session_id =
            GenerateAudioSessionId(creation_parameters);
        SB_LOG(INFO) << "Generated tunnel mode audio session id "
                     << tunnel_mode_audio_session_id;
      } else {
        SB_LOG(INFO) << "IsTunnelModeSupported() failed, disable tunnel mode.";
      }
    } else {
      SB_LOG(INFO) << "Tunnel mode not enabled.";
    }

    if (tunnel_mode_audio_session_id == -1) {
      SB_LOG(INFO) << "Create non-tunnel mode pipeline.";
    } else {
      SB_LOG(INFO) << "Create tunnel mode pipeline with audio session id "
                   << tunnel_mode_audio_session_id << '.';
    }

    bool enable_reset_audio_decoder =
        FeatureList::IsEnabled(features::kForceResetAudioDecoder);
    SB_LOG_IF(INFO, enable_reset_audio_decoder)
        << "`kForceResetAudioDecoder` is set to true, force resetting"
        << " audio decoder during Reset().";

    bool enable_flush_during_seek =
        FeatureList::IsEnabled(features::kForceFlushDecoderDuringReset);
    SB_LOG_IF(INFO, enable_flush_during_seek)
        << "`kForceFlushDecoderDuringReset` is set to true, force flushing"
        << " audio decoder during Reset().";

    MediaComponents components;
    JobQueue* job_queue = creation_parameters.job_queue();

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      const bool enable_platform_opus_decoder =
          FeatureList::IsEnabled(features::kForcePlatformOpusDecoder);
      SB_LOG_IF(INFO, enable_platform_opus_decoder)
          << "kForcePlatformOpusDecoder is set to true, force using "
          << "platform opus codec instead of libopus.";
      auto decoder_creator =
          [enable_flush_during_seek, enable_platform_opus_decoder, job_queue](
              const AudioStreamInfo& audio_stream_info,
              SbDrmSystem drm_system) -> std::unique_ptr<AudioDecoder> {
        bool use_libopus_decoder =
            audio_stream_info.codec == kSbMediaAudioCodecOpus &&
            !SbDrmSystemIsValid(drm_system) && !enable_platform_opus_decoder;
        if (use_libopus_decoder) {
          auto audio_decoder_impl =
              std::make_unique<OpusAudioDecoder>(job_queue, audio_stream_info);
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl;
          }
        } else if (audio_stream_info.codec == kSbMediaAudioCodecAac ||
                   audio_stream_info.codec == kSbMediaAudioCodecOpus) {
          auto audio_decoder_impl = std::make_unique<MediaCodecAudioDecoder>(
              job_queue, audio_stream_info, drm_system,
              enable_flush_during_seek);
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl;
          }
        } else {
          SB_LOG(ERROR) << "Unsupported audio codec "
                        << audio_stream_info.codec;
        }
        return nullptr;
      };

      components.audio.decoder = std::make_unique<AdaptiveAudioDecoder>(
          job_queue, creation_parameters.audio_stream_info(),
          creation_parameters.drm_system(), decoder_creator,
          enable_reset_audio_decoder);

      if (tunnel_mode_audio_session_id != -1) {
        components.audio.renderer_sink = TryToCreateTunnelModeAudioRendererSink(
            tunnel_mode_audio_session_id, creation_parameters);
        if (!components.audio.renderer_sink) {
          tunnel_mode_audio_session_id = -1;
        }
      }
      if (!components.audio.renderer_sink) {
        components.audio.renderer_sink =
            std::make_unique<AudioRendererSinkAndroid>();
      }
    }

    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      // Set max_video_input_size with a positive value to overwrite
      // MediaFormat.KEY_MAX_INPUT_SIZE. Use 0 as default value.
      int max_video_input_size = creation_parameters.max_video_input_size();
      SB_LOG_IF(INFO, max_video_input_size > 0)
          << "The maximum size in bytes of a buffer of data is "
          << max_video_input_size;

      if (tunnel_mode_audio_session_id == -1) {
        force_secure_pipeline_under_tunnel_mode = false;
      }

      auto video_decoder_result = CreateVideoDecoder(
          creation_parameters, tunnel_mode_audio_session_id,
          force_secure_pipeline_under_tunnel_mode, max_video_input_size);
      if (video_decoder_result) {
        auto video_decoder_impl = std::move(video_decoder_result.value());
        components.video.render_algorithm =
            video_decoder_impl->GetRenderAlgorithm();
        components.video.renderer_sink = video_decoder_impl->GetSink();
        components.video.decoder = std::move(video_decoder_impl);
      } else {
        return Failure("Failed to create video decoder: " +
                       video_decoder_result.error());
      }
    }

    return components;
  }

  void GetAudioRendererParams(const CreationParameters& creation_parameters,
                              int* max_cached_frames,
                              int* min_frames_per_append) const override {
    SB_CHECK(max_cached_frames);
    SB_CHECK(min_frames_per_append);
    SB_DCHECK(kDefaultAudioSinkMinFramesPerAppend % kAudioSinkFramesAlignment ==
              0);
    *min_frames_per_append = kDefaultAudioSinkMinFramesPerAppend;

    // AudioRenderer prefers to use kSbMediaAudioSampleTypeFloat32 and only uses
    // kSbMediaAudioSampleTypeInt16Deprecated when float32 is not supported.
    int min_frames_required = SbAudioSinkGetMinBufferSizeInFrames(
        creation_parameters.audio_stream_info().number_of_channels,
        SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)
            ? kSbMediaAudioSampleTypeFloat32
            : kSbMediaAudioSampleTypeInt16Deprecated,
        creation_parameters.audio_stream_info().samples_per_second);
    // On Android 5.0, the size of audio renderer sink buffer need to be two
    // times larger than AudioTrack minBufferSize. Otherwise, AudioTrack may
    // stop working after pause.
    *max_cached_frames =
        min_frames_required * 2 + kDefaultAudioSinkMinFramesPerAppend;
    *max_cached_frames = AlignUp(*max_cached_frames, kAudioSinkFramesAlignment);
  }

  NonNullResult<std::unique_ptr<MediaCodecVideoDecoder>> CreateVideoDecoder(
      const CreationParameters& creation_parameters,
      int tunnel_mode_audio_session_id,
      bool force_secure_pipeline_under_tunnel_mode,
      int max_video_input_size) {
    bool force_big_endian_hdr_metadata = false;
    bool enable_flush_during_seek =
        FeatureList::IsEnabled(features::kForceFlushDecoderDuringReset);
    int64_t flush_delay_usec = features::kFlushDelayUsec.Get();
    int64_t reset_delay_usec = features::kResetDelayUsec.Get();
    // The default value of |force_reset_surface| would be true.
    bool force_reset_surface = true;
    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone &&
        !creation_parameters.video_mime().empty()) {
      // Use mime param to determine endianness of HDR metadata. If param is
      // missing or invalid it defaults to Little Endian.
      MimeType video_mime_type(creation_parameters.video_mime());
      if (video_mime_type.ValidateStringParameter("hdrinfoendianness",
                                                  "big|little")) {
        const std::string& hdr_info_endianness =
            video_mime_type.GetParamStringValue("hdrinfoendianness",
                                                /*default=*/"little");
        force_big_endian_hdr_metadata = hdr_info_endianness == "big";
      }
      if (video_mime_type.ValidateBoolParameter("forceresetsurface")) {
        force_reset_surface =
            video_mime_type.GetParamBoolValue("forceresetsurface", true);
      }
    }

    SB_LOG_IF(INFO, enable_flush_during_seek)
        << "`kForceFlushDecoderDuringReset` is set to true, force flushing"
        << " video decoder during Reset().";
    SB_LOG_IF(INFO, flush_delay_usec > 0)
        << "`kFlushDelayUsec` is set to > 0, force a delay of "
        << flush_delay_usec << "us during Flush().";
    SB_LOG_IF(INFO, reset_delay_usec > 0)
        << "`kResetDelayUsec` is set to > 0, force a delay of "
        << reset_delay_usec << "us during Reset().";

    std::string error_message;
    auto video_decoder = std::make_unique<MediaCodecVideoDecoder>(
        creation_parameters.job_queue(),
        creation_parameters.video_stream_info(),
        creation_parameters.drm_system(), creation_parameters.output_mode(),
        creation_parameters.decode_target_graphics_context_provider(),
        creation_parameters.max_video_capabilities(),
        tunnel_mode_audio_session_id, force_secure_pipeline_under_tunnel_mode,
        force_reset_surface, force_big_endian_hdr_metadata,
        max_video_input_size, creation_parameters.surface_view(),
        enable_flush_during_seek, reset_delay_usec, flush_delay_usec,
        &error_message);
    if (!error_message.empty()) {
      return Failure(error_message);
    }
    if (creation_parameters.video_codec() != kSbMediaVideoCodecAv1 &&
        !video_decoder->is_decoder_created()) {
      return Failure(
          "Video decoder was not created, but no error message was provided.");
    }
    return video_decoder;
  }

  bool IsTunnelModeSupported(const CreationParameters& creation_parameters,
                             bool* force_secure_pipeline_under_tunnel_mode) {
    SB_DCHECK(force_secure_pipeline_under_tunnel_mode);
    *force_secure_pipeline_under_tunnel_mode = false;

    if (!SbAudioSinkIsAudioSampleTypeSupported(
            kSbMediaAudioSampleTypeInt16Deprecated)) {
      SB_LOG(INFO) << "Disable tunnel mode because int16 sample is required "
                      "but not supported.";
      return false;
    }

    if (creation_parameters.output_mode() != kSbPlayerOutputModePunchOut) {
      SB_LOG(INFO)
          << "Disable tunnel mode because output mode is not punchout.";
      return false;
    }

    if (creation_parameters.audio_codec() == kSbMediaAudioCodecNone) {
      SB_LOG(INFO) << "Disable tunnel mode because audio codec is none.";
      return false;
    }

    if (creation_parameters.video_codec() == kSbMediaVideoCodecNone) {
      SB_LOG(INFO) << "Disable tunnel mode because video codec is none.";
      return false;
    }

    const char* mime =
        SupportedVideoCodecToMimeType(creation_parameters.video_codec());
    if (!mime) {
      SB_LOG(INFO) << "Disable tunnel mode because "
                   << creation_parameters.video_codec() << " is not supported.";
      return false;
    }
    DrmSystem* drm_system_ptr =
        static_cast<DrmSystem*>(creation_parameters.drm_system());
    jobject j_media_crypto =
        drm_system_ptr ? drm_system_ptr->GetMediaCrypto() : NULL;

    bool is_encrypted = !!j_media_crypto;
    if (MediaCapabilitiesCache::GetInstance()->HasVideoDecoderFor(
            mime, is_encrypted, false, true, 0, 0, 0, 0)) {
      return true;
    }

    if (kForceSecurePipelineInTunnelModeWhenRequired && !is_encrypted) {
      const bool kIsEncrypted = true;
      auto support_tunnel_mode_under_secure_pipeline =
          MediaCapabilitiesCache::GetInstance()->HasVideoDecoderFor(
              mime, kIsEncrypted, false, true, 0, 0, 0, 0);
      if (support_tunnel_mode_under_secure_pipeline) {
        *force_secure_pipeline_under_tunnel_mode = true;
        return true;
      }
    }

    SB_LOG(INFO) << "Disable tunnel mode because no tunneled decoder for "
                 << mime << '.';
    return false;
  }

  int GenerateAudioSessionId(const CreationParameters& creation_parameters) {
    bool force_secure_pipeline_under_tunnel_mode = false;
    SB_DCHECK(IsTunnelModeSupported(creation_parameters,
                                    &force_secure_pipeline_under_tunnel_mode));

    JNIEnv* env = AttachCurrentThread();
    int tunnel_mode_audio_session_id =
        AudioOutputManager::GetInstance()->GenerateTunnelModeAudioSessionId(
            env, creation_parameters.audio_stream_info().number_of_channels);

    // AudioManager.generateAudioSessionId() return ERROR (-1) to indicate a
    // failure, please see the following url for more details:
    // https://developer.android.com/reference/android/media/AudioManager#generateAudioSessionId()
    SB_LOG_IF(WARNING, tunnel_mode_audio_session_id == -1)
        << "Failed to generate audio session id for tunnel mode.";

    return tunnel_mode_audio_session_id;
  }

  std::unique_ptr<AudioRendererSink> TryToCreateTunnelModeAudioRendererSink(
      int tunnel_mode_audio_session_id,
      const CreationParameters& creation_parameters) {
    std::unique_ptr<AudioRendererSink> audio_sink =
        std::make_unique<AudioRendererSinkAndroid>(
            tunnel_mode_audio_session_id);
    // We need to double check if the audio sink can actually be created.
    int max_cached_frames, min_frames_per_append;
    GetAudioRendererParams(creation_parameters, &max_cached_frames,
                           &min_frames_per_append);
    AudioRendererSinkCallbackStub callback_stub;
    std::vector<uint16_t> frame_buffer(
        max_cached_frames *
        creation_parameters.audio_stream_info().number_of_channels);
    uint16_t* frame_buffers[] = {frame_buffer.data()};
    audio_sink->Start(
        0, creation_parameters.audio_stream_info().number_of_channels,
        creation_parameters.audio_stream_info().samples_per_second,
        kSbMediaAudioSampleTypeInt16Deprecated,
        kSbMediaAudioFrameStorageTypeInterleaved,
        reinterpret_cast<SbAudioSinkFrameBuffers>(frame_buffers),
        max_cached_frames, &callback_stub);
    if (audio_sink->HasStarted() && !callback_stub.error_occurred()) {
      audio_sink->Stop();
      return audio_sink;
    }
    SB_LOG(WARNING)
        << "AudioTrack does not support tunnel mode with sample rate:"
        << creation_parameters.audio_stream_info().samples_per_second
        << ", channels:"
        << creation_parameters.audio_stream_info().number_of_channels
        << ", audio format:" << creation_parameters.audio_codec()
        << ", and audio buffer frames:" << max_cached_frames;
    return nullptr;
  }
};

// static
std::unique_ptr<PlayerComponents::Factory> PlayerComponents::Factory::Create() {
  return std::make_unique<PlayerComponentsFactory>();
}

// static
bool PlayerComponents::Factory::OutputModeSupported(
    SbPlayerOutputMode output_mode,
    SbMediaVideoCodec codec,
    SbDrmSystem drm_system) {
  if (output_mode == kSbPlayerOutputModePunchOut) {
    return true;
  }

  if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
    if (!SbDrmSystemIsValid(drm_system)) {
      return true;
    }
    DrmSystem* android_drm_system = static_cast<DrmSystem*>(drm_system);
    bool require_secure_decoder = android_drm_system->require_secured_decoder();
    SB_LOG_IF(INFO, require_secure_decoder)
        << "Output mode under decode-to-texture is not supported due to secure "
           "decoder is required.";
    return !require_secure_decoder;
  }

  return false;
}

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_PLAYER_COMPONENTS_FACTORY_H_
