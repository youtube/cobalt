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

#include "starboard/shared/starboard/media/media_util.h"

#include <cctype>

#include "starboard/character.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/codec_util.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

const int64_t kDefaultBitRate = 0;
const int64_t kDefaultAudioChannels = 2;

bool IsSupportedAudioCodec(const MimeType& mime_type,
                           const std::string& codec,
                           const char* key_system) {
  SbMediaAudioCodec audio_codec = GetAudioCodecFromString(codec.c_str());
  if (audio_codec == kSbMediaAudioCodecNone) {
    return false;
  }

  // TODO: allow platform-specific rejection of a combination of codec &
  // number of channels, by passing channels to SbMediaAudioIsSupported and /
  // or SbMediaIsSupported.

  if (SbStringGetLength(key_system) != 0) {
    if (!SbMediaIsSupported(kSbMediaVideoCodecNone, audio_codec, key_system)) {
      return false;
    }
  }

  int channels = mime_type.GetParamIntValue("channels", kDefaultAudioChannels);
  if (!IsAudioOutputSupported(kSbMediaAudioCodingTypePcm, channels)) {
    return false;
  }

  int bitrate = mime_type.GetParamIntValue("bitrate", kDefaultBitRate);

  if (!SbMediaIsAudioSupported(audio_codec, bitrate)) {
    return false;
  }

  switch (audio_codec) {
    case kSbMediaAudioCodecNone:
      SB_NOTREACHED();
      return false;
    case kSbMediaAudioCodecAac:
      return mime_type.subtype() == "mp4";
#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION || SB_HAS(AC3_AUDIO)
    case kSbMediaAudioCodecAc3:
      if (!kSbHasAc3Audio) {
        SB_NOTREACHED() << "AC3 audio is not enabled on this platform. To "
                        << "enable it, set kSbHasAc3Audio to |true|.";
        return false;
      }
      return mime_type.subtype() == "mp4";
    case kSbMediaAudioCodecEac3:
      if (!kSbHasAc3Audio) {
        SB_NOTREACHED() << "AC3 audio is not enabled on this platform. To "
                        << "enable it, set kSbHasAc3Audio to |true|.";
        return false;
      }
      return mime_type.subtype() == "mp4";
#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION ||
        // SB_HAS(AC3_AUDIO)
    case kSbMediaAudioCodecOpus:
    case kSbMediaAudioCodecVorbis:
      return mime_type.subtype() == "webm";
  }

  SB_NOTREACHED();
  return false;
}

bool IsSupportedVideoCodec(const MimeType& mime_type,
                           const std::string& codec,
                           const char* key_system,
                           bool decode_to_texture_required) {
#if SB_API_VERSION < 10
  SB_UNREFERENCED_PARAMETER(decode_to_texture_required);
#endif  // SB_API_VERSION < 10

  SbMediaVideoCodec video_codec;
  int profile = -1;
  int level = -1;
  int bit_depth = 8;
  SbMediaPrimaryId primary_id = kSbMediaPrimaryIdUnspecified;
  SbMediaTransferId transfer_id = kSbMediaTransferIdUnspecified;
  SbMediaMatrixId matrix_id = kSbMediaMatrixIdUnspecified;

  if (!ParseVideoCodec(codec.c_str(), &video_codec, &profile, &level,
                       &bit_depth, &primary_id, &transfer_id, &matrix_id)) {
    return false;
  }
  SB_DCHECK(video_codec != kSbMediaVideoCodecNone);

  if (SbStringGetLength(key_system) != 0) {
    if (!SbMediaIsSupported(video_codec, kSbMediaAudioCodecNone, key_system)) {
      return false;
    }
  }

  std::string eotf = mime_type.GetParamStringValue("eotf", "");
  if (!eotf.empty()) {
    SbMediaTransferId transfer_id_from_eotf = GetTransferIdFromString(eotf);
    // If the eotf is not known, reject immediately - without checking with
    // the platform.
    if (transfer_id_from_eotf == kSbMediaTransferIdUnknown) {
      return false;
    }
    if (transfer_id != kSbMediaTransferIdUnspecified &&
        transfer_id != transfer_id_from_eotf) {
      SB_LOG_IF(WARNING, transfer_id != kSbMediaTransferIdUnspecified)
          << "transfer_id " << transfer_id << " set by the codec string \""
          << codec << "\" will be overwritten by the eotf attribute " << eotf;
    }
    transfer_id = transfer_id_from_eotf;
#if !SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
    if (!SbMediaIsTransferCharacteristicsSupported(transfer_id)) {
      return false;
    }
#endif  // !SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
  }

  std::string cryptoblockformat =
      mime_type.GetParamStringValue("cryptoblockformat", "");
  if (!cryptoblockformat.empty()) {
    if (mime_type.subtype() != "webm" || cryptoblockformat != "subsample") {
      return false;
    }
  }

  int width = mime_type.GetParamIntValue("width", 0);
  int height = mime_type.GetParamIntValue("height", 0);
  int fps = mime_type.GetParamIntValue("framerate", 0);

  int bitrate = mime_type.GetParamIntValue("bitrate", kDefaultBitRate);

#if SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
  if (!SbMediaIsVideoSupported(video_codec, profile, level, bit_depth,
                               primary_id, transfer_id, matrix_id, width,
                               height, bitrate, fps
#if SB_API_VERSION >= 10
                               ,
                               decode_to_texture_required
#endif  // SB_API_VERSION >= 10
                               )) {
    return false;
  }
#else  //  SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
  if (!SbMediaIsVideoSupported(video_codec, width, height, bitrate, fps
#if SB_API_VERSION >= 10
                               ,
                               decode_to_texture_required
#endif  // SB_API_VERSION >= 10
                               )) {
    return false;
  }
#endif  // SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)

  switch (video_codec) {
    case kSbMediaVideoCodecNone:
      SB_NOTREACHED();
      return false;
    case kSbMediaVideoCodecH264:
    case kSbMediaVideoCodecH265:
      return mime_type.subtype() == "mp4";
    case kSbMediaVideoCodecMpeg2:
    case kSbMediaVideoCodecTheora:
      return false;  // No associated container in YT.
    case kSbMediaVideoCodecVc1:
#if SB_API_VERSION < 11
    case kSbMediaVideoCodecVp10:
#else   // SB_API_VERSION < 11
    case kSbMediaVideoCodecAv1:
#endif  // SB_API_VERSION < 11
      return mime_type.subtype() == "mp4";
    case kSbMediaVideoCodecVp8:
    case kSbMediaVideoCodecVp9:
      return mime_type.subtype() == "webm";
  }

  SB_NOTREACHED();
  return false;
}

}  // namespace

