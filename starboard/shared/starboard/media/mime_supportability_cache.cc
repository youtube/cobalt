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

#include "starboard/shared/starboard/media/mime_supportability_cache.h"

#include <cstring>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/mutex.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/once.h"
#include "starboard/shared/starboard/media/mime_type.h"

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

// Note that if bitrate parsing failed, |bitrate| will be set to -1.
void StripAndParseBitrate(const char* mime,
                          std::string* mime_without_bitrate,
                          int* bitrate) {
  SB_DCHECK(mime_without_bitrate);
  SB_DCHECK(bitrate);

  std::string bitrate_string;
  *mime_without_bitrate =
      RemoveAttributeFromMime(mime, "bitrate", &bitrate_string);

  if (bitrate_string.empty()) {
    *bitrate = 0;
    return;
  }

  MimeType::Param param;
  if (!MimeType::ParseParamString(bitrate_string, &param) ||
      param.type != MimeType::kParamTypeInteger) {
    *bitrate = -1;
    return;
  }
  SB_DCHECK(param.name == "bitrate");
  *bitrate = param.int_value;
}

}  // namespace

// static
SB_ONCE_INITIALIZE_FUNCTION(MimeSupportabilityCache,
                            MimeSupportabilityCache::GetInstance);

Supportability MimeSupportabilityCache::GetMimeSupportability(
    const char* mime,
    ParsedMimeInfo* mime_info) {
  SB_DCHECK(mime);
  SB_DCHECK(mime_info);

  // Strip the bitrate from mime string and check it separately.
  std::string mime_without_bitrate;
  int bitrate;
  StripAndParseBitrate(mime, &mime_without_bitrate, &bitrate);

  if (bitrate < 0) {
    // The mime string contains an invalid bitrate attribute. In that case, we
    // return an invalid ParsedMimeInfo with kSbMediaSupportTypeNotSupported.
    *mime_info = ParsedMimeInfo(mime);
    return kSupportabilityNotSupported;
  }

  ScopedLock scoped_lock(mutex_);
  Entry& entry = GetEntry_Locked(mime_without_bitrate);

  // Return cached ParsedMimeInfo with real bitrate.
  *mime_info = entry.mime_info;
  mime_info->SetBitrate(bitrate);

  if (!mime_info->is_valid()) {
    // Return kSupportabilityNotSupported if we can't get a valid
    // ParsedMimeInfo.
    return kSupportabilityNotSupported;
  }

  return is_enabled_ ? IsBitrateSupported_Locked(entry, bitrate)
                     : kSupportabilityUnknown;
}

void MimeSupportabilityCache::CacheMimeSupportability(
    const char* mime,
    Supportability supportability) {
  SB_DCHECK(mime);
  SB_DCHECK(supportability != kSupportabilityUnknown);

  if (!is_enabled_) {
    return;
  }

  // Strip bitrate as what we do in GetMimeSupportability().
  std::string mime_without_bitrate;
  int bitrate;
  StripAndParseBitrate(mime, &mime_without_bitrate, &bitrate);

  if (bitrate < 0) {
    // The mime string contains an invalid bitrate attribute.
    return;
  }

  ScopedLock scoped_lock(mutex_);
  Entry& entry = GetEntry_Locked(mime_without_bitrate);

  if (entry.mime_info.is_valid()) {
    UpdateBitrateSupportability_Locked(&entry, bitrate, supportability);
  }
}

void MimeSupportabilityCache::ClearCachedMimeSupportabilities() {
  ScopedLock scoped_lock(mutex_);
  for (auto& iter : entries_) {
    iter.second.max_supported_bitrate = -1;
    iter.second.min_unsupported_bitrate = INT_MAX;
  }
}

