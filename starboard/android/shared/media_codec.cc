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

#include "starboard/android/shared/media_codec.h"

#include <android/api-level.h>

#include <memory>
#include <optional>
#include <string>

#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/ndk_media_codec.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/features.h"

namespace starboard {
namespace {

bool CanUseNdkMediaCodec(std::optional<int> tunnel_mode_audio_session_id,
                         bool require_secured_decoder,
                         jobject j_media_crypto) {
  if (!features::FeatureList::IsEnabled(features::kEnableNdkVideo)) {
    return false;
  }

  // 1. Critical Usability Checks
  if (require_secured_decoder || j_media_crypto) {
    SB_LOG(INFO) << "[MediaCodec] Secure decoding requested. NDK AMediaCodec "
                    "does not support DRM. Forcing NDK AMediaCodec OFF.";
    return false;
  }

  if (tunnel_mode_audio_session_id.value_or(-1) != -1) {
    SB_LOG(INFO) << "[MediaCodec] Tunnel mode requested. NDK AMediaCodec "
                    "does not support tunnel mode. Using Java MediaCodec.";
    return false;
  }

  if (android_get_device_api_level() < 28) {
    SB_LOG(INFO) << "[MediaCodec] NDK AMediaCodec requires API level >= 28. "
                    "Using Java MediaCodec.";
    return false;
  }

  SB_LOG(INFO)
      << "[MediaCodec] NDK AMediaCodec is usable. Selecting NDK backend.";
  return true;
}
}  // namespace

// static
std::unique_ptr<MediaCodec> MediaCodec::CreateAudioMediaCodec(
    const AudioStreamInfo& audio_stream_info,
    Handler* handler,
    jobject j_media_crypto) {
  return MediaCodecBridge::CreateAudioMediaCodec(audio_stream_info, handler,
                                                 j_media_crypto);
}

// static
NonNullResult<std::unique_ptr<MediaCodec>> MediaCodec::CreateVideoMediaCodec(
    SbMediaVideoCodec video_codec,
    const Size& frame_size_hint,
    int fps,
    const std::optional<Size>& max_frame_size,
    Handler* handler,
    jobject j_surface,
    jobject j_media_crypto,
    const SbMediaColorMetadata* color_metadata,
    bool enable_frame_renderer_listener,
    bool require_secured_decoder,
    bool require_software_codec,
    std::optional<int> tunnel_mode_audio_session_id,
    bool force_big_endian_hdr_metadata,
    int max_video_input_size,
    bool enable_output_checker,
    bool skip_video_frames_over_60_fps) {
  if (max_frame_size) {
    SB_CHECK_GT(max_frame_size->width, 0);
    SB_CHECK_GT(max_frame_size->height, 0);
  }

  const char* mime = SupportedVideoCodecToMimeType(video_codec);
  if (!mime) {
    return Failure(std::string("Unsupported mime for codec: ") +
                   GetMediaVideoCodecName(video_codec));
  }

  const bool must_support_secure = require_secured_decoder;
  const bool must_support_hdr = color_metadata;
  const bool must_support_tunnel_mode =
      tunnel_mode_audio_session_id.has_value();

  std::string decoder_name =
      MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
          mime, must_support_secure, must_support_hdr, require_software_codec,
          must_support_tunnel_mode,
          /*frame_width=*/0,
          /*frame_height=*/0,
          /*bitrate=*/0,
          /*fps=*/0);
  if (decoder_name.empty() && color_metadata) {
    decoder_name = MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
        mime, must_support_secure, /*must_support_hdr=*/false,
        require_software_codec, must_support_tunnel_mode,
        /*frame_width=*/0,
        /*frame_height=*/0,
        /*bitrate=*/0,
        /*fps=*/0);
  }
  if (decoder_name.empty() && require_software_codec) {
    decoder_name = MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
        mime, must_support_secure, /*must_support_hdr=*/false,
        /*require_software_codec=*/false, must_support_tunnel_mode,
        /*frame_width=*/0,
        /*frame_height=*/0,
        /*bitrate=*/0,
        /*fps=*/0);
  }

  if (decoder_name.empty()) {
    return Failure(
        FormatString("Failed to find decoder: mime=%s, mustSupportSecure=%s",
                     static_cast<const char*>(mime),
                     starboard::ToString(!!j_media_crypto).data()));
  }

  if (CanUseNdkMediaCodec(tunnel_mode_audio_session_id, require_secured_decoder,
                          j_media_crypto)) {
    auto ndk_bridge = NdkMediaCodec::Create(
        video_codec, decoder_name, frame_size_hint, fps, max_frame_size,
        handler, j_surface, j_media_crypto, color_metadata,
        enable_frame_renderer_listener, require_secured_decoder,
        require_software_codec, max_video_input_size, enable_output_checker);
    if (ndk_bridge) {
      return ndk_bridge;
    }
    SB_LOG(WARNING)
        << "Failed to create NdkMediaCodec. Falling back to Java MediaCodec.";
  }

  SB_LOG(INFO) << "[MediaCodec] Selected Backend: Java MediaCodec (JNI).";
  auto jni_result = MediaCodecBridge::CreateVideoMediaCodec(
      video_codec, decoder_name, mime, frame_size_hint, fps, max_frame_size,
      handler, j_surface, j_media_crypto, color_metadata,
      enable_frame_renderer_listener, require_secured_decoder,
      require_software_codec, tunnel_mode_audio_session_id,
      force_big_endian_hdr_metadata, max_video_input_size,
      enable_output_checker, skip_video_frames_over_60_fps);
  if (jni_result) {
    return std::move(jni_result.value());
  }
  return Failure(jni_result.error());
}

bool MediaCodec::IsFrameRenderedCallbackEnabled() {
  return android_get_device_api_level() >= 34;
}

}  // namespace starboard
