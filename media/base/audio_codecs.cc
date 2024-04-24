// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_codecs.h"

#include <ostream>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace media {

// These names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string GetCodecName(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kUnknown:
      return "unknown";
    case AudioCodec::kAAC:
      return "aac";
    case AudioCodec::kMP3:
      return "mp3";
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
      return "pcm";
    case AudioCodec::kVorbis:
      return "vorbis";
    case AudioCodec::kFLAC:
      return "flac";
    case AudioCodec::kAMR_NB:
      return "amr_nb";
    case AudioCodec::kAMR_WB:
      return "amr_wb";
    case AudioCodec::kPCM_MULAW:
      return "pcm_mulaw";
    case AudioCodec::kGSM_MS:
      return "gsm_ms";
    case AudioCodec::kOpus:
      return "opus";
    case AudioCodec::kIAMF:
      return "iamf";
    case AudioCodec::kPCM_ALAW:
      return "pcm_alaw";
    case AudioCodec::kEAC3:
      return "eac3";
    case AudioCodec::kALAC:
      return "alac";
    case AudioCodec::kAC3:
      return "ac3";
    case AudioCodec::kMpegHAudio:
      return "mpeg-h-audio";
    case AudioCodec::kDTS:
      return "dts";
    case AudioCodec::kDTSXP2:
      return "dtsx-p2";
    case AudioCodec::kDTSE:
      return "dtse";
  }
}

std::string GetProfileName(AudioCodecProfile profile) {
  switch (profile) {
    case AudioCodecProfile::kUnknown:
      return "unknown";
    case AudioCodecProfile::kXHE_AAC:
      return "xhe-aac";
    case AudioCodecProfile::kIAMF_SIMPLE:
      return "iamf-simple";
    case AudioCodecProfile::kIAMF_BASE:
      return "iamf-base";
  }
}

AudioCodec StringToAudioCodec(const std::string& codec_id) {
  if (codec_id == "aac")
    return AudioCodec::kAAC;
  if (codec_id == "ac-3" || codec_id == "mp4a.A5" || codec_id == "mp4a.a5")
    return AudioCodec::kAC3;
  if (codec_id == "ec-3" || codec_id == "mp4a.A6" || codec_id == "mp4a.a6")
    return AudioCodec::kEAC3;
  if (codec_id == "dtsc" || codec_id == "mp4a.A9" || codec_id == "mp4a.a9") {
    return AudioCodec::kDTS;
  }
  if (codec_id == "dtse" || codec_id == "mp4a.AC" || codec_id == "mp4a.ac") {
    return AudioCodec::kDTSE;
  }
  if (codec_id == "dtsx" || codec_id == "mp4a.B2" || codec_id == "mp4a.b2") {
    return AudioCodec::kDTSXP2;
  }
  if (codec_id == "mp3" || codec_id == "mp4a.69" || codec_id == "mp4a.6B")
    return AudioCodec::kMP3;
  if (codec_id == "alac")
    return AudioCodec::kALAC;
  if (codec_id == "flac")
    return AudioCodec::kFLAC;
  if (base::StartsWith(codec_id, "mhm1.", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec_id, "mha1.", base::CompareCase::SENSITIVE)) {
    return AudioCodec::kMpegHAudio;
  }
  if (codec_id == "opus")
    return AudioCodec::kOpus;
  if (codec_id == "vorbis")
    return AudioCodec::kVorbis;
  if (base::StartsWith(codec_id, "mp4a.40.", base::CompareCase::SENSITIVE))
    return AudioCodec::kAAC;
  if (codec_id == "iamf" ||
      base::StartsWith(codec_id, "iamf.", base::CompareCase::SENSITIVE)) {
    return AudioCodec::kIAMF;
  }
  return AudioCodec::kUnknown;
}

std::ostream& operator<<(std::ostream& os, const AudioCodec& codec) {
  return os << GetCodecName(codec);
}

