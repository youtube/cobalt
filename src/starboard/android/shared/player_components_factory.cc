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

#include "starboard/android/shared/audio_decoder.h"
#include "starboard/android/shared/audio_renderer_sink_android.h"
#include "starboard/android/shared/audio_track_audio_sink_type.h"
#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_agency.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/video_decoder.h"
#include "starboard/android/shared/video_render_algorithm.h"
#include "starboard/android/shared/video_render_algorithm_tunneled.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

using ::starboard::android::shared::DrmSystem;
using ::starboard::android::shared::JniEnvExt;
using ::starboard::android::shared::MediaAgency;
using ::starboard::android::shared::ScopedLocalJavaRef;
using ::starboard::android::shared::SupportedVideoCodecToMimeType;

class PlayerComponentsFactory : public PlayerComponents::Factory {
  bool CreateSubComponents(
      const CreationParameters& creation_parameters,
      scoped_ptr<AudioDecoder>* audio_decoder,
      scoped_ptr<AudioRendererSink>* audio_renderer_sink,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink,
      std::string* error_message) override {
    SB_DCHECK(error_message);

    AudioRendererSinkAndroid* audio_renderer_sink_android = nullptr;
    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      SB_DCHECK(audio_decoder);
      SB_DCHECK(audio_renderer_sink);

      auto decoder_creator = [](const SbMediaAudioSampleInfo& audio_sample_info,
                                SbDrmSystem drm_system) {
        if (audio_sample_info.codec == kSbMediaAudioCodecAac) {
          scoped_ptr<android::shared::AudioDecoder> audio_decoder_impl(
              new android::shared::AudioDecoder(audio_sample_info.codec,
                                                audio_sample_info, drm_system));
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl.PassAs<AudioDecoder>();
          }
        } else if (audio_sample_info.codec == kSbMediaAudioCodecOpus) {
          scoped_ptr<opus::OpusAudioDecoder> audio_decoder_impl(
              new opus::OpusAudioDecoder(audio_sample_info));
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl.PassAs<AudioDecoder>();
          }
        } else {
          SB_NOTREACHED();
        }
        return scoped_ptr<AudioDecoder>();
      };

      audio_decoder->reset(new AdaptiveAudioDecoder(
          creation_parameters.audio_sample_info(),
          creation_parameters.drm_system(), decoder_creator));
      audio_renderer_sink_android = new AudioRendererSinkAndroid;
      audio_renderer_sink->reset(audio_renderer_sink_android);
    }

    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      int tunneling_audio_session_id = -1;
#if SB_HAS(FEATURE_TUNNEL_PLAYBACK)
      if (audio_renderer_sink_android) {
        // Only enable tunneled mode when there is an audio track.
        tunneling_audio_session_id = CreateAudioSessionId(creation_parameters);
      }
