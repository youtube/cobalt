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

#include "starboard/shared/starboard/player/filter/player_components.h"

#include "starboard/android/shared/audio_decoder.h"
#include "starboard/android/shared/video_decoder.h"
#include "starboard/android/shared/video_render_algorithm.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"
#include "starboard/android/shared/audio_renderer_sink_android.h"
#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_agency.h"
#include "starboard/android/shared/media_common.h"

using ::starboard::android::shared::DrmSystem;
using ::starboard::android::shared::JniEnvExt;
using ::starboard::android::shared::MediaAgency;
using ::starboard::android::shared::ScopedLocalJavaRef;
using ::starboard::android::shared::SupportedVideoCodecToMimeType;

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

class PlayerComponentsImpl : public PlayerComponents {
  bool CreateAudioComponents(const AudioParameters& audio_parameters,
                             scoped_ptr<AudioDecoder>* audio_decoder,
                             scoped_ptr<AudioRendererSink>* audio_renderer_sink,
                             std::string* error_message) override {
    SB_DCHECK(audio_decoder);
    SB_DCHECK(audio_renderer_sink);
    SB_DCHECK(error_message);

    auto decoder_creator = [](const SbMediaAudioSampleInfo& audio_sample_info,
                              SbDrmSystem drm_system) {
      using AacAudioDecoder = ::starboard::android::shared::AudioDecoder;
      using OpusAudioDecoder = ::starboard::shared::opus::OpusAudioDecoder;

      if (audio_sample_info.codec == kSbMediaAudioCodecAac) {
        scoped_ptr<AacAudioDecoder> audio_decoder_impl(new AacAudioDecoder(
            audio_sample_info.codec, audio_sample_info, drm_system));
        if (audio_decoder_impl->is_valid()) {
          return audio_decoder_impl.PassAs<AudioDecoder>();
        }
      } else if (audio_sample_info.codec == kSbMediaAudioCodecOpus) {
        scoped_ptr<OpusAudioDecoder> audio_decoder_impl(
            new OpusAudioDecoder(audio_sample_info));
        if (audio_decoder_impl->is_valid()) {
          return audio_decoder_impl.PassAs<AudioDecoder>();
        }
      } else {
        SB_NOTREACHED();
      }
      return scoped_ptr<AudioDecoder>();
    };

    audio_decoder->reset(
        new AdaptiveAudioDecoder(audio_parameters.audio_sample_info,
                                 audio_parameters.drm_system, decoder_creator));
    audio_renderer_sink->reset(new AudioRendererSinkAndroid);

    audio_renderer_sink_android_ =
        static_cast<AudioRendererSinkAndroid*>(audio_renderer_sink->get());
    audio_sample_info_ = audio_parameters.audio_sample_info;
    return true;
  }

  bool CreateVideoComponents(
      const VideoParameters& video_parameters,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink,
      std::string* error_message) override {
    using VideoDecoderImpl = ::starboard::android::shared::VideoDecoder;
    using VideoRenderAlgorithmImpl =
        ::starboard::android::shared::VideoRenderAlgorithm;

    SB_DCHECK(video_decoder);
    SB_DCHECK(video_render_algorithm);
    SB_DCHECK(video_renderer_sink);
    SB_DCHECK(error_message);

    // Note: CreateAudioComponents should be called prior to this.
    int audio_session_id = -1;
#if SB_HAS(FEATURE_TUNNEL_PLAYBACK)
    audio_session_id = CreateAudioSessionId(video_parameters);
#endif

    scoped_ptr<VideoDecoderImpl> video_decoder_impl(new VideoDecoderImpl(
        video_parameters.video_codec, video_parameters.drm_system,
        video_parameters.output_mode,
        video_parameters.decode_target_graphics_context_provider,
        audio_session_id));
    if (video_decoder_impl->is_valid()) {
      *video_renderer_sink = video_decoder_impl->GetSink();
      video_decoder->reset(video_decoder_impl.release());
    } else {
      video_decoder->reset();
      *video_renderer_sink = NULL;
      *error_message = "Failed to create video decoder.";
      return false;
    }

    video_render_algorithm->reset(new VideoRenderAlgorithmImpl);
    if (video_decoder && audio_renderer_sink_android_ != NULL) {
      VideoRenderAlgorithmImpl* video_render_algorithm_impl =
          static_cast<VideoRenderAlgorithmImpl*>(video_render_algorithm->get());
      MediaAgency::GetInstance()->UpdatePlayerClient(
          audio_session_id, audio_renderer_sink_android_,
          video_render_algorithm_impl);
    }
    return true;
  }

