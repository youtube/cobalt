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

#include <algorithm>
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
#include "starboard/shared/starboard/media/resolutions.h"

namespace starboard {
namespace {

bool CanUseNdkMediaCodec(
    const MediaCodec::VideoPlatformOptions& platform_options,
    const jni_zero::JavaRef<jobject>& j_media_crypto,
    const SbMediaColorMetadata* color_metadata) {
  if (!platform_options.enable_ndk_video) {
    return false;
  }

  // We do not use NDK AMediaCodec for DRM, since it requires architectural
  // changes.
  if (platform_options.require_secured_decoder || j_media_crypto) {
    return false;
  }
  // NDK AMediaCodec does not support tunnel mode.
  if (platform_options.tunnel_mode_audio_session_id) {
    return false;
  }
  // NDK AMediaCodec does not support HDR yet.
  // TODO: b/515461431 - Make NDK impl. support HDR.
  if (color_metadata) {
    return false;
  }
  // NDK AMediaCodec requires API level >= 28.
  if (android_get_device_api_level() < 28) {
    return false;
  }

  return true;
}

Size FindSupportedMaxFrameSizeWithFps(
    const VideoCodecCapability& video_capability,
    Size candidate,
    int fps) {
  if (video_capability.AreResolutionAndRateSupported(candidate, fps)) {
    SB_LOG(INFO) << "Set max_frame_size to " << candidate << "@" << fps
                 << " per `areSizeAndRateSupported()`";
    return candidate;
  }

  SB_LOG(WARNING) << "max_frame_size " << candidate << "@" << fps
                  << " not supported per `areSizeAndRateSupported()`,"
                  << " continue searching";
  for (Size fallback : {Resolution::k8k, Resolution::k4k, Resolution::k1080p}) {
    if (candidate.height >= fallback.height &&
        video_capability.AreResolutionAndRateSupported(fallback, fps)) {
      SB_LOG(INFO) << "Set max_frame_size to " << fallback << "@" << fps
                   << " per `areSizeAndRateSupported()`";
      return fallback;
    }
  }

  SB_LOG(ERROR) << "Failed to find a compatible resolution";
  return Resolution::k1080p;
}

Size FindSupportedMaxFrameSizeWithoutFps(
    const VideoCodecCapability& video_capability,
    Size candidate) {
  // Technically we can do this check for all resolutions, but only check for
  // resolution with height more than 480p to minimize production impact. To
  // use a lower resolution is more to reduce memory footprint, and optimize
  // for lower resolution isn't as helpful anyway.
  if (candidate.height >= Resolution::k480p.height &&
      video_capability.AreResolutionAndRateSupported(candidate, /*fps=*/0)) {
    SB_LOG(INFO) << "Set max_frame_size to " << candidate
                 << " per `isSizeSupported()`";
    return candidate;
  }

  for (Size fallback : {Resolution::k4k, Resolution::k1080p}) {
    if (candidate.height >= fallback.height &&
        video_capability.AreResolutionAndRateSupported(fallback, /*fps=*/0)) {
      SB_LOG(INFO) << "Set max_frame_size to " << fallback
                   << " per `isSizeSupported()`";
      return fallback;
    }
  }

  SB_LOG(ERROR) << "Failed to find a compatible resolution";
  return Resolution::k1080p;
}

Size FindSupportedMaxFrameSize(const VideoCodecCapability& video_capability,
                               const std::optional<Size>& max_frame_size,
                               int fps) {
  Size candidate;
  if (max_frame_size && max_frame_size->width > 0 &&
      max_frame_size->height > 0) {
    candidate = *max_frame_size;
    SB_LOG(INFO) << "Evaluate max_frame_size " << candidate << " passed in";
  } else {
    candidate = video_capability.max_size();
    SB_LOG(INFO) << "max_frame_size not passed in, using supported upper bound "
                 << candidate;
  }

  return fps > 0
             ? FindSupportedMaxFrameSizeWithFps(video_capability, candidate,
                                                fps)
             : FindSupportedMaxFrameSizeWithoutFps(video_capability, candidate);
}

Size ClampMaxFrameSize(Size size, int sdk_int) {
  // Since we haven't passed the properties of the stream we're playing down to
  // this level, from our perspective, we could potentially adapt up to 8k at
  // any point. We thus request 8k buffers up front, unless the decoder claims
  // to not be able to do 8k, in which case we're ok, since we would've
  // rejected a 8k stream when canPlayType was called, and then use those
  // decoder values instead. We only support 8k for API level 29 and above.
  const Size upper_limit = sdk_int > 28 ? Resolution::k8k : Resolution::k4k;
  return Size{std::min(size.width, upper_limit.width),
              std::min(size.height, upper_limit.height)};
}

}  // namespace

Size GetSupportedMaxFrameSize(const VideoCodecCapability* video_capability,
                              const std::optional<Size>& max_frame_size,
                              int fps,
                              int sdk_int) {
  if (!video_capability) {
    SB_LOG(WARNING) << "VideoCodecCapability is null, falling back to 1080p";
    return ClampMaxFrameSize(Resolution::k1080p, sdk_int);
  }
  Size size = FindSupportedMaxFrameSize(*video_capability, max_frame_size, fps);
  return ClampMaxFrameSize(size, sdk_int);
}

std::unique_ptr<MediaCodec> DefaultMediaCodecFactory::CreateAudioMediaCodec(
    const AudioStreamInfo& audio_stream_info,
    MediaCodec::Handler* handler,
    const jni_zero::JavaRef<jobject>& j_media_crypto) {
  return MediaCodecBridge::CreateAudioMediaCodec(audio_stream_info, handler,
                                                 j_media_crypto);
}

NonNullResult<std::unique_ptr<MediaCodec>>
DefaultMediaCodecFactory::CreateVideoMediaCodec(
    SbMediaVideoCodec video_codec,
    const Size& frame_size_hint,
    int fps,
    const std::optional<Size>& max_frame_size,
    MediaCodec::Handler* handler,
    const jni_zero::JavaRef<jobject>& j_surface,
    const jni_zero::JavaRef<jobject>& j_media_crypto,
    const SbMediaColorMetadata* color_metadata,
    const MediaCodec::VideoPlatformOptions& platform_options) {
  if (max_frame_size) {
    SB_CHECK_GT(max_frame_size->width, 0);
    SB_CHECK_GT(max_frame_size->height, 0);
  }

  const char* mime = SupportedVideoCodecToMimeType(video_codec);
  if (!mime) {
    return Failure(std::string("Unsupported mime for codec: ") +
                   GetMediaVideoCodecName(video_codec));
  }

  const bool must_support_secure = platform_options.require_secured_decoder;
  const bool must_support_hdr = color_metadata;
  const bool must_support_tunnel_mode =
      platform_options.tunnel_mode_audio_session_id.has_value();

  std::string decoder_name =
      MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
          mime, must_support_secure, must_support_hdr,
          platform_options.require_software_codec, must_support_tunnel_mode);
  if (decoder_name.empty() && color_metadata) {
    decoder_name = MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
        mime, must_support_secure, /*must_support_hdr=*/false,
        platform_options.require_software_codec, must_support_tunnel_mode);
  }
  if (decoder_name.empty() && platform_options.require_software_codec) {
    decoder_name = MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
        mime, must_support_secure, /*must_support_hdr=*/false,
        /*require_software_codec=*/false, must_support_tunnel_mode);
  }

  if (decoder_name.empty()) {
    return Failure(
        FormatString("Failed to find decoder: mime=%s, mustSupportSecure=%s",
                     mime, ToString(!!j_media_crypto).data()));
  }

  const VideoCodecCapability* video_capability =
      MediaCapabilitiesCache::GetInstance()->FindVideoCodecCapability(
          mime, decoder_name);
  const Size supported_max_frame_size = GetSupportedMaxFrameSize(
      video_capability, max_frame_size, fps, android_get_device_api_level());

  if (CanUseNdkMediaCodec(platform_options, j_media_crypto, color_metadata)) {
    auto ndk_bridge = NdkMediaCodec::Create(
        video_codec, decoder_name, frame_size_hint, fps,
        supported_max_frame_size, handler, j_surface, j_media_crypto,
        color_metadata, platform_options.enable_frame_renderer_listener,
        platform_options.require_secured_decoder,
        platform_options.require_software_codec,
        platform_options.max_input_size);
    if (ndk_bridge) {
      return ndk_bridge;
    }
    SB_LOG(WARNING)
        << "Failed to create NdkMediaCodec. Falling back to Java MediaCodec.";
  }

  auto jni_result = MediaCodecBridge::CreateVideoMediaCodec(
      video_codec, decoder_name, mime, frame_size_hint, fps,
      supported_max_frame_size, handler, j_surface, j_media_crypto,
      color_metadata, platform_options);
  if (jni_result) {
    return std::move(jni_result.value());
  }
  return Failure(jni_result.error());
}

