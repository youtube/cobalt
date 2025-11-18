// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/exoplayer/exoplayer_util.h"

#include <jni.h>

#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "cobalt/android/jni_headers/ExoPlayerCodecUtil_jni.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {
namespace {
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

// From //starboard/android/shared/video_decoder.cc.
const SbMediaMasteringMetadata kEmptyMasteringMetadata = {};

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

bool Equal(const SbMediaMasteringMetadata& lhs,
           const SbMediaMasteringMetadata& rhs) {
  return memcmp(&lhs, &rhs, sizeof(SbMediaMasteringMetadata)) == 0;
}

bool IsIdentity(const SbMediaColorMetadata& color_metadata) {
  return color_metadata.primaries == kSbMediaPrimaryIdBt709 &&
         color_metadata.transfer == kSbMediaTransferIdBt709 &&
         color_metadata.matrix == kSbMediaMatrixIdBt709 &&
         color_metadata.range == kSbMediaRangeIdLimited &&
         Equal(color_metadata.mastering_metadata, kEmptyMasteringMetadata);
}

jint SbMediaPrimaryIdToColorStandard(SbMediaPrimaryId primary_id) {
  switch (primary_id) {
    case kSbMediaPrimaryIdBt709:
      return COLOR_STANDARD_BT709;
    case kSbMediaPrimaryIdBt2020:
      return COLOR_STANDARD_BT2020;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

jint SbMediaTransferIdToColorTransfer(SbMediaTransferId transfer_id) {
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

jint SbMediaRangeIdToColorRange(SbMediaRangeId range_id) {
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

ScopedJavaLocalRef<jobject> CreateHdrColorInfo(
    const SbMediaColorMetadata& metadata) {
  if (IsIdentity(metadata)) {
    return ScopedJavaLocalRef<jobject>();
  }

  jint color_standard = SbMediaPrimaryIdToColorStandard(metadata.primaries);
  jint color_transfer = SbMediaTransferIdToColorTransfer(metadata.transfer);
  jint color_range = SbMediaRangeIdToColorRange(metadata.range);

  if (color_standard == COLOR_VALUE_UNKNOWN ||
      color_transfer == COLOR_VALUE_UNKNOWN ||
      color_range == COLOR_VALUE_UNKNOWN) {
    return ScopedJavaLocalRef<jobject>();
  }

  const auto& mastering_metadata = metadata.mastering_metadata;
  return Java_ExoPlayerCodecUtil_createHdrColorInfo(
      AttachCurrentThread(), color_range, color_standard, color_transfer,
      mastering_metadata.primary_r_chromaticity_x,
      mastering_metadata.primary_r_chromaticity_y,
      mastering_metadata.primary_g_chromaticity_x,
      mastering_metadata.primary_g_chromaticity_y,
      mastering_metadata.primary_b_chromaticity_x,
      mastering_metadata.primary_b_chromaticity_y,
      mastering_metadata.white_point_chromaticity_x,
      mastering_metadata.white_point_chromaticity_y,
      mastering_metadata.luminance_max, mastering_metadata.luminance_min,
      metadata.max_cll, metadata.max_fall);
}

bool ValidateMimeType(std::string_view mime_type_str) {
  if (!mime_type_str.empty()) {
    return MimeType(mime_type_str.data()).is_valid();
  }

  return true;
}

}  // namespace

bool ShouldEnableTunneledPlayback(const SbMediaVideoStreamInfo& stream_info) {
  if (stream_info.codec == kSbMediaVideoCodecNone ||
      !ValidateMimeType(stream_info.mime)) {
    return false;
  }

  MimeType mime_type(stream_info.mime);
  if (stream_info.mime) {
    if (!mime_type.is_valid() ||
        !mime_type.ValidateBoolParameter("tunnelmode")) {
      SB_LOG(WARNING) << "Invalid video MIME: '" << stream_info.mime
                      << "', defaulting to non-tunneled playback.";
      return false;
    }
  }

  return mime_type.GetParamBoolValue("tunnelmode", false);
}

ScopedJavaLocalRef<jobject> CreateAudioMediaSource(
    const SbMediaAudioStreamInfo& stream_info) {
  if (stream_info.codec == kSbMediaAudioCodecNone) {
    SB_LOG(ERROR)
        << "Cannot create an audio MediaSource for kSbMediaAudioCodecNone";
    return ScopedJavaLocalRef<jobject>();
  }

  if (!ValidateMimeType(stream_info.mime)) {
    SB_LOG(ERROR) << "Invalid audio MIME: '" << stream_info.mime << "'";
    return ScopedJavaLocalRef<jobject>();
  }

  int samplerate = stream_info.samples_per_second;
  int channels = stream_info.number_of_channels;

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> configuration_data;
  if (stream_info.audio_specific_config_size > 0) {
    configuration_data = ToJavaByteArray(
        env,
        reinterpret_cast<const uint8_t*>(stream_info.audio_specific_config),
        stream_info.audio_specific_config_size);
  }

  bool is_passthrough = stream_info.codec == kSbMediaAudioCodecEac3 ||
                        stream_info.codec == kSbMediaAudioCodecAc3;
  std::string j_audio_mime_str =
      SupportedAudioCodecToMimeType(stream_info.codec, &is_passthrough);
  ScopedJavaLocalRef<jstring> j_audio_mime =
      ConvertUTF8ToJavaString(env, j_audio_mime_str.c_str());

  return Java_ExoPlayerCodecUtil_createAudioMediaSource(
      env, j_audio_mime, configuration_data, samplerate, channels);
}

ScopedJavaLocalRef<jobject> CreateVideoMediaSource(
    const SbMediaVideoStreamInfo& stream_info) {
  if (stream_info.codec == kSbMediaVideoCodecNone) {
    SB_LOG(ERROR)
        << "Cannot create a video MediaSource for kSbMediaVideoCodecNone";
    return ScopedJavaLocalRef<jobject>();
  }

  if (!ValidateMimeType(stream_info.mime)) {
    SB_LOG(ERROR) << "Invalid video MIME: '" << stream_info.mime << "'";
    return ScopedJavaLocalRef<jobject>();
  }

  int width = stream_info.frame_width;
  int height = stream_info.frame_height;
  int framerate = -1;
  int bitrate = -1;

  JNIEnv* env = AttachCurrentThread();

  std::string mime_str = SupportedVideoCodecToMimeType(stream_info.codec);
  ScopedJavaLocalRef<jstring> j_mime(
      ConvertUTF8ToJavaString(env, mime_str.c_str()));

  starboard::shared::starboard::media::MimeType mime_type(stream_info.mime);
  if (mime_type.is_valid()) {
    framerate = mime_type.GetParamIntValue("framerate", -1);
    bitrate = mime_type.GetParamIntValue("bitrate", -1);
  }

  ScopedJavaLocalRef<jobject> j_hdr_color_info =
      CreateHdrColorInfo(stream_info.color_metadata);

  return Java_ExoPlayerCodecUtil_createVideoMediaSource(
      env, j_mime, width, height, framerate, bitrate, j_hdr_color_info);
}

}  // namespace starboard