 private:
  AudioRendererSinkAndroid* audio_renderer_sink_android_ = NULL;
  // kSbMediaAudioCodecNone is 0
  SbMediaAudioSampleInfo audio_sample_info_ = {};

  bool IsVideoTunnelSupported(const VideoParameters& video_parameters) {
    const char* mime =
        SupportedVideoCodecToMimeType(video_parameters.video_codec);
    if (!mime) {
      return false;
    }
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(mime));
    DrmSystem* drm_system_ptr =
        static_cast<DrmSystem*>(video_parameters.drm_system);
    jobject j_media_crypto =
        drm_system_ptr ? drm_system_ptr->GetMediaCrypto() : NULL;

    return env->CallStaticBooleanMethodOrAbort(
               "dev/cobalt/media/MediaCodecUtil", "hasTunnelCapableDecoder",
               "(Ljava/lang/String;Z)Z", j_mime.Get(),
               !!j_media_crypto) == JNI_TRUE;
  }

  int CreateAudioSessionId(const VideoParameters& video_parameters) {
    int audio_session_id = -1;
    if (video_parameters.output_mode == kSbPlayerOutputModePunchOut &&
        audio_sample_info_.codec != kSbMediaAudioCodecNone &&
        video_parameters.video_codec != kSbMediaVideoCodecNone) {
      JniEnvExt* env = JniEnvExt::Get();

      if (!IsVideoTunnelSupported(video_parameters)) {
        return -1;
      }

      ScopedLocalJavaRef<jobject> j_audio_output_manager(
          env->CallStarboardObjectMethodOrAbort(
              "getAudioOutputManager",
              "()Ldev/cobalt/media/AudioOutputManager;"));

      audio_session_id = env->CallIntMethodOrAbort(
          j_audio_output_manager.Get(), "createAudioSessionId", "()I");
      if (audio_session_id == -1) {
        // https://android.googlesource.com/platform/frameworks/base/+
        // /master/media/java/android/media/AudioSystem.java#432
        // ERROR in AudioSystem.java is -1 and means failure of
        // createAudioSessionId
        return -1;
      }

      // try to check if support tunnel mode in audio pipeline
      int audio_track_buffer_frames = 1024;
      jobject j_audio_track_bridge = env->CallObjectMethodOrAbort(
          j_audio_output_manager.Get(), "createAudioTrackBridge",
          "(IIIII)Ldev/cobalt/media/AudioTrackBridge;",
          2,  // 2 is Android AudioFormat.ENCODING_PCM_16BIT
              // equivalently kSbMediaAudioSampleTypeInt16Deprecated
              // Tunnel mode only support ENCODING_PCM_16BIT
          audio_sample_info_.samples_per_second,
          audio_sample_info_.number_of_channels, audio_track_buffer_frames,
          audio_session_id);

      if (!j_audio_track_bridge) {
        SB_LOG(ERROR) << "audio do not support tuennl mode, sample rate:"
                      << audio_sample_info_.samples_per_second
                      << " channels:" << audio_sample_info_.number_of_channels
                      << " audio format:" << audio_sample_info_.codec
                      << " share buffer frames:" << audio_track_buffer_frames;
        audio_session_id = -1;
      } else {
        env->CallVoidMethodOrAbort(
            j_audio_output_manager.Get(), "destroyAudioTrackBridge",
            "(Ldev/cobalt/media/AudioTrackBridge;)V", j_audio_track_bridge);
      }
    }

    return audio_session_id;
  }
};

}  // namespace

// static
scoped_ptr<PlayerComponents> PlayerComponents::Create() {
  return make_scoped_ptr<PlayerComponents>(new PlayerComponentsImpl);
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