// static
std::unique_ptr<MediaCodec> MediaCodec::CreateAudioMediaCodec(
    const AudioStreamInfo& audio_stream_info,
    Handler* handler,
    const jni_zero::JavaRef<jobject>& j_media_crypto) {
  DefaultMediaCodecFactory factory;
  return factory.CreateAudioMediaCodec(audio_stream_info, handler,
                                       j_media_crypto);
}

// static
NonNullResult<std::unique_ptr<MediaCodec>> MediaCodec::CreateVideoMediaCodec(
    SbMediaVideoCodec video_codec,
    const Size& frame_size_hint,
    int fps,
    const std::optional<Size>& max_frame_size,
    Handler* handler,
    const jni_zero::JavaRef<jobject>& j_surface,
    const jni_zero::JavaRef<jobject>& j_media_crypto,
    const SbMediaColorMetadata* color_metadata,
    const MediaCodec::VideoPlatformOptions& platform_options) {
  DefaultMediaCodecFactory factory;
  return factory.CreateVideoMediaCodec(
      video_codec, frame_size_hint, fps, max_frame_size, handler, j_surface,
      j_media_crypto, color_metadata, platform_options);
}

// static
bool MediaCodec::IsFrameRenderedCallbackEnabled() {
  return android_get_device_api_level() >= 34;
}

FrameSize::FrameSize() : FrameSize(Size(), /*has_crop_values=*/false) {}

FrameSize::FrameSize(Size display_size, bool has_crop_values)
    : display_size(display_size), has_crop_values(has_crop_values) {
  SB_CHECK_GE(display_size.width, 0);
  SB_CHECK_GE(display_size.height, 0);
}

std::ostream& operator<<(std::ostream& os, const FrameSize& size) {
  return os << "{display_size=" << size.display_size
            << ", has_crop_values=" << ToString(size.has_crop_values) << "}";
}

std::ostream& operator<<(std::ostream& os,
                         const MediaCodec::VideoPlatformOptions& options) {
  return os << "{max_input_size=" << options.max_input_size
            << ", skip_video_frames_over_60_fps="
            << ToString(options.skip_video_frames_over_60_fps)
            << ", enable_frame_renderer_listener="
            << ToString(options.enable_frame_renderer_listener)
            << ", require_secured_decoder="
            << ToString(options.require_secured_decoder)
            << ", require_software_codec="
            << ToString(options.require_software_codec)
            << ", tunnel_mode_audio_session_id="
            << ToString(options.tunnel_mode_audio_session_id) << "}";
}

}  // namespace starboard
