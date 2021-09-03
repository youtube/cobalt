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

#include <string>
#include <vector>

#include "starboard/android/shared/audio_decoder.h"
#include "starboard/android/shared/audio_renderer_passthrough.h"
#include "starboard/android/shared/audio_track_audio_sink_type.h"
#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_decoder.h"
#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
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
namespace android {
namespace shared {

// Tunnel mode has to be enabled explicitly by the web app via mime attributes
// "tunnelmode", set the following variable to true to force enabling tunnel
// mode on all playbacks.
constexpr bool kForceTunnelMode = false;

// On some platforms tunnel mode is only supported in the secure pipeline.  Set
// the following variable to true to force creating a secure pipeline in tunnel
// mode, even for clear content.
// TODO: Allow this to be configured per playback at run time from the web app.
constexpr bool kForceSecurePipelineInTunnelModeWhenRequired = true;

// This class allows us to force int16 sample type when tunnel mode is enabled.
class AudioRendererSinkAndroid : public ::starboard::shared::starboard::player::
                                     filter::AudioRendererSinkImpl {
 public:
  explicit AudioRendererSinkAndroid(bool enable_audio_device_callback,
                                    int tunnel_mode_audio_session_id = -1)
      : AudioRendererSinkImpl(
            [=](SbTime start_media_time,
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
                  SbAudioSinkPrivate::GetPreferredType());

              return type->Create(
                  channels, sampling_frequency_hz, audio_sample_type,
                  audio_frame_storage_type, frame_buffers,
                  frame_buffers_size_in_frames, update_source_status_func,
                  consume_frames_func, error_func, start_media_time,
                  tunnel_mode_audio_session_id, enable_audio_device_callback,
                  context);
            }) {}