bool IsAudioOutputSupported(SbMediaAudioCodingType coding_type, int channels) {
  int count = SbMediaGetAudioOutputCount();

  for (int output_index = 0; output_index < count; ++output_index) {
    SbMediaAudioConfiguration configuration;
    if (!SbMediaGetAudioConfiguration(output_index, &configuration)) {
      continue;
    }

    if (configuration.coding_type == coding_type &&
        configuration.number_of_channels >= channels) {
      return true;
    }
  }

  return false;
}

bool IsSDRVideo(int bit_depth,
                SbMediaPrimaryId primary_id,
                SbMediaTransferId transfer_id,
                SbMediaMatrixId matrix_id) {
  if (bit_depth != 8) {
    return false;
  }

  if (primary_id != kSbMediaPrimaryIdBt709 &&
      primary_id != kSbMediaPrimaryIdUnspecified &&
      primary_id != kSbMediaPrimaryIdSmpte170M) {
    return false;
  }

  if (transfer_id != kSbMediaTransferIdBt709 &&
      transfer_id != kSbMediaTransferIdUnspecified &&
      transfer_id != kSbMediaTransferIdSmpte170M) {
    return false;
  }

  if (matrix_id != kSbMediaMatrixIdBt709 &&
      matrix_id != kSbMediaMatrixIdUnspecified &&
      matrix_id != kSbMediaMatrixIdSmpte170M) {
    return false;
  }

  return true;
}

SbMediaTransferId GetTransferIdFromString(const std::string& transfer_id) {
  if (transfer_id == "bt709") {
    return kSbMediaTransferIdBt709;
  } else if (transfer_id == "smpte2084") {
    return kSbMediaTransferIdSmpteSt2084;
  } else if (transfer_id == "arib-std-b67") {
    return kSbMediaTransferIdAribStdB67;
  }
  return kSbMediaTransferIdUnknown;
}

int GetBytesPerSample(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return 2;
    case kSbMediaAudioSampleTypeFloat32:
      return 4;
  }

  SB_NOTREACHED();
  return 4;
}

