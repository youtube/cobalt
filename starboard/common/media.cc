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
#include <cctype>
#include <string>

#include "starboard/common/media.h"

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"

namespace starboard {

namespace {

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

// Read one digit hex from |str| and store into |output|. Return false if
// the character is not a digit, return true otherwise.
template <typename T>
bool ReadOneDigitHex(const char* str, T* output) {
  SB_DCHECK(str);

  if (str[0] >= 'A' && str[0] <= 'F') {
    *output = static_cast<T>((str[0] - 'A' + 10));
    return true;
  }
  if (str[0] >= 'a' && str[0] <= 'f') {
    *output = static_cast<T>((str[0] - 'a' + 10));
    return true;
  }

  if (!isdigit(str[0])) {
    return false;
  }

  *output = static_cast<T>((str[0] - '0'));
  return true;
}

// Read multi digit decimal from |str| until the next '.' character or end of
// string, and store into |output|. Return false if one of the character is not
// a digit, return true otherwise.
template <typename T>
bool ReadDecimalUntilDot(const char* str, T* output) {
  SB_DCHECK(str);

  *output = 0;
  while (*str != 0 && *str != '.') {
    if (!isdigit(*str)) {
      return false;
    }
    *output = *output * 10 + (*str - '0');
    ++str;
  }

  return true;
}

// Read two digit decimal from |str| and store into |output|. Return false if
// any character is not a digit, return true otherwise.
template <typename T>
bool ReadTwoDigitDecimal(const char* str, T* output) {
  SB_DCHECK(str);

  if (!isdigit(str[0]) || !isdigit(str[1])) {
    return false;
  }

  *output = static_cast<T>((str[0] - '0') * 10 + (str[1] - '0'));
  return true;
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
bool VerifyFormat(const char* format, const char* reference) {
  auto format_size = strlen(format);
  auto reference_size = strlen(reference);
  if (format_size < reference_size) {
    return false;
  }
  for (size_t i = 0; i < reference_size; ++i) {
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
  if (format_size == reference_size) {
    return true;
  }
  return format[reference_size] != '.' && !isdigit(format[reference_size]);
}

// It works exactly the same as the above function, except that the size of
// |format| has to be exactly the same as the size of |reference|.
bool VerifyFormatStrictly(const char* format, const char* reference) {
  if (strlen(format) != strlen(reference)) {
    return false;
  }
  return VerifyFormat(format, reference);
}

// This function parses an av01 codec in the form of "av01.0.05M.08" or
// "av01.0.04M.10.0.110.09.16.09.0" as
// specified by https://aomediacodec.github.io/av1-isobmff/#codecsparam.
//
// Note that the spec also supports of different chroma subsamplings but the
// implementation always assume that it is 4:2:0 and returns false when it
// isn't.
bool ParseAv1Info(std::string codec,
                  int* profile,
                  int* level,
                  int* bit_depth,
                  SbMediaPrimaryId* primary_id,
                  SbMediaTransferId* transfer_id,
                  SbMediaMatrixId* matrix_id) {
  // The codec can only in one of the following formats:
  //   Full: av01.0.04M.10.0.110.09.16.09.0
  //   Short: av01.0.05M.08
  // When short format is used, it is assumed that the omitted parts are
  // "0.110.01.01.01.0".
  // All fields are fixed size and leading zero cannot be omitted, so the
  // expected sizes are known.
  const char kShortFormReference[] = "av01.0.05M.08";
  const char kLongFormReference[] = "av01.0.04M.10.0.110.09.16.09.0";
  const size_t kShortFormSize = strlen(kShortFormReference);
  const size_t kLongFormSize = strlen(kLongFormReference);

  // 1. Sanity check the format.
  if (strncmp(codec.c_str(), "av01.", 5) != 0) {
    return false;
  }
  if (VerifyFormat(codec.c_str(), kLongFormReference)) {
    codec.resize(kLongFormSize);
  } else if (VerifyFormat(codec.c_str(), kShortFormReference)) {
    codec.resize(kShortFormSize);
  } else {
    return false;
  }

  // 2. Parse profile, which can only be 0, 1, or 2.
  if (codec[5] < '0' || codec[5] > '2') {
    return false;
  }
  *profile = codec[5] - '0';

  // 3. Parse level, which is two digit value from 0 to 23, maps to level 2.0,
  //    2.1, 2.2, 2.3, 3.0, 3.1, 3.2, 3.3, ..., 7.0, 7.1, 7.2, 7.3.
  int level_value;
  if (!ReadTwoDigitDecimal(codec.c_str() + 7, &level_value)) {
    return false;
  }

  if (level_value > 23) {
    return false;
  }
  // Level x.y is represented by integer |xy|, for example, 23 means level 2.3.
  *level = (level_value / 4 + 2) * 10 + (level_value % 4);

  // 4. Parse tier, which can only be 'M' or 'H'
  if (codec[9] != 'M' && codec[9] != 'H') {
    return false;
  }

  // 5. Parse bit depth, which can be value 08, 10, or 12.
  if (!ReadTwoDigitDecimal(codec.c_str() + 11, bit_depth)) {
    return false;
  }
  if (*bit_depth != 8 && *bit_depth != 10 && *bit_depth != 12) {
    return false;
  }

  // 6. Return now if it is a well-formed short form codec string.
  *primary_id = kSbMediaPrimaryIdBt709;
  *transfer_id = kSbMediaTransferIdBt709;
  *matrix_id = kSbMediaMatrixIdBt709;

  if (codec.size() == kShortFormSize) {
    return true;
  }

  // 7. Parse monochrome, which can only be 0 or 1.
  // Note that this value is not returned.
  if (codec[14] != '0' && codec[14] != '1') {
    return false;
  }

  // 8. Parse chroma subsampling, which we only support 110.
  // Note that this value is not returned.
  if (strncmp(codec.c_str() + 16, "110", 3) != 0) {
    return false;
  }

  // 9. Parse color primaries, which can be 1 to 12, and 22 (EBU Tech. 3213-E).
  //    Note that 22 is not currently supported by Cobalt.
  if (!ReadTwoDigitDecimal(codec.c_str() + 20, primary_id)) {
    return false;
  }
  SB_LOG_IF(WARNING, *primary_id == 22)
      << codec << " uses primary id 22 (EBU Tech. 3213-E)."
      << " It is rejected because Cobalt doesn't support primary id 22.";
  if (*primary_id < 1 || *primary_id > 12) {
    return false;
  }

  // 10. Parse transfer characteristics, which can be 0 to 18.
  if (!ReadTwoDigitDecimal(codec.c_str() + 23, transfer_id)) {
    return false;
  }
  if (*transfer_id > 18) {
    return false;
  }

  // 11. Parse matrix coefficients, which can be 0 to 14.
  //     Note that 12, 13, and 14 are not currently supported by Cobalt.
  if (!ReadTwoDigitDecimal(codec.c_str() + 26, matrix_id)) {
    return false;
  }
  if (*matrix_id > 11) {
    return false;
  }

  // 12. Parse color range, which can only be 0 or 1.
  if (codec[29] != '0' && codec[29] != '1') {
    return false;
  }

  // 13. Return
  return true;
}

// This function parses an h264 codec in the form of {avc1|avc3}.PPCCLL as
// specified by https://tools.ietf.org/html/rfc6381#section-3.3.
//
// Note that the leading codec is not necessarily to be "avc1" or "avc3" per
// spec but this function only parses "avc1" and "avc3".  This function returns
// false when |codec| doesn't contain a valid codec string.
bool ParseH264Info(const char* codec, int* profile, int* level) {
  if (strncmp(codec, "avc1.", 5) != 0 && strncmp(codec, "avc3.", 5) != 0) {
    return false;
  }

  if (strlen(codec) != 11 || !isxdigit(codec[9]) || !isxdigit(codec[10])) {
    return false;
  }

  *profile = hex_to_int(codec[5]) * 16 + hex_to_int(codec[6]);
  *level = hex_to_int(codec[9]) * 16 + hex_to_int(codec[10]);
  return true;
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
bool ParseH265Info(const char* codec, int* profile, int* level) {
  if (strncmp(codec, "hev1.", 5) != 0 && strncmp(codec, "hvc1.", 5) != 0) {
    return false;
  }

  codec += 5;

  // Read profile space
  if (codec[0] == 'A' || codec[0] == 'B' || codec[0] == 'C') {
    ++codec;
  }

  if (strlen(codec) < 3) {
    return false;
  }

  // Read profile
  int general_profile_idc;
  if (codec[1] == '.') {
    if (!ReadDecimalUntilDot(codec, &general_profile_idc)) {
      return false;
    }
    codec += 2;
  } else if (codec[2] == '.') {
    if (!ReadDecimalUntilDot(codec, &general_profile_idc)) {
      return false;
    }
    codec += 3;
  } else {
    return false;
  }

  // Read profile compatibility, up to 32 bits hex.
  const char* dot = strchr(codec, '.');
  if (dot == NULL || dot - codec == 0 || dot - codec > 8) {
    return false;
  }

  uint32_t general_profile_compatibility_flags = 0;
  for (int i = 0; i < 9; ++i) {
    if (codec[0] == '.') {
      ++codec;
      break;
    }
    uint32_t hex = 0;
    if (!ReadOneDigitHex(codec, &hex)) {
      return false;
    }
    general_profile_compatibility_flags *= 16;
    general_profile_compatibility_flags += hex;
    ++codec;
  }

  *profile = -1;
  if (general_profile_idc == 3 || (general_profile_compatibility_flags & 4)) {
    *profile = 3;
  }
  if (general_profile_idc == 2 || (general_profile_compatibility_flags & 2)) {
    *profile = 2;
  }
  if (general_profile_idc == 1 || (general_profile_compatibility_flags & 1)) {
    *profile = 1;
  }
  if (*profile == -1) {
    return false;
  }

  // Parse tier
  if (codec[0] != 'L' && codec[0] != 'H') {
    return false;
  }
  ++codec;

  // Parse level in 2 or 3 digits decimal.
  if (strlen(codec) < 2) {
    return false;
  }
  if (!ReadDecimalUntilDot(codec, level)) {
    return false;
  }
  if (*level % 3 != 0) {
    return false;
  }
  *level /= 3;
  if (codec[2] == 0 || codec[2] == '.') {
    codec += 2;
  } else if (codec[3] == 0 || codec[3] == '.') {
    codec += 3;
  } else {
    return false;
  }

  // Parse up to 6 constraint flags in the form of ".HH".
  for (int i = 0; i < 6; ++i) {
    if (codec[0] == 0) {
      return true;
    }
    if (codec[0] != '.' || !isxdigit(codec[1]) || !isxdigit(codec[2])) {
      return false;
    }
    codec += 3;
  }

  return *codec == 0;
}

// This function parses an vp09 codec in the form of "vp09.00.41.08" or
// "vp09.02.10.10.01.09.16.09.01" as specified by
// https://www.webmproject.org/vp9/mp4/.  YouTube also uses the long form
// without the last part (color range), so we also support it.
//
// Note that the spec also supports of different chroma subsamplings but the
// implementation always assume that it is 4:2:0 and returns false when it
// isn't.
bool ParseVp09Info(const char* codec,
                   int* profile,
                   int* level,
                   int* bit_depth,
                   SbMediaPrimaryId* primary_id,
                   SbMediaTransferId* transfer_id,
                   SbMediaMatrixId* matrix_id) {
  // The codec can only in one of the following formats:
  //   Full: vp09.02.10.10.01.09.16.09.01
  //   Short: vp09.00.41.08
  // Note that currently the player also uses the following form:
  //   Medium: vp09.02.10.10.01.09.16.09
  // When short format is used, it is assumed that the omitted parts are
  // "01.01.01.01.00".  When medium format is used, the omitted part is "00".
  // All fields are fixed size and leading zero cannot be omitted, so the
  // expected sizes are known.
  const char kShortFormReference[] = "vp09.00.41.08";
  const char kMediumFormReference[] = "vp09.02.10.10.01.09.16.09";
  const char kLongFormReference[] = "vp09.02.10.10.01.09.16.09.01";
  const size_t kShortFormSize = strlen(kShortFormReference);
  const size_t kMediumFormSize = strlen(kMediumFormReference);
  const size_t kLongFormSize = strlen(kLongFormReference);

  // 1. Sanity check the format.
  if (strncmp(codec, "vp09.", 5) != 0) {
    return false;
  }
  if (!VerifyFormatStrictly(codec, kLongFormReference) &&
      !VerifyFormatStrictly(codec, kMediumFormReference) &&
      !VerifyFormatStrictly(codec, kShortFormReference)) {
    return false;
  }

  // 2. Parse profile, which can only be 00, 01, 02, or 03.
  if (!ReadTwoDigitDecimal(codec + 5, profile)) {
    return false;
  }
  if (*profile < 0 || *profile > 3) {
    return false;
  }

  // 3. Parse level, which is two digit value in the following list:
  const int kLevels[] = {10, 11, 20, 21, 30, 31, 40,
                         41, 50, 51, 52, 60, 61, 62};
  if (!ReadTwoDigitDecimal(codec + 8, level)) {
    return false;
  }
  auto end = kLevels + SB_ARRAY_SIZE(kLevels);
  if (std::find(kLevels, end, *level) == end) {
    return false;
  }

  // 4. Parse bit depth, which can be value 08, 10, or 12.
  if (!ReadTwoDigitDecimal(codec + 11, bit_depth)) {
    return false;
  }
  if (*bit_depth != 8 && *bit_depth != 10 && *bit_depth != 12) {
    return false;
  }

  // 5. Return now if it is a well-formed short form codec string.
  *primary_id = kSbMediaPrimaryIdBt709;
  *transfer_id = kSbMediaTransferIdBt709;
  *matrix_id = kSbMediaMatrixIdBt709;

  if (strlen(codec) == kShortFormSize) {
    return true;
  }

  // 6. Parse chroma subsampling, which we only support 00 and 01.
  // Note that this value is not returned.
  int chroma;
  if (!ReadTwoDigitDecimal(codec + 14, &chroma) ||
      (chroma != 0 && chroma != 1)) {
    return false;
  }

  // 7. Parse color primaries, which can be 1 to 12, and 22 (EBU Tech. 3213-E).
  //    Note that 22 is not currently supported by Cobalt.
  if (!ReadTwoDigitDecimal(codec + 17, primary_id)) {
    return false;
  }
  if (*primary_id < 1 || *primary_id > 12) {
    return false;
  }

  // 8. Parse transfer characteristics, which can be 0 to 18.
  if (!ReadTwoDigitDecimal(codec + 20, transfer_id)) {
    return false;
  }
  if (*transfer_id > 18) {
    return false;
  }

  // 9. Parse matrix coefficients, which can be 0 to 14.
  //     Note that 12, 13, and 14 are not currently supported by Cobalt.
  if (!ReadTwoDigitDecimal(codec + 23, matrix_id)) {
    return false;
  }
  if (*matrix_id > 11) {
    return false;
  }

  // 10. Return now if it is a well-formed medium form codec string.
  if (strlen(codec) == kMediumFormSize) {
    return true;
  }

  // 11. Parse color range, which can only be 0 or 1.
  int color_range;
  if (!ReadTwoDigitDecimal(codec + 26, &color_range)) {
    return false;
  }
  if (color_range != 0 && color_range != 1) {
    return false;
  }

  // 12. Return
  return true;
}

// This function parses a vp9 codec in the form of "vp9", "vp9.0", "vp9.1",
// "vp9.2", or "vp9.3".
bool ParseVp9Info(const char* codec, int* profile, int* bit_depth) {
  SB_DCHECK(profile);
  SB_DCHECK(bit_depth);

  if (strcmp(codec, "vp9") == 0) {
    *profile = -1;
    *bit_depth = 8;
    return true;
  }
  if (strcmp(codec, "vp9.0") == 0) {
    *profile = 0;
    *bit_depth = 8;
    return true;
  }
  if (strcmp(codec, "vp9.1") == 0) {
    *profile = 1;
    *bit_depth = 8;
    return true;
  }
  if (strcmp(codec, "vp9.2") == 0) {
    *profile = 2;
    *bit_depth = 10;
    return true;
  }
  if (strcmp(codec, "vp9.3") == 0) {
    *profile = 3;
    *bit_depth = 10;
    return true;
  }
  return false;
}

}  // namespace

const char* GetMediaAudioCodecName(SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecNone:
      return "none";
    case kSbMediaAudioCodecAac:
      return "aac";
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
    case kSbMediaAudioCodecOpus:
      return "opus";
    case kSbMediaAudioCodecVorbis:
      return "vorbis";
#if SB_API_VERSION >= 14
    case kSbMediaAudioCodecMp3:
      return "mp3";
    case kSbMediaAudioCodecFlac:
      return "flac";
    case kSbMediaAudioCodecPcm:
      return "pcm";
#endif  // SB_API_VERSION >= 14
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
#if SB_API_VERSION >= 14
    case kSbMediaMatrixIdInvalid:
      return "Invalid";
#else   // SB_API_VERSION >= 14
    case kSbMediaMatrixIdUnknown:
      return "Unknown";
#endif  // SB_API_VERSION >= 14
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

bool ParseVideoCodec(const char* codec_string,
                     SbMediaVideoCodec* codec,
                     int* profile,
                     int* level,
                     int* bit_depth,
                     SbMediaPrimaryId* primary_id,
                     SbMediaTransferId* transfer_id,
                     SbMediaMatrixId* matrix_id) {
  SB_DCHECK(codec_string);
  SB_DCHECK(codec);
  SB_DCHECK(profile);
  SB_DCHECK(level);
  SB_DCHECK(bit_depth);
  SB_DCHECK(primary_id);
  SB_DCHECK(transfer_id);
  SB_DCHECK(matrix_id);

  *codec = kSbMediaVideoCodecNone;
  *profile = -1;
  *level = -1;
  *bit_depth = 8;
  *primary_id = kSbMediaPrimaryIdUnspecified;
  *transfer_id = kSbMediaTransferIdUnspecified;
  *matrix_id = kSbMediaMatrixIdUnspecified;

  if (strncmp(codec_string, "av01.", 5) == 0) {
    *codec = kSbMediaVideoCodecAv1;
    return ParseAv1Info(codec_string, profile, level, bit_depth, primary_id,
                        transfer_id, matrix_id);
  }
  if (strncmp(codec_string, "avc1.", 5) == 0 ||
      strncmp(codec_string, "avc3.", 5) == 0) {
    *codec = kSbMediaVideoCodecH264;
    return ParseH264Info(codec_string, profile, level);
  }
  if (strncmp(codec_string, "hev1.", 5) == 0 ||
      strncmp(codec_string, "hvc1.", 5) == 0) {
    *codec = kSbMediaVideoCodecH265;
    return ParseH265Info(codec_string, profile, level);
  }
  if (strncmp(codec_string, "vp09.", 5) == 0) {
    *codec = kSbMediaVideoCodecVp9;
    return ParseVp09Info(codec_string, profile, level, bit_depth, primary_id,
                         transfer_id, matrix_id);
  }
  if (strncmp(codec_string, "vp8", 3) == 0) {
    *codec = kSbMediaVideoCodecVp8;
    return true;
  }
  if (strncmp(codec_string, "vp9", 3) == 0) {
    *codec = kSbMediaVideoCodecVp9;
    return ParseVp9Info(codec_string, profile, bit_depth);
  }

  return false;
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
  os << "mime: " << (sample_info.mime ? sample_info.mime : "<null>")
     << ", max video capabilities: "
     << (sample_info.max_video_capabilities ? sample_info.max_video_capabilities
                                            : "<null>")
     << ", ";

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
  os << "mime: " << (sample_info.mime ? sample_info.mime : "<null>");
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