#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
bool ParseIamfCodecId(std::string_view codec_id,
                      uint8_t* primary_profilec,
                      uint8_t* additional_profilec) {
  // Reference: Immersive Audio Model and Formats;
  //            v1.0.0
  //            6.3. Codecs Parameter String
  // (https://aomediacodec.github.io/iamf/v1.0.0.html#codecsparameter)
  if (!(std::string(codec_id).find("iamf") == 0)) {
    return false;
  }

  // For test purposes only, just "iamf" is acceptable.
  if (codec_id == "iamf") {
    return true;
  }

  constexpr int kMaxIamfCodecIdLength =
      4     // FOURCC string "iamf".
      + 1   // delimiting period.
      + 3   // primary_profile as 3 digit string.
      + 1   // delimiting period.
      + 3   // additional_profile as 3 digit string.
      + 1   // delimiting period.
      + 9;  // The remaining string is one of
            // "opus", "mp4a.40.2", "flac", "ipcm".

  if (codec_id.size() > kMaxIamfCodecIdLength) {
    DVLOG(4) << __func__ << ": Codec id is too long (" << codec_id << ")";
    return false;
  }

  std::vector<std::string> elem = base::SplitString(
      std::string(codec_id), ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (elem.size() < 4) {
    DVLOG(4) << __func__ << ": invalid IAMF codec id:" << codec_id;
    return false;
  }

  DCHECK_EQ(elem[0], "iamf");

  if (StringToAudioCodec(elem[0]) != AudioCodec::kIAMF) {
    DVLOG(4) << __func__ << ": invalid IAMF codec id:" << codec_id;
    return false;
  }

  // The primary profile string should be three digits, and should be between 0
  // and 255 inclusive.
  uint32_t primary_profile = 0;
  if (elem[1].size() != 3 || !base::StringToUint(elem[1], &primary_profile) ||
      primary_profile > 0xFF) {
    DVLOG(4) << __func__ << ": invalid IAMF primary profile: " << elem[1];
    return false;
  }

  // The additional profile string should be three digits, and should be between
  // 0 and 255 inclusive.
  uint32_t additional_profile = 0;
  if (elem[2].size() != 3 ||
      !base::StringToUint(elem[2], &additional_profile) ||
      additional_profile > 0xFF) {
    DVLOG(4) << __func__ << ": invalid IAMF additional profile: " << elem[2];
    return false;
  }

  // The codec string should be one of "opus", "mp4a", "flac", or "ipcm".
  std::string codec = base::ToLowerASCII(elem[3]);
  if (codec.size() != 4 || ((codec != "opus") && (codec != "mp4a") &&
                            (codec != "flac") && (codec != "ipcm"))) {
    DVLOG(4) << __func__ << ": invalid IAMF stream codec: " << elem[3];
    return false;
  }

  if (codec == "mp4a") {
    if (elem.size() != 6) {
      DVLOG(4) << __func__
               << ": incorrect mp4a codec string syntax:" << codec_id;
      return false;
    }

    // The fields following "mp4a" should be "40" and "2" to signal AAC-LC.
    uint32_t object_type_indication = 0;
    if (elem[4].size() != 2 ||
        !base::HexStringToUInt(elem[4], &object_type_indication) ||
        object_type_indication != 0x40) {
      DVLOG(4) << __func__
               << ": invalid mp4a Object Type Indication:" << codec_id;
      return false;
    }

    uint32_t audio_object_type = 0;
    if (elem[5].size() != 1 ||
        !base::HexStringToUInt(elem[5], &audio_object_type) ||
        audio_object_type != 0x02) {
      DVLOG(4) << __func__ << ": invalid mp4a Audio Object Type:" << codec_id;
      return false;
    }
  }

  if (primary_profilec) {
    *primary_profilec = primary_profile;
  }

  if (additional_profilec) {
    *additional_profilec = additional_profile;
  }

  return true;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
}  // namespace media