SbMediaSupportType CanPlayMimeAndKeySystem(const MimeType& mime_type,
                                           const char* key_system) {
  SB_DCHECK(mime_type.is_valid());

  if (mime_type.type() != "audio" && mime_type.type() != "video") {
    return kSbMediaSupportTypeNotSupported;
  }

  auto codecs = mime_type.GetCodecs();

  // Pre-filter for |key_system|.
  if (SbStringGetLength(key_system) != 0) {
    if (!SbMediaIsSupported(kSbMediaVideoCodecNone, kSbMediaAudioCodecNone,
                            key_system)) {
      return kSbMediaSupportTypeNotSupported;
    }
  }

  bool decode_to_texture_required = false;
#if SB_API_VERSION >= 10
  std::string decode_to_texture_value =
      mime_type.GetParamStringValue("decode-to-texture", "false");
  if (decode_to_texture_value == "true") {
    decode_to_texture_required = true;
  } else if (decode_to_texture_value != "false") {
    // If an invalid value (e.g. not "true" or "false") is passed in for
    // decode-to-texture, trivially reject.
    return kSbMediaSupportTypeNotSupported;
  }
#endif  // SB_API_VERSION >= 10

  if (codecs.size() == 0) {
    // This is a progressive query.  We only support "video/mp4" in this case.
    if (mime_type.type() == "video" && mime_type.subtype() == "mp4") {
      return kSbMediaSupportTypeMaybe;
    }
    return kSbMediaSupportTypeNotSupported;
  }

  if (codecs.size() > 2) {
    return kSbMediaSupportTypeNotSupported;
  }

  bool has_audio_codec = false;
  bool has_video_codec = false;
  for (const auto& codec : codecs) {
    if (IsSupportedAudioCodec(mime_type, codec, key_system)) {
      if (has_audio_codec) {
        // We don't support two audio codecs in one stream.
        return kSbMediaSupportTypeNotSupported;
      }
      has_audio_codec = true;
      continue;
    }
    if (IsSupportedVideoCodec(mime_type, codec, key_system,
                              decode_to_texture_required)) {
      if (mime_type.type() != "video") {
        // Video can only be contained in "video/*", while audio can be
        // contained in both "audio/*" and "video/*".
        return kSbMediaSupportTypeNotSupported;
      }
      if (has_video_codec) {
        // We don't support two video codecs in one stream.
        return kSbMediaSupportTypeNotSupported;
      }
      has_video_codec = true;
      continue;
    }
    return kSbMediaSupportTypeNotSupported;
  }

  if (has_audio_codec || has_video_codec) {
    return kSbMediaSupportTypeProbably;
  }
  return kSbMediaSupportTypeNotSupported;
}

const char* GetCodecName(SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecNone:
      return "none";
    case kSbMediaAudioCodecAac:
      return "aac";
#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION || SB_HAS(AC3_AUDIO)
    case kSbMediaAudioCodecAc3:
      if (!kSbHasAc3Audio) {
        SB_NOTREACHED() << "AC3 audio is not enabled on this platform. To "
                        << "enable it, set kSbHasAc3Audio to |true|.";
        return "invalid";
      }
      return "ac3";
    case kSbMediaAudioCodecEac3:
      if (!kSbHasAc3Audio) {
        SB_NOTREACHED() << "AC3 audio is not enabled on this platform. To "
                        << "enable it, set kSbHasAc3Audio to |true|.";
        return "invalid";
      }
      return "ec3";
#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION ||
        // SB_HAS(AC3_AUDIO)
    case kSbMediaAudioCodecOpus:
      return "opus";
    case kSbMediaAudioCodecVorbis:
      return "vorbis";
  }
  SB_NOTREACHED();
  return "invalid";
}

const char* GetCodecName(SbMediaVideoCodec codec) {
  switch (codec) {
    case kSbMediaVideoCodecNone:
      return "none";
    case kSbMediaVideoCodecH264:
      return "avc";
    case kSbMediaVideoCodecH265:
      return "hevc";
    case kSbMediaVideoCodecMpeg2:
      return "mpeg2";
    case kSbMediaVideoCodecTheora:
      return "theora";
    case kSbMediaVideoCodecVc1:
      return "vc1";
#if SB_API_VERSION < 11
    case kSbMediaVideoCodecVp10:
      return "vp10";
#else   // SB_API_VERSION < 11
    case kSbMediaVideoCodecAv1:
      return "av1";
#endif  // SB_API_VERSION < 11
    case kSbMediaVideoCodecVp8:
      return "vp8";
    case kSbMediaVideoCodecVp9:
      return "vp9";
  }
  SB_NOTREACHED();
  return "invalid";
}

