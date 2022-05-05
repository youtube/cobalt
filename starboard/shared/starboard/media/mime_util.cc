// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/mime_util.h"

#include <cstring>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/media/bitrate_supportability_cache.h"
#include "starboard/shared/starboard/media/key_system_supportability_cache.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/shared/starboard/media/parsed_mime_info.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

// RemoveAttributeFromMime() will return a new mime string with the specified
// attribute removed. If |attribute_string| is not null, the removed attribute
// string will be returned via |attribute_string|. Following are some examples:
//   mime: "video/webm; codecs=\"vp9\"; bitrate=300000"
//   attribute_name: "bitrate"
//   return: "video/webm; codecs=\"vp9\""
//   attribute_string: "bitrate=300000"
//
//   mime: "video/webm; codecs=\"vp9\"; bitrate=300000; eotf=bt709"
//   attribute_name: "bitrate"
//   return: "video/webm; codecs=\"vp9\"; eotf=bt709"
//   attribute_string: "bitrate=300000"
//
//   mime: "bitrate=300000"
//   attribute_name: "bitrate"
//   return: ""
//   attribute_string: "bitrate=300000"
std::string RemoveAttributeFromMime(const char* mime,
                                    const char* attribute_name,
                                    std::string* attribute_string) {
  size_t name_length = strlen(attribute_name);
  if (name_length == 0) {
    return mime;
  }

  std::string mime_without_attribute;
  const char* start_pos = strstr(mime, attribute_name);
  while (start_pos) {
    if ((start_pos == mime || start_pos[-1] == ';' || isspace(start_pos[-1])) &&
        (start_pos[name_length] &&
         (start_pos[name_length] == '=' || isspace(start_pos[name_length])))) {
      break;
    }
    start_pos += name_length;
    start_pos = strstr(start_pos, attribute_name);
  }

  if (!start_pos) {
    // Target attribute is not found.
    return std::string(mime);
  }
  const char* end_pos = strstr(start_pos, ";");
  if (end_pos) {
    // There may be other attribute after target attribute.
    if (attribute_string) {
      // Returned |attribute_string| will not have a trailing ';'.
      attribute_string->assign(start_pos, end_pos - start_pos);
    }

    end_pos++;
    // Remove leading spaces.
    while (*end_pos && isspace(*end_pos)) {
      end_pos++;
    }
    if (*end_pos) {
      // Append the string after target attribute.
      mime_without_attribute = std::string(mime, start_pos - mime);
      mime_without_attribute.append(end_pos);
    } else {
      // Target attribute is the last one. Remove trailing spaces.
      size_t mime_length = start_pos - mime;
      while (mime_length > 0 && (isspace(mime[mime_length - 1]))) {
        mime_length--;
      }
      mime_without_attribute = std::string(mime, mime_length);
    }
  } else {
    // It can't find a trailing ';'. The target attribute must be the last one.
    size_t mime_length = start_pos - mime;
    // Remove trailing spaces.
    while (mime_length > 0 && (isspace(mime[mime_length - 1]))) {
      mime_length--;
    }
    // Remove the trailing ';'.
    if (mime_length > 0 && mime[mime_length - 1] == ';') {
      mime_length--;
    }
    mime_without_attribute = std::string(mime, mime_length);
    if (attribute_string) {
      *attribute_string = std::string(start_pos);
    }
  }
  return mime_without_attribute;
}

// Use SbMediaGetAudioConfiguration() to check if the platform can support
// |channels|.
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

bool IsSupportedKeySystem(SbMediaAudioCodec codec, const char* key_system) {
  SB_DCHECK(key_system);
  // KeySystemSupportabilityCache() should always return supported for empty
  // |key_system|, so here it should always be non empty.
  SB_DCHECK(strlen(key_system) > 0);

  return SbMediaIsSupported(kSbMediaVideoCodecNone, codec, key_system);
}

bool IsSupportedKeySystem(SbMediaVideoCodec codec, const char* key_system) {
  SB_DCHECK(key_system);
  // KeySystemSupportabilityCache() should always return supported for empty
  // |key_system|, so here it should always be non empty.
  SB_DCHECK(strlen(key_system) > 0);

  return SbMediaIsSupported(codec, kSbMediaAudioCodecNone, key_system);
}

