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

#include "starboard/character.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/codec_util.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/string.h"

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
#if SB_HAS(AC3_AUDIO)
    case kSbMediaAudioCodecAc3:
      return mime_type.subtype() == "mp4";
    case kSbMediaAudioCodecEac3:
      return mime_type.subtype() == "mp4";
#endif  // SB_HAS(AC3_AUDIO)
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
    SB_LOG_IF(WARNING, transfer_id != kSbMediaTransferIdUnspecified)
        << "transfer_id " << transfer_id << " set by the codec string \""
        << codec << "\" will be overwritten by the eotf attribute " << eotf;
    transfer_id = GetTransferIdFromString(eotf);
    // If the eotf is not known, reject immediately - without checking with
    // the platform.
    if (transfer_id == kSbMediaTransferIdUnknown) {
      return false;
    }
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
#if SB_API_VERSION < SB_HAS_AV1_VERSION
    case kSbMediaVideoCodecVp10:
#else   // SB_API_VERSION < SB_HAS_AV1_VERSION
    case kSbMediaVideoCodecAv1:
#endif  // SB_API_VERSION < SB_HAS_AV1_VERSION
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
      primary_id != kSbMediaPrimaryIdUnspecified) {
    return false;
  }

  if (transfer_id != kSbMediaTransferIdBt709 &&
      transfer_id != kSbMediaTransferIdUnspecified) {
    return false;
  }

  if (matrix_id != kSbMediaMatrixIdBt709 &&
      matrix_id != kSbMediaMatrixIdUnspecified) {
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

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
