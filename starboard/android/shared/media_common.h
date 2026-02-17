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

#include "base/android/jni_array.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"

namespace starboard {

struct DrmSubsampleData {
  base::android::ScopedJavaLocalRef<jintArray> clear_bytes;
  base::android::ScopedJavaLocalRef<jintArray> encrypted_bytes;
  int32_t subsample_count;
};

inline DrmSubsampleData GetDrmSubsampleData(
    JNIEnv* env,
    const SbDrmSampleInfo& drm_sample_info,
    int offset = 0) {
  int32_t subsample_count = drm_sample_info.subsample_count;
  std::unique_ptr<jint[]> clear_bytes(new jint[subsample_count]);
  std::unique_ptr<jint[]> encrypted_bytes(new jint[subsample_count]);

  for (int i = 0; i < subsample_count; ++i) {
    int32_t clear = drm_sample_info.subsample_mapping[i].clear_byte_count;
    int32_t encrypted =
        drm_sample_info.subsample_mapping[i].encrypted_byte_count;

    // Apply offset to the first subsample
    if (i == 0 && offset > 0) {
      if (clear >= offset) {
        clear -= offset;
      } else {
        // Logic to handle cases where offset > clear header (e.g. consume into
        // encrypted)
        int32_t remaining_offset = offset - clear;
        clear = 0;
        encrypted = std::max(0, encrypted - remaining_offset);
      }
    }
    clear_bytes[i] = clear;
    encrypted_bytes[i] = encrypted;
  }

  return {
      base::android::ToJavaIntArray(
          env, base::span<const jint>(clear_bytes.get(),
                                      static_cast<size_t>(subsample_count))),
      base::android::ToJavaIntArray(
          env, base::span<const jint>(encrypted_bytes.get(),
                                      static_cast<size_t>(subsample_count))),
      subsample_count};
}

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
  SB_DCHECK(is_passthrough);

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
  SB_DCHECK(sample_type);

  switch (sample_type.value()) {
    case kSbMediaAudioSampleTypeFloat32:
      return 4;  // Android AudioFormat.ENCODING_PCM_FLOAT.
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return 2;  // Android AudioFormat.ENCODING_PCM_16BIT.
  }
  SB_NOTREACHED();
  return 0u;
}

inline bool IsIdentity(const SbMediaColorMetadata& color_metadata) {
  auto is_identity = [](const SbMediaMasteringMetadata& metadata) {
    static const SbMediaMasteringMetadata kEmptyMasteringMetadata = {};
    return memcmp(&metadata, &kEmptyMasteringMetadata,
                  sizeof(SbMediaMasteringMetadata)) == 0;
  };

  return color_metadata.primaries == kSbMediaPrimaryIdBt709 &&
         color_metadata.transfer == kSbMediaTransferIdBt709 &&
         color_metadata.matrix == kSbMediaMatrixIdBt709 &&
         color_metadata.range == kSbMediaRangeIdLimited &&
         is_identity(color_metadata.mastering_metadata);
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

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