const char* GetPrimaryIdName(SbMediaPrimaryId primary_id) {
  switch (primary_id) {
    case kSbMediaPrimaryIdReserved0:
      return "Reserved0";
    case kSbMediaPrimaryIdBt709:
      return "Bt709";
    case kSbMediaPrimaryIdUnspecified:
      return "Unspecified";
    case kSbMediaPrimaryIdReserved:
      return "Reserved";
    case kSbMediaPrimaryIdBt470M:
      return "Bt470M";
    case kSbMediaPrimaryIdBt470Bg:
      return "Bt470Bg";
    case kSbMediaPrimaryIdSmpte170M:
      return "Smpte170M";
    case kSbMediaPrimaryIdSmpte240M:
      return "Smpte240M";
    case kSbMediaPrimaryIdFilm:
      return "Film";
    case kSbMediaPrimaryIdBt2020:
      return "Bt2020";
    case kSbMediaPrimaryIdSmpteSt4281:
      return "SmpteSt4281";
    case kSbMediaPrimaryIdSmpteSt4312:
      return "SmpteSt4312";
    case kSbMediaPrimaryIdSmpteSt4321:
      return "SmpteSt4321";
    case kSbMediaPrimaryIdUnknown:
      return "Unknown";
    case kSbMediaPrimaryIdXyzD50:
      return "XyzD50";
    case kSbMediaPrimaryIdCustom:
      return "Custom";
  }
  SB_NOTREACHED();
  return "Invalid";
}

const char* GetTransferIdName(SbMediaTransferId transfer_id) {
  switch (transfer_id) {
    case kSbMediaTransferIdReserved0:
      return "Reserved0";
    case kSbMediaTransferIdBt709:
      return "Bt709";
    case kSbMediaTransferIdUnspecified:
      return "Unspecified";
    case kSbMediaTransferIdReserved:
      return "Reserved";
    case kSbMediaTransferIdGamma22:
      return "Gamma22";
    case kSbMediaTransferIdGamma28:
      return "Gamma28";
    case kSbMediaTransferIdSmpte170M:
      return "Smpte170M";
    case kSbMediaTransferIdSmpte240M:
      return "Smpte240M";
    case kSbMediaTransferIdLinear:
      return "Linear";
    case kSbMediaTransferIdLog:
      return "Log";
    case kSbMediaTransferIdLogSqrt:
      return "LogSqrt";
    case kSbMediaTransferIdIec6196624:
      return "Iec6196624";
    case kSbMediaTransferIdBt1361Ecg:
      return "Bt1361Ecg";
    case kSbMediaTransferIdIec6196621:
      return "Iec6196621";
    case kSbMediaTransferId10BitBt2020:
      return "10BitBt2020";
    case kSbMediaTransferId12BitBt2020:
      return "12BitBt2020";
    case kSbMediaTransferIdSmpteSt2084:
      return "SmpteSt2084";
    case kSbMediaTransferIdSmpteSt4281:
      return "SmpteSt4281";
    case kSbMediaTransferIdAribStdB67:
      return "AribStdB67/HLG";
    case kSbMediaTransferIdUnknown:
      return "Unknown";
    case kSbMediaTransferIdGamma24:
      return "Gamma24";
    case kSbMediaTransferIdSmpteSt2084NonHdr:
      return "SmpteSt2084NonHdr";
    case kSbMediaTransferIdCustom:
      return "Custom";
  }
  SB_NOTREACHED();
  return "Invalid";
}

const char* GetMatrixIdName(SbMediaMatrixId matrix_id) {
  switch (matrix_id) {
    case kSbMediaMatrixIdRgb:
      return "Rgb";
    case kSbMediaMatrixIdBt709:
      return "Bt709";
    case kSbMediaMatrixIdUnspecified:
      return "Unspecified";
    case kSbMediaMatrixIdReserved:
      return "Reserved";
    case kSbMediaMatrixIdFcc:
      return "Fcc";
    case kSbMediaMatrixIdBt470Bg:
      return "Bt470Bg";
    case kSbMediaMatrixIdSmpte170M:
      return "Smpte170M";
    case kSbMediaMatrixIdSmpte240M:
      return "Smpte240M";
    case kSbMediaMatrixIdYCgCo:
      return "YCgCo";
    case kSbMediaMatrixIdBt2020NonconstantLuminance:
      return "Bt2020NonconstantLuminance";
    case kSbMediaMatrixIdBt2020ConstantLuminance:
      return "Bt2020ConstantLuminance";
    case kSbMediaMatrixIdYDzDx:
      return "YDzDx";
    case kSbMediaMatrixIdUnknown:
      return "Unknown";
  }
  SB_NOTREACHED();
  return "Invalid";
}