 private:
  bool IsAudioSampleTypeSupported(
      SbMediaAudioSampleType audio_sample_type) const override {
    // Currently the implementation only supports tunnel mode with int16 audio
    // samples.
    return audio_sample_type == kSbMediaAudioSampleTypeInt16Deprecated;
  }
};

class AudioRendererSinkCallbackStub
    : public starboard::shared::starboard::player::filter::AudioRendererSink::
          RenderCallback {
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
  void ConsumeFrames(int frames_consumed, SbTime frames_consumed_at) override {
    SB_DCHECK(frames_consumed == 0);
  }

  void OnError(bool capability_changed,
               const std::string& error_message) override {
    error_occurred_.store(true);
  }

  atomic_bool error_occurred_;
};

class PlayerComponentsPassthrough
    : public starboard::shared::starboard::player::filter::PlayerComponents {
 public:
  PlayerComponentsPassthrough(
      scoped_ptr<AudioRendererPassthrough> audio_renderer,
      scoped_ptr<VideoRenderer> video_renderer)
      : audio_renderer_(audio_renderer.Pass()),
        video_renderer_(video_renderer.Pass()) {}

 private:
  // PlayerComponents methods
  MediaTimeProvider* GetMediaTimeProvider() override {
    return audio_renderer_.get();
  }
  AudioRenderer* GetAudioRenderer() override { return audio_renderer_.get(); }
  VideoRenderer* GetVideoRenderer() override { return video_renderer_.get(); }

  scoped_ptr<AudioRendererPassthrough> audio_renderer_;
  scoped_ptr<VideoRenderer> video_renderer_;
};

// TODO: Invesigate if the implementation of member functions should be moved
//       into .cc file.
class PlayerComponentsFactory : public starboard::shared::starboard::player::
                                    filter::PlayerComponents::Factory {
  typedef starboard::shared::opus::OpusAudioDecoder OpusAudioDecoder;
  typedef starboard::shared::starboard::player::filter::AdaptiveAudioDecoder
      AdaptiveAudioDecoder;
  typedef starboard::shared::starboard::player::filter::AudioDecoder
      AudioDecoderBase;
  typedef starboard::shared::starboard::player::filter::AudioRendererSink
      AudioRendererSink;
  typedef starboard::shared::starboard::player::filter::AudioRendererSinkImpl
      AudioRendererSinkImpl;
  typedef starboard::shared::starboard::player::filter::PlayerComponents
      PlayerComponents;
  typedef starboard::shared::starboard::player::filter::VideoDecoder
      VideoDecoderBase;
  typedef starboard::shared::starboard::player::filter::VideoRenderAlgorithm
      VideoRenderAlgorithmBase;
  typedef starboard::shared::starboard::player::filter::VideoRendererSink
      VideoRendererSink;

  const int kAudioSinkFramesAlignment = 256;
  const int kDefaultAudioSinkMinFramesPerAppend = 1024;
  const int kDefaultAudioSinkMaxCachedFrames =
      8 * kDefaultAudioSinkMinFramesPerAppend;

  virtual SbDrmSystem GetExtendedDrmSystem(SbDrmSystem drm_system) {
    return drm_system;
  }

  static int AlignUp(int value, int alignment) {
    return (value + alignment - 1) / alignment * alignment;
  }

  scoped_ptr<PlayerComponents> CreateComponents(
      const CreationParameters& creation_parameters,
      std::string* error_message) override {
    using starboard::shared::starboard::media::MimeType;
    SB_DCHECK(error_message);

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecAc3 &&
        creation_parameters.audio_codec() != kSbMediaAudioCodecEac3) {
      SB_LOG(INFO) << "Creating non-passthrough components.";
      return PlayerComponents::Factory::CreateComponents(creation_parameters,
                                                         error_message);
    }

    MimeType audio_mime_type(creation_parameters.audio_mime());

    if (strlen(creation_parameters.audio_mime()) > 0) {
      audio_mime_type.RegisterBoolParameter("enableaudiodevicecallback");
      audio_mime_type.RegisterBoolParameter("audiopassthrough");
      if (!audio_mime_type.is_valid()) {
        return scoped_ptr<PlayerComponents>();
      }
    }

    bool enable_audio_device_callback =
        audio_mime_type.GetParamBoolValue("enableaudiodevicecallback", true);
    SB_LOG(INFO) << "AudioDeviceCallback is "
                 << (enable_audio_device_callback ? "enabled." : "disabled.");

    if (!audio_mime_type.GetParamBoolValue("audiopassthrough", true)) {
      SB_LOG(INFO) << "Mime attribute \"audiopassthrough\" is set to: "
                      "false. Passthrough is disabled.";
      return scoped_ptr<PlayerComponents>();
    }

    SB_LOG(INFO) << "Creating passthrough components.";
    // TODO: Enable tunnel mode for passthrough
    scoped_ptr<AudioRendererPassthrough> audio_renderer;
    audio_renderer.reset(new AudioRendererPassthrough(
        creation_parameters.audio_sample_info(),
        GetExtendedDrmSystem(creation_parameters.drm_system()),
        enable_audio_device_callback));
    if (!audio_renderer->is_valid()) {
      return scoped_ptr<PlayerComponents>();
    }

    scoped_ptr<::starboard::shared::starboard::player::filter::VideoRenderer>
        video_renderer;
    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      constexpr int kTunnelModeAudioSessionId = -1;
      constexpr bool kForceSecurePipelineUnderTunnelMode = false;

      scoped_ptr<VideoDecoder> video_decoder = CreateVideoDecoder(
          creation_parameters, kTunnelModeAudioSessionId,
          kForceSecurePipelineUnderTunnelMode, error_message);
      if (video_decoder) {
        using starboard::shared::starboard::player::filter::VideoRendererImpl;

        auto video_render_algorithm = video_decoder->GetRenderAlgorithm();
        auto video_renderer_sink = video_decoder->GetSink();
        auto media_time_provider = audio_renderer.get();

        video_renderer.reset(new VideoRendererImpl(
            scoped_ptr<VideoDecoderBase>(video_decoder.Pass()),
            media_time_provider, video_render_algorithm.Pass(),
            video_renderer_sink));
      } else {
        return scoped_ptr<PlayerComponents>();
      }
    }
    return scoped_ptr<PlayerComponents>(new PlayerComponentsPassthrough(
        audio_renderer.Pass(), video_renderer.Pass()));
  }

  bool CreateSubComponents(
      const CreationParameters& creation_parameters,
      scoped_ptr<AudioDecoderBase>* audio_decoder,
      scoped_ptr<AudioRendererSink>* audio_renderer_sink,
      scoped_ptr<VideoDecoderBase>* video_decoder,
      scoped_ptr<VideoRenderAlgorithmBase>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink,
      std::string* error_message) override {
    using starboard::shared::starboard::media::MimeType;
    SB_DCHECK(error_message);

    const char* audio_mime =
        creation_parameters.audio_codec() != kSbMediaAudioCodecNone
            ? creation_parameters.audio_mime()
            : "";
    MimeType audio_mime_type(audio_mime);
    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone &&
        strlen(creation_parameters.audio_mime()) > 0) {
      audio_mime_type.RegisterBoolParameter("tunnelmode");
      audio_mime_type.RegisterBoolParameter("enableaudiodevicecallback");

      if (!audio_mime_type.is_valid()) {
        *error_message =
            "Invalid audio MIME: '" + std::string(audio_mime) + "'";
        return false;
      }
    }

    const char* video_mime =
        creation_parameters.video_codec() != kSbMediaVideoCodecNone
            ? creation_parameters.video_mime()
            : "";
    MimeType video_mime_type(video_mime);
    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone &&
        strlen(creation_parameters.video_mime()) > 0) {
      video_mime_type.RegisterBoolParameter("tunnelmode");

      if (!video_mime_type.is_valid()) {
        *error_message =
            "Invalid video MIME: '" + std::string(video_mime) + "'";
        return false;
      }
    }

    int tunnel_mode_audio_session_id = -1;
    bool enable_tunnel_mode = false;
    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone &&
        creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      bool enable_tunnel_mode =
          audio_mime_type.GetParamBoolValue("tunnelmode", false) &&
          video_mime_type.GetParamBoolValue("tunnelmode", false);

      if (!enable_tunnel_mode) {
        SB_LOG(INFO) << "Tunnel mode is disabled. "
                     << "Audio mime parameter \"tunnelmode\" value: "
                     << audio_mime_type.GetParamStringValue("tunnelmode",
                                                            "<not provided>")
                     << ", video mime parameter \"tunnelmode\" value: "
                     << video_mime_type.GetParamStringValue("tunnelmode",
                                                            "<not provided>")
                     << ".";
      }
    } else {
      SB_LOG(INFO) << "Tunnel mode requires both an audio and video stream. "
                   << "Audio codec: "
                   << GetMediaAudioCodecName(creation_parameters.audio_codec())
                   << ", Video codec: "
                   << GetMediaVideoCodecName(creation_parameters.video_codec())
                   << ". Tunnel mode is disabled.";
    }

    if (kForceTunnelMode && !enable_tunnel_mode) {
      SB_LOG(INFO) << "`kForceTunnelMode` is set to true, force enabling tunnel"
                   << " mode.";
      enable_tunnel_mode = true;
    }

    bool force_secure_pipeline_under_tunnel_mode = false;
    if (enable_tunnel_mode &&
        IsTunnelModeSupported(creation_parameters,
                              &force_secure_pipeline_under_tunnel_mode)) {
      tunnel_mode_audio_session_id =
          GenerateAudioSessionId(creation_parameters);
    }

    if (tunnel_mode_audio_session_id == -1) {
      SB_LOG(INFO) << "Create non-tunnel mode pipeline.";
    } else {
      SB_LOG(INFO) << "Create tunnel mode pipeline with audio session id "
                   << tunnel_mode_audio_session_id << '.';
    }

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      SB_DCHECK(audio_decoder);
      SB_DCHECK(audio_renderer_sink);

      auto decoder_creator = [](const SbMediaAudioSampleInfo& audio_sample_info,
                                SbDrmSystem drm_system) {
        if (audio_sample_info.codec == kSbMediaAudioCodecAac) {
          scoped_ptr<AudioDecoder> audio_decoder_impl(new AudioDecoder(
              audio_sample_info.codec, audio_sample_info, drm_system));
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl.PassAs<AudioDecoderBase>();
          }
        } else if (audio_sample_info.codec == kSbMediaAudioCodecOpus) {
          scoped_ptr<OpusAudioDecoder> audio_decoder_impl(
              new OpusAudioDecoder(audio_sample_info));
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl.PassAs<AudioDecoderBase>();
          }
        } else {
          SB_LOG(ERROR) << "Unsupported audio codec "
                        << audio_sample_info.codec;
        }
        return scoped_ptr<AudioDecoderBase>();
      };

      audio_decoder->reset(new AdaptiveAudioDecoder(
          creation_parameters.audio_sample_info(),
          GetExtendedDrmSystem(creation_parameters.drm_system()),
          decoder_creator));

      bool enable_audio_device_callback =
          audio_mime_type.GetParamBoolValue("enableaudiodevicecallback", true);
      SB_LOG(INFO) << "AudioDeviceCallback is "
                   << (enable_audio_device_callback ? "enabled." : "disabled.");

      if (tunnel_mode_audio_session_id != -1) {
        *audio_renderer_sink = TryToCreateTunnelModeAudioRendererSink(
            tunnel_mode_audio_session_id, creation_parameters,
            enable_audio_device_callback);
        if (!*audio_renderer_sink) {
          tunnel_mode_audio_session_id = -1;
        }
      }
      if (!*audio_renderer_sink) {
        audio_renderer_sink->reset(
            new AudioRendererSinkAndroid(enable_audio_device_callback));
      }
    }

    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      SB_DCHECK(video_decoder);
      SB_DCHECK(video_render_algorithm);
      SB_DCHECK(video_renderer_sink);
      SB_DCHECK(error_message);

      if (tunnel_mode_audio_session_id == -1) {
        force_secure_pipeline_under_tunnel_mode = false;
      }

      scoped_ptr<VideoDecoder> video_decoder_impl = CreateVideoDecoder(
          creation_parameters, tunnel_mode_audio_session_id,
          force_secure_pipeline_under_tunnel_mode, error_message);
      if (video_decoder_impl) {
        *video_render_algorithm = video_decoder_impl->GetRenderAlgorithm();
        *video_renderer_sink = video_decoder_impl->GetSink();
        video_decoder->reset(video_decoder_impl.release());
      } else {
        video_decoder->reset();
        *video_renderer_sink = NULL;
        return false;
      }
    }

    return true;
  }

  void GetAudioRendererParams(const CreationParameters& creation_parameters,
                              int* max_cached_frames,
                              int* min_frames_per_append) const override {
    SB_DCHECK(max_cached_frames);
    SB_DCHECK(min_frames_per_append);
    SB_DCHECK(kDefaultAudioSinkMinFramesPerAppend % kAudioSinkFramesAlignment ==
              0);
    *min_frames_per_append = kDefaultAudioSinkMinFramesPerAppend;

    // AudioRenderer prefers to use kSbMediaAudioSampleTypeFloat32 and only uses
    // kSbMediaAudioSampleTypeInt16Deprecated when float32 is not supported.
    int min_frames_required = SbAudioSinkGetMinBufferSizeInFrames(
        creation_parameters.audio_sample_info().number_of_channels,
        SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)
            ? kSbMediaAudioSampleTypeFloat32
            : kSbMediaAudioSampleTypeInt16Deprecated,
        creation_parameters.audio_sample_info().samples_per_second);
    // On Android 5.0, the size of audio renderer sink buffer need to be two
    // times larger than AudioTrack minBufferSize. Otherwise, AudioTrack may
    // stop working after pause.
    *max_cached_frames =
        min_frames_required * 2 + kDefaultAudioSinkMinFramesPerAppend;
    *max_cached_frames = AlignUp(*max_cached_frames, kAudioSinkFramesAlignment);
  }

  scoped_ptr<VideoDecoder> CreateVideoDecoder(
      const CreationParameters& creation_parameters,
      int tunnel_mode_audio_session_id,
      bool force_secure_pipeline_under_tunnel_mode,
      std::string* error_message) {
    using starboard::shared::starboard::media::MimeType;
    // Use mime param to determine endianness of HDR metadata. If param is
    // missing or invalid it defaults to Little Endian.
    MimeType video_mime_type(creation_parameters.video_mime());

    if (strlen(creation_parameters.video_mime()) > 0) {
      video_mime_type.RegisterStringParameter("hdrinfoendianness",
                                              "big|little");
    }
    const std::string& hdr_info_endianness =
        video_mime_type.GetParamStringValue("hdrinfoendianness",
                                            /*default=*/"little");
    bool force_big_endian_hdr_metadata = hdr_info_endianness == "big";

    scoped_ptr<VideoDecoder> video_decoder(new VideoDecoder(
        creation_parameters.video_codec(),
        GetExtendedDrmSystem(creation_parameters.drm_system()),
        creation_parameters.output_mode(),
        creation_parameters.decode_target_graphics_context_provider(),
        creation_parameters.max_video_capabilities(),
        tunnel_mode_audio_session_id, force_secure_pipeline_under_tunnel_mode,
        force_big_endian_hdr_metadata, error_message));
    if (creation_parameters.video_codec() == kSbMediaVideoCodecAv1 ||
        video_decoder->is_decoder_created()) {
      return video_decoder.Pass();
    }
    *error_message =
        "Failed to create video decoder with error: " + *error_message;
    return scoped_ptr<VideoDecoder>();
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
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(mime));
    DrmSystem* drm_system_ptr =
        static_cast<DrmSystem*>(creation_parameters.drm_system());
    jobject j_media_crypto =
        drm_system_ptr ? drm_system_ptr->GetMediaCrypto() : NULL;

    bool is_encrypted = !!j_media_crypto;
    if (env->CallStaticBooleanMethodOrAbort(
            "dev/cobalt/media/MediaCodecUtil", "hasVideoDecoderFor",
            "(Ljava/lang/String;ZIIIIZZ)Z", j_mime.Get(), is_encrypted, 0, 0, 0,
            0, false, true) == JNI_TRUE) {
      return true;
    }

    if (kForceSecurePipelineInTunnelModeWhenRequired && !is_encrypted) {
      const bool kIsEncrypted = true;
      auto support_tunnel_mode_under_secure_pipeline =
          env->CallStaticBooleanMethodOrAbort(
              "dev/cobalt/media/MediaCodecUtil", "hasVideoDecoderFor",
              "(Ljava/lang/String;ZIIIIZZ)Z", j_mime.Get(), kIsEncrypted, 0, 0,
              0, 0, false, true) == JNI_TRUE;
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

    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jobject> j_audio_output_manager(
        env->CallStarboardObjectMethodOrAbort(
            "getAudioOutputManager",
            "()Ldev/cobalt/media/AudioOutputManager;"));
    int tunnel_mode_audio_session_id = env->CallIntMethodOrAbort(
        j_audio_output_manager.Get(), "generateTunnelModeAudioSessionId",
        "(I)I", creation_parameters.audio_sample_info().number_of_channels);

    // AudioManager.generateAudioSessionId() return ERROR (-1) to indicate a
    // failure, please see the following url for more details:
    // https://developer.android.com/reference/android/media/AudioManager#generateAudioSessionId()
    SB_LOG_IF(WARNING, tunnel_mode_audio_session_id == -1)
        << "Failed to generate audio session id for tunnel mode.";

    return tunnel_mode_audio_session_id;
  }

  scoped_ptr<AudioRendererSink> TryToCreateTunnelModeAudioRendererSink(
      int tunnel_mode_audio_session_id,
      const CreationParameters& creation_parameters,
      bool enable_audio_device_callback) {
    scoped_ptr<AudioRendererSink> audio_sink(new AudioRendererSinkAndroid(
        enable_audio_device_callback, tunnel_mode_audio_session_id));
    // We need to double check if the audio sink can actually be created.
    int max_cached_frames, min_frames_per_append;
    GetAudioRendererParams(creation_parameters, &max_cached_frames,
                           &min_frames_per_append);
    AudioRendererSinkCallbackStub callback_stub;
    std::vector<uint16_t> frame_buffer(
        max_cached_frames *
        creation_parameters.audio_sample_info().number_of_channels);
    uint16_t* frame_buffers[] = {frame_buffer.data()};
    audio_sink->Start(
        0, creation_parameters.audio_sample_info().number_of_channels,
        creation_parameters.audio_sample_info().samples_per_second,
        kSbMediaAudioSampleTypeInt16Deprecated,
        kSbMediaAudioFrameStorageTypeInterleaved,
        reinterpret_cast<SbAudioSinkFrameBuffers>(frame_buffers),
        max_cached_frames, &callback_stub);
    if (audio_sink->HasStarted() && !callback_stub.error_occurred()) {
      audio_sink->Stop();
      return audio_sink.Pass();
    }
    SB_LOG(WARNING)
        << "AudioTrack does not support tunnel mode with sample rate:"
        << creation_parameters.audio_sample_info().samples_per_second
        << ", channels:"
        << creation_parameters.audio_sample_info().number_of_channels
        << ", audio format:" << creation_parameters.audio_codec()
        << ", and audio buffer frames:" << max_cached_frames;
    return scoped_ptr<AudioRendererSink>();
  }
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_PLAYER_COMPONENTS_FACTORY_H_
