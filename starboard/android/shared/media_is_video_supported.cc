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

#include "starboard/shared/starboard/media/media_support_internal.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/media/mime_type.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;
using starboard::android::shared::SupportedVideoCodecToMimeType;
using starboard::shared::starboard::media::IsSDRVideo;
using starboard::shared::starboard::media::MimeType;

namespace {

// https://developer.android.com/reference/android/view/Display.HdrCapabilities.html#HDR_TYPE_HDR10
const jint HDR_TYPE_DOLBY_VISION = 1;
const jint HDR_TYPE_HDR10 = 2;
const jint HDR_TYPE_HLG = 3;

bool IsHDRTransferCharacteristicsSupported(SbMediaTransferId transfer_id) {
  jint hdr_type;
  if (transfer_id == kSbMediaTransferIdSmpteSt2084) {
    hdr_type = HDR_TYPE_HDR10;
  } else if (transfer_id == kSbMediaTransferIdAribStdB67) {
    hdr_type = HDR_TYPE_HLG;
  } else {
    // No other transfer functions are supported, see
    // https://source.android.com/devices/tech/display/hdr.
    return false;
  }

  return JniEnvExt::Get()->CallStarboardBooleanMethodOrAbort(
             "isHdrTypeSupported", "(I)Z", hdr_type) == JNI_TRUE;
}

}  // namespace

bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
                             const char* content_type,
                             int profile,
                             int level,
                             int bit_depth,
                             SbMediaPrimaryId primary_id,
                             SbMediaTransferId transfer_id,
                             SbMediaMatrixId matrix_id,
                             int frame_width,
                             int frame_height,
                             int64_t bitrate,
                             int fps,
                             bool decode_to_texture_required) {
  const bool must_support_hdr =
      !IsSDRVideo(bit_depth, primary_id, transfer_id, matrix_id);
  if (must_support_hdr && !IsHDRTransferCharacteristicsSupported(transfer_id)) {
    return false;
  }
  // While not necessarily true, for now we assume that all Android devices
  // can play decode-to-texture video just as well as normal video.

  // Check extended parameters for correctness and return false if any invalid
  // invalid params are found.
  MimeType mime_type(content_type);
  if (strlen(content_type) > 0) {
    // Allows for enabling tunneled playback. Disabled by default.
    // https://source.android.com/devices/tv/multimedia-tunneling
    mime_type.RegisterBoolParameter("tunnelmode");
    // Override endianness on HDR Info header. Defaults to little.
    mime_type.RegisterStringParameter("hdrinfoendianness", "big|little");
    // Forces the use of specific Android APIs (isSizeSupported() and
    // areSizeAndRateSupported()) to determine format support.
    mime_type.RegisterBoolParameter("forceimprovedsupportcheck");
    // Override the default decoder cache TTL to the specified value.
    // The cache will be disabled if the value is non-positive.
    // TODO(b/227356434): RegisterIntParameter requires API review.
    // mime_type.RegisterIntParameter("decoder_cache_ttl_ms");

    if (!mime_type.is_valid()) {
      return false;
    }
  }

  bool must_support_tunnel_mode =
      mime_type.GetParamBoolValue("tunnelmode", false);
  if (must_support_tunnel_mode && decode_to_texture_required) {
    SB_LOG(WARNING) << "Tunnel mode is rejected because output mode decode to "
                       "texture is required but not supported.";
    return false;
  }

  const char* mime = SupportedVideoCodecToMimeType(video_codec);
  if (!mime) {
    return false;
  }
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(mime));

  // We assume that if a device supports a format for clear playback, it will
  // also support it for encrypted playback. However, some devices require
  // tunneled playback to be encrypted, so we must align the tunnel mode
  // requirement with the secure playback requirement.
  const bool require_secure_playback = must_support_tunnel_mode;
  const bool force_improved_support_check =
      mime_type.GetParamBoolValue("forceimprovedsupportcheck", true);
  const int decoder_cache_ttl_ms =
      mime_type.GetParamIntValue("decoder_cache_ttl_ms", -1);

  return env->CallStaticBooleanMethodOrAbort(
             "dev/cobalt/media/MediaCodecUtil", "hasVideoDecoderFor",
             "(Ljava/lang/String;ZZZZIIIII)Z", j_mime.Get(),
             require_secure_playback, must_support_hdr,
             must_support_tunnel_mode, force_improved_support_check,
             decoder_cache_ttl_ms, frame_width, frame_height,
             static_cast<jint>(bitrate), fps) == JNI_TRUE;
}