#endif
      using VideoDecoderImpl = ::starboard::android::shared::VideoDecoder;
      using VideoRenderAlgorithmImpl =
          ::starboard::android::shared::VideoRenderAlgorithm;
      using VideoRenderAlgorithmImplTunneled =
          ::starboard::android::shared::VideoRenderAlgorithmTunneled;

      SB_DCHECK(video_decoder);
      SB_DCHECK(video_render_algorithm);
      SB_DCHECK(video_renderer_sink);
      SB_DCHECK(error_message);

      using ::starboard::android::shared::VideoFramesDroppedCB;
      VideoFramesDroppedCB video_frames_dropped_cb = nullptr;
      if (tunneling_audio_session_id == -1) {
        auto video_render_algorithm_impl = new VideoRenderAlgorithmImpl;
        video_render_algorithm->reset(video_render_algorithm_impl);
      } else {
        auto video_render_algorithm_impl =
            new VideoRenderAlgorithmImplTunneled();
        video_frames_dropped_cb =
            video_render_algorithm_impl->GetVideoFramesDroppedCB();
        video_render_algorithm->reset(video_render_algorithm_impl);
      }

      scoped_ptr<VideoDecoderImpl> video_decoder_impl(new VideoDecoderImpl(
          creation_parameters.video_codec(), creation_parameters.drm_system(),
          creation_parameters.output_mode(),
          creation_parameters.decode_target_graphics_context_provider(),
          creation_parameters.max_video_capabilities(),
          tunneling_audio_session_id));
      if (video_decoder_impl->is_valid()) {
        *video_renderer_sink = video_decoder_impl->GetSink();
        video_decoder_impl->SetVideoFramesDroppedCB(video_frames_dropped_cb);
        video_decoder->reset(video_decoder_impl.release());
      } else {
        video_decoder->reset();
        *video_renderer_sink = NULL;
        *error_message = "Failed to create video decoder.";
        return false;
      }

      if (audio_renderer_sink_android) {
        MediaAgency::GetInstance()->UpdatePlayerClient(
            tunneling_audio_session_id, audio_renderer_sink_android);
      }
    }

    return true;
  }

  bool IsVideoTunneledSupported(const CreationParameters& creation_parameters) {
    const char* mime =
        SupportedVideoCodecToMimeType(creation_parameters.video_codec());
    if (!mime) {
      return false;
    }
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(mime));
    DrmSystem* drm_system_ptr =
        static_cast<DrmSystem*>(creation_parameters.drm_system());
    jobject j_media_crypto =
        drm_system_ptr ? drm_system_ptr->GetMediaCrypto() : NULL;

    return env->CallStaticBooleanMethodOrAbort(
               "dev/cobalt/media/MediaCodecUtil", "hasTunneledCapableDecoder",
               "(Ljava/lang/String;Z)Z", j_mime.Get(),
               !!j_media_crypto) == JNI_TRUE;
  }

  int CreateAudioSessionId(const CreationParameters& creation_parameters) {
    int tunneling_audio_session_id = -1;
    if (creation_parameters.output_mode() == kSbPlayerOutputModePunchOut &&
        creation_parameters.audio_codec() != kSbMediaAudioCodecNone &&
        creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      JniEnvExt* env = JniEnvExt::Get();

      if (!IsVideoTunneledSupported(creation_parameters)) {
        return -1;
      }

      ScopedLocalJavaRef<jobject> j_audio_output_manager(
          env->CallStarboardObjectMethodOrAbort(
              "getAudioOutputManager",
              "()Ldev/cobalt/media/AudioOutputManager;"));

      tunneling_audio_session_id = env->CallIntMethodOrAbort(
          j_audio_output_manager.Get(), "createAudioSessionId", "()I");
      if (tunneling_audio_session_id == -1) {
        // https://android.googlesource.com/platform/frameworks/base/+
        // /master/media/java/android/media/AudioSystem.java#432
        // ERROR in AudioSystem.java is -1 and means failure of
        // createAudioSessionId
        return -1;
      }

      // just to check if support tunneled mode in audio pipeline
      using ::starboard::android::shared::AudioTrackAudioSinkType;
      int audio_track_buffer_frames =
          creation_parameters.audio_sample_info().number_of_channels * 2 *
          AudioTrackAudioSinkType::GetMinBufferSizeInFrames(
              creation_parameters.audio_sample_info().number_of_channels,
              kSbMediaAudioSampleTypeInt16Deprecated,
              creation_parameters.audio_sample_info().samples_per_second);
      jobject j_audio_track_bridge = env->CallObjectMethodOrAbort(
          j_audio_output_manager.Get(), "createAudioTrackBridge",
          "(IIIII)Ldev/cobalt/media/AudioTrackBridge;",
          2,  // 2 is Android AudioFormat.ENCODING_PCM_16BIT
              // equivalently kSbMediaAudioSampleTypeInt16Deprecated
              // Tunneled mode only support ENCODING_PCM_16BIT
          creation_parameters.audio_sample_info().samples_per_second,
          creation_parameters.audio_sample_info().number_of_channels,
          audio_track_buffer_frames, tunneling_audio_session_id);

      if (!j_audio_track_bridge) {
        SB_LOG(ERROR)
            << "audio do not support tuennl mode, sample rate:"
            << creation_parameters.audio_sample_info().samples_per_second
            << " channels:"
            << creation_parameters.audio_sample_info().number_of_channels
            << " audio format:" << creation_parameters.audio_codec()
            << " share buffer frames:" << audio_track_buffer_frames;
        tunneling_audio_session_id = -1;
      } else {
        env->CallVoidMethodOrAbort(
            j_audio_output_manager.Get(), "destroyAudioTrackBridge",
            "(Ldev/cobalt/media/AudioTrackBridge;)V", j_audio_track_bridge);
      }
    }

    return tunneling_audio_session_id;
  }
};

}  // namespace

// static
scoped_ptr<PlayerComponents::Factory> PlayerComponents::Factory::Create() {
  return make_scoped_ptr<PlayerComponents::Factory>(
      new PlayerComponentsFactory);
}

// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  if (output_mode == kSbPlayerOutputModePunchOut) {
    return true;
  }

  if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
    return !SbDrmSystemIsValid(drm_system);
  }

  return false;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
