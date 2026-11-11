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

#include "starboard/common/media.h"

#include <string.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace starboard {

namespace {

bool StartsWith(std::string_view str, std::string_view prefix) {
  return str.substr(0, prefix.size()) == prefix;
}

int hex_to_int(char hex) {
  if (hex >= '0' && hex <= '9') {
    return hex - '0';
  }
  if (hex >= 'A' && hex <= 'F') {
    return hex - 'A' + 10;
  }
  if (hex >= 'a' && hex <= 'f') {
    return hex - 'a' + 10;
  }
  SB_NOTREACHED();
  return 0;
}

// Read one digit hex from the first character of |str|. Return std::nullopt
// if |str| is empty or the character is not a hex digit.
template <typename T>
std::optional<T> ReadOneDigitHex(std::string_view str) {
  if (str.empty()) {
    return std::nullopt;
  }

  if (str[0] >= 'A' && str[0] <= 'F') {
    return static_cast<T>((str[0] - 'A' + 10));
  }
  if (str[0] >= 'a' && str[0] <= 'f') {
    return static_cast<T>((str[0] - 'a' + 10));
  }

  if (!isdigit(str[0])) {
    return std::nullopt;
  }

  return static_cast<T>((str[0] - '0'));
}

// Read multi digit decimal from |str| until the next '.' character or end of
// string. Return std::nullopt if one of the character is not a digit.
template <typename T>
std::optional<T> ReadDecimalUntilDot(std::string_view str) {
  T output = 0;
  for (char c : str) {
    if (c == '.') {
      break;
    }
    if (!isdigit(c)) {
      return std::nullopt;
    }
    output = output * 10 + (c - '0');
  }

  return output;
}

// Read two digit decimal from the first two characters of |str|. Return
// std::nullopt if |str| has less than two characters or any of the first two
// characters is not a digit.
template <typename T>
std::optional<T> ReadTwoDigitDecimal(std::string_view str) {
  if (str.size() < 2 || !isdigit(str[0]) || !isdigit(str[1])) {
    return std::nullopt;
  }

  return static_cast<T>((str[0] - '0') * 10 + (str[1] - '0'));
}

// Verify the format against a reference using the following rules:
// 1. They must have the same size.
// 2. If reference[i] is a letter, format[i] must contain the *same* letter.
// 3. If reference[i] is a number, format[i] can contain *any* number.
// 4. If reference[i] is '.', format[i] must also be '.'.
// 5. If reference[i] is ?, format[i] can contain *any* character.
// 6. If |format| is longer than |reference|, then |format[reference_size]| can
//    not be '.' or digit.
// For example, both "av01.0.05M.08" and "av01.0.05M.08****" match reference
// "av01.0.05?.08", but "vp09.0.05M.08" or "vp09.0.05M.08." don't.
// The function returns true when |format| matches |reference|.
bool VerifyFormat(std::string_view format, std::string_view reference) {
  if (format.size() < reference.size()) {
    return false;
  }
  for (size_t i = 0; i < reference.size(); ++i) {
    if (isdigit(reference[i])) {
      if (!isdigit(format[i])) {
        return false;
      }
    } else if (std::isalpha(reference[i])) {
      if (reference[i] != format[i]) {
        return false;
      }
    } else if (reference[i] == '.') {
      if (format[i] != '.') {
        return false;
      }
    } else if (reference[i] != '?') {
      return false;
    }
  }
  if (format.size() == reference.size()) {
    return true;
  }
  return format[reference.size()] != '.' && !isdigit(format[reference.size()]);
}

// It works exactly the same as the above function, except that the size of
// |format| has to be exactly the same as the size of |reference|.
bool VerifyFormatStrictly(std::string_view format, std::string_view reference) {
  if (format.size() != reference.size()) {
    return false;
  }
  return VerifyFormat(format, reference);
}

// This function parses an av01 codec in the form of "av01.0.05M.08" or
// "av01.0.04M.10.0.110.09.16.09.0" as
// specified by https://aomediacodec.github.io/av1-isobmff/#codecsparam.
//
// Note that the spec also supports of different chroma subsamplings but the
// implementation always assume that it is 4:2:0 and returns std::nullopt when
// it isn't.
std::optional<ParsedVideoCodec> ParseAv1Info(std::string_view codec) {
  // The codec can only in one of the following formats:
  //   Full: av01.0.04M.10.0.110.09.16.09.0
  //   Short: av01.0.05M.08
  // When short format is used, it is assumed that the omitted parts are
  // "0.110.01.01.01.0".
  // All fields are fixed size and leading zero cannot be omitted, so the
  // expected sizes are known.
  constexpr std::string_view kShortFormReference = "av01.0.05?.08";
  constexpr std::string_view kLongFormReference =
      "av01.0.04?.10.0.110.09.16.09.0";

  // 1. Sanity check the format.
  if (!StartsWith(codec, "av01.")) {
    return std::nullopt;
  }

  ParsedVideoCodec info;
  info.codec = kSbMediaVideoCodecAv1;
  if (VerifyFormat(codec, kLongFormReference)) {
    codec = codec.substr(0, kLongFormReference.size());
  } else if (VerifyFormat(codec, kShortFormReference)) {
    codec = codec.substr(0, kShortFormReference.size());
  } else {
    return std::nullopt;
  }

  // 2. Parse profile, which can only be 0, 1, or 2.
  if (codec[5] < '0' || codec[5] > '2') {
    return std::nullopt;
  }
  info.profile = codec[5] - '0';

  // 3. Parse level, which is two digit value from 0 to 23, maps to level 2.0,
  //    2.1, 2.2, 2.3, 3.0, 3.1, 3.2, 3.3, ..., 7.0, 7.1, 7.2, 7.3.
  std::optional<int> level_value = ReadTwoDigitDecimal<int>(codec.substr(7));
  if (!level_value) {
    return std::nullopt;
  }

  if (*level_value > 23) {
    return std::nullopt;
  }
  // Level x.y is represented by integer |xy|, for example, 23 means level 2.3.
  info.level = (*level_value / 4 + 2) * 10 + (*level_value % 4);

  // 4. Parse tier, which can only be 'M' or 'H'
  if (codec[9] != 'M' && codec[9] != 'H') {
    return std::nullopt;
  }

  // 5. Parse bit depth, which can be value 08, 10, or 12.
  std::optional<int> bit_depth = ReadTwoDigitDecimal<int>(codec.substr(11));
  if (!bit_depth) {
    return std::nullopt;
  }
  info.bit_depth = *bit_depth;
  if (info.bit_depth != 8 && info.bit_depth != 10 && info.bit_depth != 12) {
    return std::nullopt;
  }

  // 6. Return now if it is a well-formed short form codec string.
  info.primary_id = kSbMediaPrimaryIdBt709;
  info.transfer_id = kSbMediaTransferIdBt709;
  info.matrix_id = kSbMediaMatrixIdBt709;

  if (codec.size() == kShortFormReference.size()) {
    return info;
  }

  // 7. Parse monochrome, which can only be 0 or 1.
  // Note that this value is not returned.
  if (codec[14] != '0' && codec[14] != '1') {
    return std::nullopt;
  }

  // 8. Parse chroma subsampling, which we only support 110.
  // Note that this value is not returned.
  if (codec.substr(16, 3) != "110") {
    return std::nullopt;
  }

  // 9. Parse color primaries, which can be 1 to 12, and 22 (EBU Tech. 3213-E).
  //    Note that 22 is not currently supported by Cobalt.
  std::optional<SbMediaPrimaryId> primary_id =
      ReadTwoDigitDecimal<SbMediaPrimaryId>(codec.substr(20));
  if (!primary_id) {
    return std::nullopt;
  }
  info.primary_id = *primary_id;
  SB_LOG_IF(WARNING, info.primary_id == 22)
      << codec << " uses primary id 22 (EBU Tech. 3213-E)."
      << " It is rejected because Cobalt doesn't support primary id 22.";
  if (info.primary_id < 1 || info.primary_id > 12) {
    return std::nullopt;
  }

  // 10. Parse transfer characteristics, which can be 0 to 18.
  std::optional<SbMediaTransferId> transfer_id =
      ReadTwoDigitDecimal<SbMediaTransferId>(codec.substr(23));
  if (!transfer_id) {
    return std::nullopt;
  }
  info.transfer_id = *transfer_id;
  if (info.transfer_id > 18) {
    return std::nullopt;
  }

  // 11. Parse matrix coefficients, which can be 0 to 14.
  //     Note that 12, 13, and 14 are not currently supported by Cobalt.
  std::optional<SbMediaMatrixId> matrix_id =
      ReadTwoDigitDecimal<SbMediaMatrixId>(codec.substr(26));
  if (!matrix_id) {
    return std::nullopt;
  }
  info.matrix_id = *matrix_id;
  if (info.matrix_id > 11) {
    return std::nullopt;
  }

  // 12. Parse color range, which can only be 0 or 1.
  if (codec[29] != '0' && codec[29] != '1') {
    return std::nullopt;
  }

  // 13. Return
  return info;
}

// This function parses an h264 codec in the form of {avc1|avc3}.PPCCLL as
// specified by https://tools.ietf.org/html/rfc6381#section-3.3.
//
// Note that the leading codec is not necessarily to be "avc1" or "avc3" per
// spec but this function only parses "avc1" and "avc3".  This function returns
// std::nullopt when |codec| doesn't contain a valid codec string.
std::optional<ParsedVideoCodec> ParseH264Info(std::string_view codec) {
  if (!StartsWith(codec, "avc1.") && !StartsWith(codec, "avc3.")) {
    return std::nullopt;
  }

  if (codec.size() != 11 || !isxdigit(codec[9]) || !isxdigit(codec[10])) {
    return std::nullopt;
  }

  ParsedVideoCodec info;
  info.codec = kSbMediaVideoCodecH264;
  info.profile = hex_to_int(codec[5]) * 16 + hex_to_int(codec[6]);
  info.level = hex_to_int(codec[9]) * 16 + hex_to_int(codec[10]);
  return info;
}

// This function parses an h265 codec as specified by ISO IEC 14496-15 dated
// 2012 or newer in the Annex E.3.  The codec will be in the form of:
//   hvc1.PPP.PCPCPCPC.TLLL.CB.CB.CB.CB.CB.CB, where
// PPP: 0 or 1 byte general_profile_space ('', 'A', 'B', or 'C') +
//        up to two bytes profile idc.
// PCPCPCPC: Profile compatibility, up to 32 bits hex, with leading 0 omitted.
// TLLL: One byte tier ('L' or 'H') + up to three bytes level.
// CB: Up to 6 constraint bytes, separated by '.'.
// Note that the above level in decimal = 30 * real level, i.e. 93 means level
// 3.1, 120 mean level 4.
// Please see the comment in the code for interactions between the various
// parts.
std::optional<ParsedVideoCodec> ParseH265Info(std::string_view codec) {
  if (!StartsWith(codec, "hev1.") && !StartsWith(codec, "hvc1.")) {
    return std::nullopt;
  }

  ParsedVideoCodec info;
  info.codec = kSbMediaVideoCodecH265;

  codec.remove_prefix(5);

  // Read profile space
  if (!codec.empty() &&
      (codec[0] == 'A' || codec[0] == 'B' || codec[0] == 'C')) {
    codec.remove_prefix(1);
  }

  if (codec.size() < 3) {
    return std::nullopt;
  }

  // Read profile
  std::optional<int> general_profile_idc;
  if (codec[1] == '.') {
    general_profile_idc = ReadDecimalUntilDot<int>(codec);
    if (!general_profile_idc) {
      return std::nullopt;
    }
    codec.remove_prefix(2);
  } else if (codec[2] == '.') {
    general_profile_idc = ReadDecimalUntilDot<int>(codec);
    if (!general_profile_idc) {
      return std::nullopt;
    }
    codec.remove_prefix(3);
  } else {
    return std::nullopt;
  }

  // Read profile compatibility, up to 32 bits hex.
  const size_t dot = codec.find('.');
  if (dot == std::string_view::npos || dot == 0 || dot > 8) {
    return std::nullopt;
  }

  uint32_t general_profile_compatibility_flags = 0;
  for (int i = 0; i < 9; ++i) {
    if (codec[0] == '.') {
      codec.remove_prefix(1);
      break;
    }
    std::optional<uint32_t> hex = ReadOneDigitHex<uint32_t>(codec);
    if (!hex) {
      return std::nullopt;
    }
    general_profile_compatibility_flags *= 16;
    general_profile_compatibility_flags += *hex;
    codec.remove_prefix(1);
  }

  info.profile = -1;
  if (*general_profile_idc == 3 || (general_profile_compatibility_flags & 4)) {
    info.profile = 3;
  }
  if (*general_profile_idc == 2 || (general_profile_compatibility_flags & 2)) {
    info.profile = 2;
  }
  if (*general_profile_idc == 1 || (general_profile_compatibility_flags & 1)) {
    info.profile = 1;
  }
  if (info.profile == -1) {
    return std::nullopt;
  }

  // Parse tier
  if (codec.empty() || (codec[0] != 'L' && codec[0] != 'H')) {
    return std::nullopt;
  }
  codec.remove_prefix(1);

  // Parse level in 2 or 3 digits decimal.
  if (codec.size() < 2) {
    return std::nullopt;
  }
  std::optional<int> level = ReadDecimalUntilDot<int>(codec);
  if (!level) {
    return std::nullopt;
  }
  info.level = *level;
  if (info.level % 3 != 0) {
    return std::nullopt;
  }
  info.level /= 3;
  if (codec.size() == 2 || codec[2] == '.') {
    codec.remove_prefix(2);
  } else if (codec.size() == 3 || codec[3] == '.') {
    codec.remove_prefix(3);
  } else {
    return std::nullopt;
  }

  // Parse up to 6 constraint flags in the form of ".HH".
  for (int i = 0; i < 6; ++i) {
    if (codec.empty()) {
      return info;
    }
    if (codec.size() < 3 || codec[0] != '.' || !isxdigit(codec[1]) ||
        !isxdigit(codec[2])) {
      return std::nullopt;
    }
    codec.remove_prefix(3);
  }

  if (!codec.empty()) {
    return std::nullopt;
  }
  return info;
}

// This function parses an vp09 codec in the form of "vp09.00.41.08" or
// "vp09.02.10.10.01.09.16.09.01" as specified by
// https://www.webmproject.org/vp9/mp4/.  YouTube also uses the long form
// without the last part (color range), so we also support it.
//
// Note that the spec also supports of different chroma subsamplings but the
// implementation always assume that it is 4:2:0 and returns std::nullopt when
// it isn't.
std::optional<ParsedVideoCodec> ParseVp09Info(std::string_view codec) {
  // The codec can only in one of the following formats:
  //   Full: vp09.02.10.10.01.09.16.09.01
  //   Short: vp09.00.41.08
  // Note that currently the player also uses the following form:
  //   Medium: vp09.02.10.10.01.09.16.09
  // When short format is used, it is assumed that the omitted parts are
  // "01.01.01.01.00".  When medium format is used, the omitted part is "00".
  // All fields are fixed size and leading zero cannot be omitted, so the
  // expected sizes are known.
  constexpr std::string_view kShortFormReference = "vp09.00.41.08";
  constexpr std::string_view kMediumFormReference = "vp09.02.10.10.01.09.16.09";
  constexpr std::string_view kLongFormReference =
      "vp09.02.10.10.01.09.16.09.01";

  // 1. Sanity check the format.
  if (!StartsWith(codec, "vp09.")) {
    return std::nullopt;
  }
  if (!VerifyFormatStrictly(codec, kLongFormReference) &&
      !VerifyFormatStrictly(codec, kMediumFormReference) &&
      !VerifyFormatStrictly(codec, kShortFormReference)) {
    return std::nullopt;
  }

  ParsedVideoCodec info;
  info.codec = kSbMediaVideoCodecVp9;

  // 2. Parse profile, which can only be 00, 01, 02, or 03.
  std::optional<int> profile = ReadTwoDigitDecimal<int>(codec.substr(5));
  if (!profile) {
    return std::nullopt;
  }
  info.profile = *profile;
  if (info.profile < 0 || info.profile > 3) {
    return std::nullopt;
  }

  // 3. Parse level, which is two digit value in the following list:
  const int kLevels[] = {10, 11, 20, 21, 30, 31, 40,
                         41, 50, 51, 52, 60, 61, 62};
  std::optional<int> level = ReadTwoDigitDecimal<int>(codec.substr(8));
  if (!level) {
    return std::nullopt;
  }
  info.level = *level;
  auto end = kLevels + SB_ARRAY_SIZE(kLevels);
  if (std::find(kLevels, end, info.level) == end) {
    return std::nullopt;
  }

  // 4. Parse bit depth, which can be value 08, 10, or 12.
  std::optional<int> bit_depth = ReadTwoDigitDecimal<int>(codec.substr(11));
  if (!bit_depth) {
    return std::nullopt;
  }
  info.bit_depth = *bit_depth;
  if (info.bit_depth != 8 && info.bit_depth != 10 && info.bit_depth != 12) {
    return std::nullopt;
  }

  // 5. Return now if it is a well-formed short form codec string.
  info.primary_id = kSbMediaPrimaryIdBt709;
  info.transfer_id = kSbMediaTransferIdBt709;
  info.matrix_id = kSbMediaMatrixIdBt709;

  if (codec.size() == kShortFormReference.size()) {
    return info;
  }

  // 6. Parse chroma subsampling, which we only support 00 and 01.
  // Note that this value is not returned.
  std::optional<int> chroma = ReadTwoDigitDecimal<int>(codec.substr(14));
  if (!chroma || (*chroma != 0 && *chroma != 1)) {
    return std::nullopt;
  }

  // 7. Parse color primaries, which can be 1 to 12, and 22 (EBU Tech. 3213-E).
  //    Note that 22 is not currently supported by Cobalt.
  std::optional<SbMediaPrimaryId> primary_id =
      ReadTwoDigitDecimal<SbMediaPrimaryId>(codec.substr(17));
  if (!primary_id) {
    return std::nullopt;
  }
  info.primary_id = *primary_id;
  if (info.primary_id < 1 || info.primary_id > 12) {
    return std::nullopt;
  }

  // 8. Parse transfer characteristics, which can be 0 to 18.
  std::optional<SbMediaTransferId> transfer_id =
      ReadTwoDigitDecimal<SbMediaTransferId>(codec.substr(20));
  if (!transfer_id) {
    return std::nullopt;
  }
  info.transfer_id = *transfer_id;
  if (info.transfer_id > 18) {
    return std::nullopt;
  }

  // 9. Parse matrix coefficients, which can be 0 to 14.
  //     Note that 12, 13, and 14 are not currently supported by Cobalt.
  std::optional<SbMediaMatrixId> matrix_id =
      ReadTwoDigitDecimal<SbMediaMatrixId>(codec.substr(23));
  if (!matrix_id) {
    return std::nullopt;
  }
  info.matrix_id = *matrix_id;
  if (info.matrix_id > 11) {
    return std::nullopt;
  }

  // 10. Return now if it is a well-formed medium form codec string.
  if (codec.size() == kMediumFormReference.size()) {
    return info;
  }

  // 11. Parse color range, which can only be 0 or 1.
  std::optional<int> color_range = ReadTwoDigitDecimal<int>(codec.substr(26));
  if (!color_range) {
    return std::nullopt;
  }
  if (*color_range != 0 && *color_range != 1) {
    return std::nullopt;
  }

  // 12. Return
  return info;
}

// This function parses a vp9 codec in the form of "vp9", "vp9.0", "vp9.1",
// "vp9.2", or "vp9.3".
std::optional<ParsedVideoCodec> ParseVp9Info(std::string_view codec) {
  ParsedVideoCodec info;
  info.codec = kSbMediaVideoCodecVp9;

  if (codec == "vp9") {
    info.profile = -1;
    info.bit_depth = 8;
    return info;
  }
  if (codec == "vp9.0") {
    info.profile = 0;
    info.bit_depth = 8;
    return info;
  }
  if (codec == "vp9.1") {
    info.profile = 1;
    info.bit_depth = 8;
    return info;
  }
  if (codec == "vp9.2") {
    info.profile = 2;
    info.bit_depth = 10;
    return info;
  }
  if (codec == "vp9.3") {
    info.profile = 3;
    info.bit_depth = 10;
    return info;
  }
  return std::nullopt;
}

}  // namespace

