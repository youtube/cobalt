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

  if (strlen(key_system) != 0) {
    if (!SbMediaIsSupported(kSbMediaVideoCodecNone, audio_codec, key_system)) {
      return false;
    }
  }

  int channels = mime_type.GetParamIntValue("channels", kDefaultAudioChannels);
  if (!IsAudioOutputSupported(kSbMediaAudioCodingTypePcm, channels)) {
    return false;
  }

  int bitrate = mime_type.GetParamIntValue("bitrate", kDefaultBitRate);

  if (!SbMediaIsAudioSupported(audio_codec,
#if SB_API_VERSION >= 12
                               mime_type.raw_content_type().c_str(),
#endif  // SB_API_VERSION >= 12
                               bitrate)) {
    return false;
  }

  switch (audio_codec) {
    case kSbMediaAudioCodecNone:
      SB_NOTREACHED();
      return false;
    case kSbMediaAudioCodecAac:
      return mime_type.subtype() == "mp4";
#if SB_API_VERSION >= 12 || SB_HAS(AC3_AUDIO)
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
#endif  // SB_API_VERSION >= 12 ||
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

  if (strlen(key_system) != 0) {
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

  if (width < 0 || height < 0 || fps < 0 || bitrate < 0) {
    return false;
  }

#if SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
  if (!SbMediaIsVideoSupported(video_codec,
#if SB_API_VERSION >= 12
                               mime_type.raw_content_type().c_str(),
#endif  // SB_API_VERSION >= 12
                               profile, level, bit_depth, primary_id,
                               transfer_id, matrix_id, width, height, bitrate,
                               fps, decode_to_texture_required)) {
    return false;
  }
#else   //  SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
  if (!SbMediaIsVideoSupported(video_codec, width, height, bitrate, fps,
                               decode_to_texture_required)) {
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
    case kSbMediaVideoCodecAv1:
      return mime_type.subtype() == "mp4";
    case kSbMediaVideoCodecVp8:
      return mime_type.subtype() == "webm";
    case kSbMediaVideoCodecVp9:
      return mime_type.subtype() == "mp4" || mime_type.subtype() == "webm";
  }

  SB_NOTREACHED();
  return false;
}

}  // namespace

AudioSampleInfo::AudioSampleInfo() {
  memset(this, 0, sizeof(SbMediaAudioSampleInfo));
  codec = kSbMediaAudioCodecNone;
}

AudioSampleInfo::AudioSampleInfo(const SbMediaAudioSampleInfo& that) {
  *this = that;
}

AudioSampleInfo& AudioSampleInfo::operator=(
    const SbMediaAudioSampleInfo& that) {
  *static_cast<SbMediaAudioSampleInfo*>(this) = that;
  if (audio_specific_config_size > 0) {
    audio_specific_config_storage.resize(audio_specific_config_size);
    memcpy(audio_specific_config_storage.data(), audio_specific_config,
                 audio_specific_config_size);
    audio_specific_config = audio_specific_config_storage.data();
  }
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  if (codec == kSbMediaAudioCodecNone) {
    mime_storage.clear();
  } else {
    SB_DCHECK(that.mime);
    mime_storage = that.mime;
  }
  mime = mime_storage.c_str();
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  return *this;
}

VideoSampleInfo::VideoSampleInfo() {
  memset(this, 0, sizeof(SbMediaAudioSampleInfo));
  codec = kSbMediaVideoCodecNone;
}

VideoSampleInfo::VideoSampleInfo(const SbMediaVideoSampleInfo& that) {
  *this = that;
}

VideoSampleInfo& VideoSampleInfo::operator=(
    const SbMediaVideoSampleInfo& that) {
  *static_cast<SbMediaVideoSampleInfo*>(this) = that;
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  if (codec == kSbMediaVideoCodecNone) {
    mime_storage.clear();
    max_video_capabilities_storage.clear();
  } else {
    SB_DCHECK(that.mime);
    mime_storage = that.mime;
    max_video_capabilities_storage = that.max_video_capabilities;
  }
  mime = mime_storage.c_str();
  max_video_capabilities = max_video_capabilities_storage.c_str();
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  return *this;
}

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
  if (strlen(key_system) != 0) {
    if (!SbMediaIsSupported(kSbMediaVideoCodecNone, kSbMediaAudioCodecNone,
                            key_system)) {
      return kSbMediaSupportTypeNotSupported;
    }
  }

  bool decode_to_texture_required = false;
  std::string decode_to_texture_value =
      mime_type.GetParamStringValue("decode-to-texture", "false");
  if (decode_to_texture_value == "true") {
    decode_to_texture_required = true;
  } else if (decode_to_texture_value != "false") {
    // If an invalid value (e.g. not "true" or "false") is passed in for
    // decode-to-texture, trivially reject.
    return kSbMediaSupportTypeNotSupported;
  }

  if (codecs.size() == 0) {
    // This happens when the H5 player is either querying for progressive
    // playback support, or probing for generic mp4 support without specific
    // codecs.  We only support "audio/mp4" and "video/mp4" for these cases.
    if ((mime_type.type() == "audio" || mime_type.type() == "video") &&
        mime_type.subtype() == "mp4") {
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

std::string GetStringRepresentation(const uint8_t* data, const int size) {
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
                                   const int size,
                                   const int bytes_per_line) {
  std::string result;

  for (int i = 0; i < size; i += bytes_per_line) {
    if (i + bytes_per_line <= size) {
      result += HexEncode(data + i, bytes_per_line);
      result += " | ";
      result += GetStringRepresentation(data + i, bytes_per_line);
      result += '\n';
    } else {
      int bytes_left = size - i;
      result += HexEncode(data + i, bytes_left);
      result += std::string((bytes_per_line - bytes_left) * 2, ' ');
      result += " | ";
      result += GetStringRepresentation(data + i, bytes_left);
      result += std::string(bytes_per_line - bytes_left, ' ');
      result += '\n';
    }
  }

  return result;
}

bool IsAudioSampleInfoSubstantiallyDifferent(
    const SbMediaAudioSampleInfo& left,
    const SbMediaAudioSampleInfo& right) {
  return left.codec != right.codec ||
         left.samples_per_second != right.samples_per_second ||
         left.number_of_channels != right.number_of_channels ||
         left.audio_specific_config_size != right.audio_specific_config_size ||
         memcmp(left.audio_specific_config,
                right.audio_specific_config,
                left.audio_specific_config_size) != 0;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

bool operator==(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return memcmp(&metadata_1, &metadata_2,
                sizeof(SbMediaColorMetadata)) == 0;
}

bool operator==(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
  if (sample_info_1.codec != sample_info_2.codec) {
    return false;
  }
  if (sample_info_1.codec == kSbMediaVideoCodecNone) {
    return true;
  }

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  if (strcmp(sample_info_1.mime, sample_info_2.mime) != 0) {
    return false;
  }
  if (strcmp(sample_info_1.max_video_capabilities,
                         sample_info_2.max_video_capabilities) != 0) {
    return false;
  }
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  if (sample_info_1.is_key_frame != sample_info_2.is_key_frame) {
    return false;
  }
  if (sample_info_1.frame_width != sample_info_2.frame_width) {
    return false;
  }
  if (sample_info_1.frame_height != sample_info_2.frame_height) {
    return false;
  }
  return sample_info_1.color_metadata == sample_info_2.color_metadata;
}

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return !(metadata_1 == metadata_2);
}

bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
  return !(sample_info_1 == sample_info_2);
}
