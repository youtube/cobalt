// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>

#include "starboard/common/media.h"

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"

namespace starboard {

const char* GetMediaAudioCodecName(SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecNone:
      return "none";
    case kSbMediaAudioCodecAac:
      return "aac";
#if SB_API_VERSION >= 12 || SB_HAS(AC3_AUDIO)
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
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(AC3_AUDIO)
    case kSbMediaAudioCodecOpus:
      return "opus";
    case kSbMediaAudioCodecVorbis:
      return "vorbis";
  }
  SB_NOTREACHED();
  return "invalid";
}

const char* GetMediaVideoCodecName(SbMediaVideoCodec codec) {
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
    case kSbMediaVideoCodecAv1:
      return "av1";
    case kSbMediaVideoCodecVp8:
      return "vp8";
    case kSbMediaVideoCodecVp9:
      return "vp9";
  }
  SB_NOTREACHED();
  return "invalid";
}

const char* GetMediaPrimaryIdName(SbMediaPrimaryId primary_id) {
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

const char* GetMediaTransferIdName(SbMediaTransferId transfer_id) {
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

const char* GetMediaMatrixIdName(SbMediaMatrixId matrix_id) {
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

const char* GetMediaRangeIdName(SbMediaRangeId range_id) {
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

}  // namespace starboard

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
  using starboard::GetMediaPrimaryIdName;
  using starboard::GetMediaTransferIdName;
  using starboard::GetMediaMatrixIdName;
  using starboard::GetMediaRangeIdName;

  os << metadata.bits_per_channel
     << " bits, mastering metadata: " << metadata.mastering_metadata
     << ", chroma subsampling horizontal: "
     << metadata.chroma_subsampling_horizontal
     << ", chroma subsampling vertical: "
     << metadata.chroma_subsampling_vertical
     << ", cb subsampling horizontal: " << metadata.cb_subsampling_horizontal
     << ", cb subsampling vertical: " << metadata.cb_subsampling_vertical
     << ", chroma sitting horizontal: " << metadata.chroma_siting_horizontal
     << ", chroma sitting vertical: " << metadata.chroma_siting_vertical
     << ", max cll: " << metadata.max_cll << ", max fall: " << metadata.max_fall
     << ", primary: " << GetMediaPrimaryIdName(metadata.primaries)
     << ", transfer: " << GetMediaTransferIdName(metadata.transfer)
     << ", matrix: " << GetMediaMatrixIdName(metadata.matrix)
     << ", range: " << GetMediaRangeIdName(metadata.range);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SbMediaVideoSampleInfo& sample_info) {
  using starboard::GetMediaVideoCodecName;

  if (sample_info.codec == kSbMediaVideoCodecNone) {
    return os;
  }
  os << "codec: " << GetMediaVideoCodecName(sample_info.codec) << ", ";
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  os << "mime: " << (sample_info.mime ? sample_info.mime : "<null>")
     << ", max video capabilities: "
     << (sample_info.max_video_capabilities ? sample_info.max_video_capabilities
                                            : "<null>")
     << ", ";
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  if (sample_info.is_key_frame) {
    os << "key frame, ";
  }
  os << sample_info.frame_width << 'x' << sample_info.frame_height << ' ';
  os << '(' << sample_info.color_metadata << ')';
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SbMediaAudioSampleInfo& sample_info) {
  using starboard::GetMediaAudioCodecName;
  using starboard::HexEncode;

  if (sample_info.codec == kSbMediaAudioCodecNone) {
    return os;
  }
  os << "codec: " << GetMediaAudioCodecName(sample_info.codec) << ", ";
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  os << "mime: " << (sample_info.mime ? sample_info.mime : "<null>");
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  os << "channels: " << sample_info.number_of_channels
     << ", sample rate: " << sample_info.samples_per_second
     << ", config: " << sample_info.audio_specific_config_size << " bytes, "
     << "["
     << HexEncode(
            sample_info.audio_specific_config,
            std::min(static_cast<int>(sample_info.audio_specific_config_size),
                     16),
            " ")
     << (sample_info.audio_specific_config_size > 16 ? " ...]" : " ]");
  return os;
}