const char* GetMediaAudioCodecName(SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecNone:
      return "none";
    case kSbMediaAudioCodecAac:
      return "aac";
    case kSbMediaAudioCodecAc3:
      return "ac3";
    case kSbMediaAudioCodecEac3:
      return "ec3";
    case kSbMediaAudioCodecOpus:
      return "opus";
    case kSbMediaAudioCodecVorbis:
      return "vorbis";
    case kSbMediaAudioCodecMp3:
      return "mp3";
    case kSbMediaAudioCodecFlac:
      return "flac";
    case kSbMediaAudioCodecPcm:
      return "pcm";
    case kSbMediaAudioCodecIamf:
      return "iamf";
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
    case kSbMediaVideoCodecAv2:
      return "av2";
  }
  SB_NOTREACHED();
  return "invalid";
}

const char* GetMediaAudioConnectorName(SbMediaAudioConnector connector) {
  switch (connector) {
    case kSbMediaAudioConnectorUnknown:
      return "unknown";
    case kSbMediaAudioConnectorAnalog:
      return "analog";
    case kSbMediaAudioConnectorBluetooth:
      return "bluetooth";
    case kSbMediaAudioConnectorBuiltIn:
      return "builtin";
    case kSbMediaAudioConnectorHdmi:
      return "hdmi";
    case kSbMediaAudioConnectorRemoteWired:
      return "remote-wired";
    case kSbMediaAudioConnectorRemoteWireless:
      return "remote-wireless";
    case kSbMediaAudioConnectorRemoteOther:
      return "remote-other";
    case kSbMediaAudioConnectorSpdif:
      return "spdif";
    case kSbMediaAudioConnectorUsb:
      return "usb";
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
    case kSbMediaMatrixIdInvalid:
      return "Invalid";
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

const char* GetMediaAudioSampleTypeName(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeFloat32:
      return "float32";
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return "int16";
  }
  SB_NOTREACHED();
  return "Invalid";
}

std::optional<ParsedVideoCodec> ParseVideoCodec(std::string_view codec_string) {
  if (StartsWith(codec_string, "av01.")) {
    return ParseAv1Info(codec_string);
  }
  if (StartsWith(codec_string, "avc1.") || StartsWith(codec_string, "avc3.")) {
    return ParseH264Info(codec_string);
  }
  if (StartsWith(codec_string, "hev1.") || StartsWith(codec_string, "hvc1.")) {
    return ParseH265Info(codec_string);
  }
  if (StartsWith(codec_string, "vp09.")) {
    return ParseVp09Info(codec_string);
  }
  if (StartsWith(codec_string, "vp8")) {
    ParsedVideoCodec info;
    info.codec = kSbMediaVideoCodecVp8;
    return info;
  }
  if (StartsWith(codec_string, "vp9")) {
    return ParseVp9Info(codec_string);
  }

  return std::nullopt;
}

std::ostream& operator<<(std::ostream& os, const ParsedVideoCodec& codec_info) {
  return os << "codec=" << GetMediaVideoCodecName(codec_info.codec)
            << ", profile=" << codec_info.profile
            << ", level=" << codec_info.level
            << ", bit_depth=" << codec_info.bit_depth
            << ", primary_id=" << GetMediaPrimaryIdName(codec_info.primary_id)
            << ", transfer_id="
            << GetMediaTransferIdName(codec_info.transfer_id)
            << ", matrix_id=" << GetMediaMatrixIdName(codec_info.matrix_id);
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
  using starboard::GetMediaMatrixIdName;
  using starboard::GetMediaPrimaryIdName;
  using starboard::GetMediaRangeIdName;
  using starboard::GetMediaTransferIdName;

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

  const SbMediaVideoStreamInfo& stream_info = sample_info.stream_info;

  if (stream_info.codec == kSbMediaVideoCodecNone) {
    return os;
  }

  os << "codec: " << GetMediaVideoCodecName(stream_info.codec) << ", ";
  os << "mime: " << (stream_info.mime ? stream_info.mime : "<null>")
     << ", max video capabilities: "
     << (stream_info.max_video_capabilities ? stream_info.max_video_capabilities
                                            : "<null>")
     << ", ";

  if (sample_info.is_key_frame) {
    os << "key frame, ";
  }

  os << stream_info.frame_width << 'x' << stream_info.frame_height << ' ';
  os << '(' << stream_info.color_metadata << ')';

  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SbMediaAudioSampleInfo& sample_info) {
  os << sample_info.stream_info;

  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SbMediaVideoStreamInfo& stream_info) {
  using starboard::GetMediaVideoCodecName;

  if (stream_info.codec == kSbMediaVideoCodecNone) {
    return os;
  }

  os << "codec: " << GetMediaVideoCodecName(stream_info.codec) << ", ";
  os << "mime: " << (stream_info.mime ? stream_info.mime : "<null>")
     << ", max video capabilities: "
     << (stream_info.max_video_capabilities ? stream_info.max_video_capabilities
                                            : "<null>")
     << ", ";

  os << stream_info.frame_width << 'x' << stream_info.frame_height << ' ';
  os << '(' << stream_info.color_metadata << ')';

  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const SbMediaAudioStreamInfo& stream_info) {
  using starboard::GetMediaAudioCodecName;
  using starboard::HexEncode;

  if (stream_info.codec == kSbMediaAudioCodecNone) {
    return os;
  }

  os << "codec: " << GetMediaAudioCodecName(stream_info.codec) << ", ";
  os << "mime: " << (stream_info.mime ? stream_info.mime : "<null>");
  os << ", channels: " << stream_info.number_of_channels
     << ", sample rate: " << stream_info.samples_per_second
     << ", config: " << stream_info.audio_specific_config_size << " bytes, "
     << "["
     << HexEncode(
            stream_info.audio_specific_config,
            std::min(static_cast<int>(stream_info.audio_specific_config_size),
                     16),
            " ")
     << (stream_info.audio_specific_config_size > 16 ? " ...]" : " ]");

  return os;
}

bool operator==(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return memcmp(&metadata_1, &metadata_2, sizeof(SbMediaColorMetadata)) == 0;
}

bool operator==(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
  const SbMediaVideoStreamInfo& stream_info_1 = sample_info_1.stream_info;
  const SbMediaVideoStreamInfo& stream_info_2 = sample_info_2.stream_info;

  if (stream_info_1.codec != stream_info_2.codec) {
    return false;
  }
  if (stream_info_1.codec == kSbMediaVideoCodecNone) {
    return true;
  }

  if (strcmp(stream_info_1.mime, stream_info_2.mime) != 0) {
    return false;
  }
  if (strcmp(stream_info_1.max_video_capabilities,
             stream_info_2.max_video_capabilities) != 0) {
    return false;
  }

  if (sample_info_1.is_key_frame != sample_info_2.is_key_frame) {
    return false;
  }
  if (stream_info_1.frame_width != stream_info_2.frame_width) {
    return false;
  }
  if (stream_info_1.frame_height != stream_info_2.frame_height) {
    return false;
  }
  return stream_info_1.color_metadata == stream_info_2.color_metadata;
}

bool operator==(const SbMediaVideoStreamInfo& stream_info_1,
                const SbMediaVideoStreamInfo& stream_info_2) {
  if (stream_info_1.codec != stream_info_2.codec) {
    return false;
  }
  if (stream_info_1.codec == kSbMediaVideoCodecNone) {
    return true;
  }

  if (strcmp(stream_info_1.mime, stream_info_2.mime) != 0) {
    return false;
  }
  if (strcmp(stream_info_1.max_video_capabilities,
             stream_info_2.max_video_capabilities) != 0) {
    return false;
  }
  if (stream_info_1.frame_width != stream_info_2.frame_width) {
    return false;
  }
  if (stream_info_1.frame_height != stream_info_2.frame_height) {
    return false;
  }
  return stream_info_1.color_metadata == stream_info_2.color_metadata;
}

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return !(metadata_1 == metadata_2);
}

bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
  return !(sample_info_1 == sample_info_2);
}

bool operator!=(const SbMediaVideoStreamInfo& stream_info_1,
                const SbMediaVideoStreamInfo& stream_info_2) {
  return !(stream_info_1 == stream_info_2);
}