bool IsSupportedAudioCodec(const ParsedMimeInfo& mime_info) {
  SB_DCHECK(mime_info.is_valid());
  SB_DCHECK(mime_info.mime_type().is_valid());
  SB_DCHECK(mime_info.has_audio_info());

  const MimeType& mime_type = mime_info.mime_type();
  const ParsedMimeInfo::AudioCodecInfo& audio_info = mime_info.audio_info();

  switch (audio_info.codec) {
    case kSbMediaAudioCodecNone:
      SB_NOTREACHED();
      return false;
    case kSbMediaAudioCodecAac:
    case kSbMediaAudioCodecAc3:
    case kSbMediaAudioCodecEac3:
      if (mime_type.subtype() != "mp4") {
        return false;
      }
      break;
    case kSbMediaAudioCodecOpus:
    case kSbMediaAudioCodecVorbis:
      if (mime_type.subtype() != "webm") {
        return false;
      }
      break;
#if SB_API_VERSION >= 14
    case kSbMediaAudioCodecMp3:
    case kSbMediaAudioCodecFlac:
    case kSbMediaAudioCodecPcm:
      return false;
#endif  // SB_API_VERSION >= 14
  }

  if (!IsAudioOutputSupported(kSbMediaAudioCodingTypePcm,
                              audio_info.channels)) {
    return false;
  }

  return SbMediaIsAudioSupported(audio_info.codec, &mime_type,
                                 audio_info.bitrate);
}

bool IsSupportedVideoCodec(const ParsedMimeInfo& mime_info) {
  SB_DCHECK(mime_info.is_valid());
  SB_DCHECK(mime_info.mime_type().is_valid());
  SB_DCHECK(mime_info.has_video_info());

  const MimeType& mime_type = mime_info.mime_type();
  const ParsedMimeInfo::VideoCodecInfo& video_info = mime_info.video_info();

  switch (video_info.codec) {
    case kSbMediaVideoCodecNone:
      SB_NOTREACHED();
      return false;
    case kSbMediaVideoCodecH264:
    case kSbMediaVideoCodecH265:
      if (mime_type.subtype() != "mp4") {
        return false;
      }
      break;
    case kSbMediaVideoCodecMpeg2:
    case kSbMediaVideoCodecTheora:
      return false;  // No associated container in YT.
    case kSbMediaVideoCodecVc1:
    case kSbMediaVideoCodecAv1:
      if (mime_type.subtype() != "mp4") {
        return false;
      }
      break;
    case kSbMediaVideoCodecVp8:
      if (mime_type.subtype() != "webm") {
        return false;
      }
      break;
    case kSbMediaVideoCodecVp9:
      if (mime_type.subtype() != "mp4" && mime_type.subtype() != "webm") {
        return false;
      }
      break;
  }

  std::string cryptoblockformat =
      mime_type.GetParamStringValue("cryptoblockformat", "");
  if (!cryptoblockformat.empty()) {
    if (mime_type.subtype() != "webm" || cryptoblockformat != "subsample") {
      return false;
    }
  }

  return SbMediaIsVideoSupported(
      video_info.codec, &mime_type, video_info.profile, video_info.level,
      video_info.bit_depth, video_info.primary_id, video_info.transfer_id,
      video_info.matrix_id, video_info.frame_width, video_info.frame_height,
      video_info.bitrate, video_info.fps,
      video_info.decode_to_texture_required);
}

bool ValidateAndParseBitrate(const std::string& bitrate_string, int* bitrate) {
  SB_DCHECK(!bitrate_string.empty());

  MimeType::Param param;
  if (!MimeType::ParseParamString(bitrate_string, &param)) {
    return false;
  }
  if (param.type != MimeType::kParamTypeInteger) {
    return false;
  }
  if (bitrate) {
    *bitrate = param.int_value;
  }
  return true;
}

}  // namespace