const char* GetRangeIdName(SbMediaRangeId range_id) {
  switch (range_id) {
    case kSbMediaRangeIdUnspecified:
      return "Unspecified";
    case kSbMediaRangeIdLimited:
      return "Limited";
    case kSbMediaRangeIdFull:
      return "Full";
    case kSbMediaRangeIdDerived:
      return "Derived";
  }
  SB_NOTREACHED();
  return "Invalid";
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

bool operator==(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return SbMemoryCompare(&metadata_1, &metadata_2,
                         sizeof(SbMediaColorMetadata)) == 0;
}

bool operator==(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
#if SB_API_VERSION >= 11
  if (sample_info_1.codec != sample_info_2.codec) {
    return false;
  }
#endif  // SB_API_VERSION >= 11
  if (sample_info_1.is_key_frame != sample_info_2.is_key_frame) {
    return false;
  }
  if (sample_info_1.frame_width != sample_info_2.frame_width) {
    return false;
  }
  if (sample_info_1.frame_height != sample_info_2.frame_height) {
    return false;
  }
#if SB_API_VERSION >= 11
  return sample_info_1.color_metadata == sample_info_2.color_metadata;
#else   // SB_API_VERSION >= 11
  return *sample_info_1.color_metadata == *sample_info_2.color_metadata;
#endif  // SB_API_VERSION >= 11
}

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return !(metadata_1 == metadata_2);
}

bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
  return !(sample_info_1 == sample_info_2);
}

std::ostream& operator<<(std::ostream& os,
                         const SbMediaMasteringMetadata& metadata) {
  os << "r(" << metadata.primary_r_chromaticity_x << ", "
     << metadata.primary_r_chromaticity_y << "), g("
     << metadata.primary_g_chromaticity_x << ", "
     << metadata.primary_g_chromaticity_y << "), b("
     << metadata.primary_b_chromaticity_x << ", "
     << metadata.primary_b_chromaticity_y << "), white("
     << metadata.white_point_chromaticity_x << ", "
     << metadata.white_point_chromaticity_y << "), luminance("
     << metadata.luminance_min << " to " << metadata.luminance_max << ")";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SbMediaColorMetadata& metadata) {
  using starboard::shared::starboard::media::GetPrimaryIdName;
  using starboard::shared::starboard::media::GetTransferIdName;
  using starboard::shared::starboard::media::GetMatrixIdName;
  using starboard::shared::starboard::media::GetRangeIdName;
  os << metadata.bits_per_channel
     << " bits, mastering metadata: " << metadata.mastering_metadata
     << ", primary: " << GetPrimaryIdName(metadata.primaries)
     << ", transfer: " << GetTransferIdName(metadata.transfer)
     << ", matrix: " << GetMatrixIdName(metadata.matrix)
     << ", range: " << GetRangeIdName(metadata.range);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SbMediaVideoSampleInfo& sample_info) {
  using starboard::shared::starboard::media::GetCodecName;
#if SB_API_VERSION >= 11
  os << GetCodecName(sample_info.codec) << ", ";
#endif  // SB_API_VERSION >= 11
  if (sample_info.is_key_frame) {
    os << "key frame, ";
  }
  os << sample_info.frame_width << 'x' << sample_info.frame_height << ' ';
#if SB_API_VERSION >= 11
  os << '(' << sample_info.color_metadata << ')';
#else   // SB_API_VERSION >= 11
  os << '(' << *sample_info.color_metadata << ')';
#endif  // SB_API_VERSION >= 11
  return os;
}

std::string GetHexRepresentation(const uint8_t* data, int size) {
  const char kBinToHex[] = "0123456789abcdef";

  std::string result;

  for (int i = 0; i < size; ++i) {
    result += kBinToHex[data[i] / 16];
    result += kBinToHex[data[i] % 16];
    if (i != size - 1) {
      result += ' ';
    }
  }

  return result;
}

std::string GetStringRepresentation(const uint8_t* data, int size) {
  std::string result;

  for (int i = 0; i < size; ++i) {
    if (std::isspace(data[i])) {
      result += ' ';
    } else if (std::isprint(data[i])) {
      result += data[i];
    } else {
      result += '?';
    }
  }

  return result;
}

std::string GetMixedRepresentation(const uint8_t* data,
                                   int size,
                                   int bytes_per_line) {
  std::string result;

  for (int i = 0; i < size; i += bytes_per_line) {
    if (i + bytes_per_line <= size) {
      result += GetHexRepresentation(data + i, bytes_per_line);
      result += " | ";
      result += GetStringRepresentation(data + i, bytes_per_line);
      result += '\n';
    } else {
      int bytes_left = size - i;
      result += GetHexRepresentation(data + i, bytes_left);
      result += std::string((bytes_per_line - bytes_left) * 3, ' ');
      result += " | ";
      result += GetStringRepresentation(data + i, bytes_left);
      result += std::string(bytes_per_line - bytes_left, ' ');
      result += '\n';
    }
  }

  return result;
}