void MimeSupportabilityCache::DumpCache() {
  ScopedLock scoped_lock(mutex_);
  std::stringstream ss;
  ss << "\n========Dumping MimeSupportabilityCache========";
  for (const auto& entry_iter : entries_) {
    const ParsedMimeInfo& mime_info = entry_iter.second.mime_info;
    ss << "\nMime: " << entry_iter.first;
    ss << "\n  ParsedMimeInfo:";
    ss << "\n    MimeType : " << mime_info.mime_type().ToString();
    if (mime_info.is_valid()) {
      if (mime_info.has_audio_info()) {
        const ParsedMimeInfo::AudioCodecInfo& audio_info =
            mime_info.audio_info();
        ss << "\n    Audio Codec : "
           << GetMediaAudioCodecName(audio_info.codec);
        ss << "\n    Channels : " << audio_info.channels;
      }
      if (mime_info.has_video_info()) {
        const ParsedMimeInfo::VideoCodecInfo& video_info =
            mime_info.video_info();
        ss << "\n    Video Codec : "
           << GetMediaVideoCodecName(video_info.codec);
        ss << "\n    Profile : " << video_info.profile;
        ss << "\n    Level : " << video_info.level;
        ss << "\n    BitDepth : " << video_info.bit_depth;
        ss << "\n    PrimaryId : "
           << GetMediaPrimaryIdName(video_info.primary_id);
        ss << "\n    TransferId : "
           << GetMediaTransferIdName(video_info.transfer_id);
        ss << "\n    MatrixId : " << GetMediaMatrixIdName(video_info.matrix_id);
        ss << "\n    Width : " << video_info.frame_width;
        ss << "\n    Height : " << video_info.frame_height;
        ss << "\n    Fps : " << video_info.fps;
        ss << "\n    DecodeToTexture : "
           << (video_info.decode_to_texture_required ? "true" : "false");
      }
    } else {
      ss << "\n    Mime info is not valid";
    }

    ss << "\n  MaxSupportedBitrate: "
       << entry_iter.second.max_supported_bitrate;
    ss << "\n  MinUnsupportedBitrate: "
       << entry_iter.second.min_unsupported_bitrate;
  }
  ss << "\n========End of Dumping========";

  SB_DLOG(INFO) << ss.str();
}

MimeSupportabilityCache::Entry& MimeSupportabilityCache::GetEntry_Locked(
    const std::string& mime_string) {
  auto entry_iter = entries_.find(mime_string);
  if (entry_iter != entries_.end()) {
    return entry_iter->second;
  }

  // We can't find anything from the cache. Parse mime string and cache
  // parsed ParsedMimeInfo.
  auto insert_result = entries_.insert({mime_string, Entry(mime_string)});

  // Keep cached items not exceeding max size.
  fifo_queue_.push(insert_result.first);
  while (fifo_queue_.size() > max_size_) {
    entries_.erase(fifo_queue_.front());
    fifo_queue_.pop();
  }
  SB_DCHECK(entries_.size() == fifo_queue_.size());

  return insert_result.first->second;
}

Supportability MimeSupportabilityCache::IsBitrateSupported_Locked(
    const Entry& entry,
    int bitrate) const {
  SB_DCHECK(bitrate >= 0);

  if (bitrate <= entry.max_supported_bitrate) {
    return kSupportabilitySupported;
  }
  if (bitrate >= entry.min_unsupported_bitrate) {
    return kSupportabilityNotSupported;
  }
  return kSupportabilityUnknown;
}

void MimeSupportabilityCache::UpdateBitrateSupportability_Locked(
    Entry* entry,
    int bitrate,
    Supportability supportability) {
  SB_DCHECK(entry);
  SB_DCHECK(bitrate >= 0);
  SB_DCHECK(supportability != kSupportabilityUnknown);

  if (supportability == kSupportabilitySupported) {
    SB_DCHECK(bitrate < entry->min_unsupported_bitrate);
    if (bitrate > entry->max_supported_bitrate) {
      entry->max_supported_bitrate = bitrate;
    }
  } else if (supportability == kSupportabilityNotSupported) {
    SB_DCHECK(bitrate > entry->max_supported_bitrate);
    if (bitrate < entry->min_unsupported_bitrate) {
      entry->min_unsupported_bitrate = bitrate;
    }
  }
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
