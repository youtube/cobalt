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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_

#include <jni.h>

#include <algorithm>
#include <cstring>
#include <optional>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"

namespace starboard {

// See
// https://developer.android.com/reference/android/media/MediaFormat.html#COLOR_RANGE_FULL.
constexpr jint COLOR_RANGE_FULL = 1;
constexpr jint COLOR_RANGE_LIMITED = 2;
// Not defined in MediaFormat. Represents unspecified color ID range.
constexpr jint COLOR_RANGE_UNSPECIFIED = 0;

constexpr jint COLOR_STANDARD_BT2020 = 6;
constexpr jint COLOR_STANDARD_BT709 = 1;

constexpr jint COLOR_TRANSFER_HLG = 7;
constexpr jint COLOR_TRANSFER_SDR_VIDEO = 3;
constexpr jint COLOR_TRANSFER_ST2084 = 6;

// A special value to represent that no mapping between an SbMedia* HDR
// metadata value and Android HDR metadata value is possible.  This value
// implies that HDR playback should not be attempted.
constexpr jint COLOR_VALUE_UNKNOWN = -1;

inline bool IsWidevineL1(const char* key_system) {
  return strcmp(key_system, "com.widevine") == 0 ||
         strcmp(key_system, "com.widevine.alpha") == 0;
}

inline bool IsWidevineL3(const char* key_system) {
  return strcmp(key_system, "com.youtube.widevine.l3") == 0;
}

// Map a supported |SbMediaAudioCodec| into its corresponding mime type
// string.  Returns |nullptr| if |audio_codec| is not supported.
// On return, |is_passthrough| will be set to true if the codec should be played
// in passthrough mode, i.e. the AudioDecoder shouldn't decode the input to pcm,
// and should rely on the audio output device to decode and play the input.
inline const char* SupportedAudioCodecToMimeType(
    const SbMediaAudioCodec audio_codec,
    bool* is_passthrough) {
  SB_CHECK(is_passthrough);

  *is_passthrough = false;

  if (audio_codec == kSbMediaAudioCodecAc3 ||
      audio_codec == kSbMediaAudioCodecEac3) {
    *is_passthrough = true;
    return "audio/raw";
  }
  if (audio_codec == kSbMediaAudioCodecAac) {
    return "audio/mp4a-latm";
  }
  if (audio_codec == kSbMediaAudioCodecOpus) {
    return "audio/opus";
  }
  return nullptr;
}

// Map a supported |SbMediaVideoCodec| into its corresponding mime type
// string.  Returns |nullptr| if |video_codec| is not supported.
inline const char* SupportedVideoCodecToMimeType(
    const SbMediaVideoCodec video_codec) {
  if (video_codec == kSbMediaVideoCodecVp9) {
    return "video/x-vnd.on2.vp9";
  } else if (video_codec == kSbMediaVideoCodecH264) {
    return "video/avc";
  } else if (video_codec == kSbMediaVideoCodecH265) {
    return "video/hevc";
  } else if (video_codec == kSbMediaVideoCodecAv1) {
    return "video/av01";
  }
  return nullptr;
}

inline int GetAudioFormatSampleType(
    SbMediaAudioCodingType coding_type,
    const std::optional<SbMediaAudioSampleType>& sample_type =
        std::optional<SbMediaAudioSampleType>()) {
  if (coding_type == kSbMediaAudioCodingTypeAc3) {
    SB_DCHECK(!sample_type);
    return 5;  // Android AudioFormat.ENCODING_AC3.
  }
  if (coding_type == kSbMediaAudioCodingTypeDolbyDigitalPlus) {
    SB_DCHECK(!sample_type);
    return 6;  // Android AudioFormat.ENCODING_E_AC3.
    // TODO: Consider using 18 (AudioFormat.ENCODING_E_AC3_JOC) when supported.
  }

  SB_DCHECK_EQ(coding_type, kSbMediaAudioCodingTypePcm);
  SB_CHECK(sample_type);

  switch (sample_type.value()) {
    case kSbMediaAudioSampleTypeFloat32:
      return 4;  // Android AudioFormat.ENCODING_PCM_FLOAT.
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return 2;  // Android AudioFormat.ENCODING_PCM_16BIT.
  }
  SB_NOTREACHED();
  return 0u;
}

// GENERATED_JAVA_ENUM_PACKAGE: dev.cobalt.media
// GENERATED_JAVA_PREFIX_TO_STRIP: TUNNEL_MODE_AUDIO_SESSION_ID_
enum TunnelModeAudioSessionId {
  TUNNEL_MODE_AUDIO_SESSION_ID_NONE = -1,
};

inline bool IsIdentity(const SbMediaColorMetadata& color_metadata) {
  // Convenience HDR mastering metadata.
  const SbMediaMasteringMetadata kEmptyMasteringMetadata = {};
  return color_metadata.primaries == kSbMediaPrimaryIdBt709 &&
         color_metadata.transfer == kSbMediaTransferIdBt709 &&
         color_metadata.matrix == kSbMediaMatrixIdBt709 &&
         color_metadata.range == kSbMediaRangeIdLimited &&
         memcmp(&color_metadata.mastering_metadata, &kEmptyMasteringMetadata,
                sizeof(SbMediaMasteringMetadata)) == 0;
}

inline jint SbMediaPrimaryIdToColorStandard(SbMediaPrimaryId primary_id) {
  switch (primary_id) {
    case kSbMediaPrimaryIdBt709:
      return COLOR_STANDARD_BT709;
    case kSbMediaPrimaryIdBt2020:
      return COLOR_STANDARD_BT2020;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

inline jint SbMediaTransferIdToColorTransfer(SbMediaTransferId transfer_id) {
  switch (transfer_id) {
    case kSbMediaTransferIdBt709:
      return COLOR_TRANSFER_SDR_VIDEO;
    case kSbMediaTransferIdSmpteSt2084:
      return COLOR_TRANSFER_ST2084;
    case kSbMediaTransferIdAribStdB67:
      return COLOR_TRANSFER_HLG;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

inline jint SbMediaRangeIdToColorRange(SbMediaRangeId range_id) {
  switch (range_id) {
    case kSbMediaRangeIdLimited:
      return COLOR_RANGE_LIMITED;
    case kSbMediaRangeIdFull:
      return COLOR_RANGE_FULL;
    case kSbMediaRangeIdUnspecified:
      return COLOR_RANGE_UNSPECIFIED;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

// Returns true iff |color_metadata| describes an HDR color configuration that
// Android's MediaCodecBridge actually forwards to MediaFormat
// (KEY_COLOR_TRANSFER = ST2084/PQ or HLG, plus valid primary/range mappings).
//
// Why we must NOT use `!IsIdentity(color_metadata)` for MediaCodec lifecycle
// decisions:
//   1. Non-BT.709 SDR streams (e.g. <= 480p SD streams using BT.601 /
//      kSbMediaPrimaryIdSmpte170M, or streams with Unspecified color fields)
//      fail `IsIdentity()`, yet SbMediaPrimaryIdToColorStandard() maps them to
//      COLOR_VALUE_UNKNOWN so MediaCodecBridge::CreateVideoMediaCodec() passes
//      `j_color_info = nullptr` anyway. Treating `!IsIdentity()` as HDR tore
//      down a working MediaCodec (`j_color_info = nullptr`) only to recreate an
//      identical MediaCodec (`j_color_info = nullptr`) on startup and on every
//      480p <-> 1080p flushed seek / gapless Short transition.
//   2. Full-range BT.709 SDR streams (`range = kSbMediaRangeIdFull`) also fail
//      `IsIdentity()` and construct a non-null Java ColorInfo, which previously
//      caused MediaCodecBridge.java on HDR-capable TVs to attach fabricated
//      HDR10 static metadata (defaultMaxCll = 1000, defaultMaxFall = 200) to an
//      8-bit SDR video.
inline bool ShouldConfigureHdrColorMetadata(
    const SbMediaColorMetadata& color_metadata) {
  const bool is_hdr_transfer =
      color_metadata.transfer == kSbMediaTransferIdSmpteSt2084 ||
      color_metadata.transfer == kSbMediaTransferIdAribStdB67;
  if (!is_hdr_transfer) {
    return false;
  }
  return SbMediaPrimaryIdToColorStandard(color_metadata.primaries) !=
             COLOR_VALUE_UNKNOWN &&
         SbMediaTransferIdToColorTransfer(color_metadata.transfer) !=
             COLOR_VALUE_UNKNOWN &&
         SbMediaRangeIdToColorRange(color_metadata.range) !=
             COLOR_VALUE_UNKNOWN;
}

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