SbMediaSupportType CanPlayMimeAndKeySystem(const char* mime,
                                           const char* key_system) {
  SB_DCHECK(mime);
  SB_DCHECK(key_system);

  // Remove bitrate from mime string and read bitrate if presents.
  std::string bitrate_string;
  std::string mime_without_bitrate =
      RemoveAttributeFromMime(mime, "bitrate", &bitrate_string);
  int bitrate = 0;
  if (!bitrate_string.empty()) {
    if (!ValidateAndParseBitrate(bitrate_string, &bitrate)) {
      return kSbMediaSupportTypeNotSupported;
    }
  }

  if (bitrate < 0) {
    // Reject invalid bitrate.
    return kSbMediaSupportTypeNotSupported;
  }

  // Get cached parsed mime infos and supportability. If it is not found in the
  // cache, MimeSupportabilityCache would parse the mime string and return a
  // ParsedMimeInfo.
  ParsedMimeInfo mime_info;
  Supportability mime_supportability =
      MimeSupportabilityCache::GetInstance()->GetMimeSupportability(
          mime_without_bitrate, &mime_info);
  // Overwrite the bitrate.
  mime_info.SetBitrate(bitrate);

  if (mime_info.disable_cache()) {
    // Disable all caches if required.
    mime_supportability = kSupportabilityUnknown;
    MimeSupportabilityCache::GetInstance()->SetCacheEnabled(false);
    KeySystemSupportabilityCache::GetInstance()->SetCacheEnabled(false);
    BitrateSupportabilityCache::GetInstance()->SetCacheEnabled(false);
  }

  // Reject mime if cached result is not supported.
  if (mime_supportability == kSupportabilityNotSupported) {
    return kSbMediaSupportTypeNotSupported;
  }

  // Reject mime if parsed mime info is invalid.
  if (!mime_info.is_valid()) {
    return kSbMediaSupportTypeNotSupported;
  }

  const MimeType& mime_type = mime_info.mime_type();
  const std::vector<std::string>& codecs = mime_type.GetCodecs();

  // Quick check for mp4 format.
  if (codecs.size() == 0) {
    // This happens when the H5 player is either querying for progressive
    // playback support, or probing for generic mp4 support without specific
    // codecs.
    if (mime_type.subtype() == "mp4") {
      return kSbMediaSupportTypeMaybe;
    } else {
      return kSbMediaSupportTypeNotSupported;
    }
  }

  // Reject mime if it doesn't have any valid codec info.
  if (!mime_info.has_audio_info() && !mime_info.has_video_info()) {
    return kSbMediaSupportTypeNotSupported;
  }

  // Get cached key system supportability. Note that we check if audio or video
  // codec supports key system separately.
  if (mime_info.has_audio_info()) {
    Supportability key_system_supportability =
        KeySystemSupportabilityCache::GetInstance()->GetKeySystemSupportability(
            mime_info.audio_info().codec, key_system);
    if (key_system_supportability == kSupportabilityUnknown) {
      key_system_supportability =
          IsSupportedKeySystem(mime_info.audio_info().codec, key_system)
              ? kSupportabilitySupported
              : kSupportabilityNotSupported;
      KeySystemSupportabilityCache::GetInstance()->CacheKeySystemSupportability(
          mime_info.audio_info().codec, key_system, key_system_supportability);
    }
    // Reject mime if audio codec doesn't support the key system.
    if (key_system_supportability == kSupportabilityNotSupported) {
      return kSbMediaSupportTypeNotSupported;
    }
  }
  if (mime_info.has_video_info()) {
    Supportability key_system_supportability =
        KeySystemSupportabilityCache::GetInstance()->GetKeySystemSupportability(
            mime_info.video_info().codec, key_system);
    if (key_system_supportability == kSupportabilityUnknown) {
      key_system_supportability =
          IsSupportedKeySystem(mime_info.video_info().codec, key_system)
              ? kSupportabilitySupported
              : kSupportabilityNotSupported;
      KeySystemSupportabilityCache::GetInstance()->CacheKeySystemSupportability(
          mime_info.video_info().codec, key_system, key_system_supportability);
    }
    // Reject mime if video codec doesn't the key system.
    if (key_system_supportability == kSupportabilityNotSupported) {
      return kSbMediaSupportTypeNotSupported;
    }
  }

  // Get cached bitrate supportability.
  Supportability bitrate_supportability =
      BitrateSupportabilityCache::GetInstance()->GetBitrateSupportability(
          mime_info);

  // Reject mime if bitrate is not supported.
  if (bitrate_supportability == kSupportabilityNotSupported) {
    return kSbMediaSupportTypeNotSupported;
  }

  // Return supported if mime and bitrate are all supported.
  if (mime_supportability == kSupportabilitySupported &&
      bitrate_supportability == kSupportabilitySupported) {
    return kSbMediaSupportTypeProbably;
  }

  // At this point, either mime or bitrate supportability must be unknown.
  // Call platform functions to check if they are supported.
  SB_DCHECK(mime_supportability == kSupportabilityUnknown ||
            bitrate_supportability == kSupportabilityUnknown);
  if (mime_info.has_audio_info() && !IsSupportedAudioCodec(mime_info)) {
    mime_supportability = kSupportabilityNotSupported;
  } else if (mime_info.has_video_info() && !IsSupportedVideoCodec(mime_info)) {
    mime_supportability = kSupportabilityNotSupported;
  } else {
    mime_supportability = kSupportabilitySupported;
  }

  // Cache mime supportability when bitrate supportability is known.
  if (bitrate_supportability == kSupportabilitySupported) {
    MimeSupportabilityCache::GetInstance()->CacheMimeSupportability(
        mime_without_bitrate, mime_supportability);
  }

  SB_DCHECK(mime_supportability != kSupportabilityUnknown);
  return mime_supportability == kSupportabilitySupported
             ? kSbMediaSupportTypeProbably
             : kSbMediaSupportTypeNotSupported;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
