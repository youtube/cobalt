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
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {

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
  SB_DCHECK_EQ(param.name, "bitrate");
  *bitrate = param.int_value;
}

}  // namespace

// static
SB_ONCE_INITIALIZE_FUNCTION(MimeSupportabilityCache,
                            MimeSupportabilityCache::GetInstance)

MimeSupportabilityCache::MimeSupportabilityResult
MimeSupportabilityCache::GetMimeSupportability(const char* mime) {
  SB_DCHECK(mime);

  // Strip the bitrate from mime string and check it separately.
  std::string mime_without_bitrate;
  int bitrate;
  StripAndParseBitrate(mime, &mime_without_bitrate, &bitrate);

  if (bitrate < 0) {
    // The mime string contains an invalid bitrate attribute. In that case, we
    // return an empty mime_info with kSupportabilityNotSupported.
    return {kSupportabilityNotSupported, std::nullopt};
  }

  std::lock_guard scoped_lock(mutex_);
  Entry* entry = GetEntry_Locked(mime_without_bitrate);

  if (!entry) {
    // Failed to parse mime string.
    return {kSupportabilityNotSupported, std::nullopt};
  }

  // Return cached ParsedMimeInfo with real bitrate.
  ParsedMimeInfo mime_info = entry->mime_info.WithBitrate(bitrate);

  Supportability supportability =
      is_enabled_ ? IsBitrateSupported_Locked(*entry, bitrate)
                  : kSupportabilityUnknown;

  return {supportability, std::move(mime_info)};
}

void MimeSupportabilityCache::CacheMimeSupportability(
    const char* mime,
    Supportability supportability) {
  SB_DCHECK(mime);
  SB_DCHECK_NE(supportability, kSupportabilityUnknown);

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

  std::lock_guard scoped_lock(mutex_);
  Entry* entry = GetEntry_Locked(mime_without_bitrate);

  if (entry) {
    UpdateBitrateSupportability_Locked(entry, bitrate, supportability);
  }
}

void MimeSupportabilityCache::ClearCachedMimeSupportabilities() {
  std::lock_guard scoped_lock(mutex_);
  for (auto& iter : entries_) {
    iter.second.max_supported_bitrate = -1;
    iter.second.min_unsupported_bitrate = INT_MAX;
  }
}

void MimeSupportabilityCache::DumpCache() {
  std::lock_guard scoped_lock(mutex_);
  std::stringstream ss;
  ss << "\n========Dumping MimeSupportabilityCache========";
  for (const auto& entry_iter : entries_) {
    ss << "\nMime: " << entry_iter.first;
    ss << "\n  ParsedMimeInfo: " << entry_iter.second.mime_info;
    ss << "\n  MaxSupportedBitrate: "
       << entry_iter.second.max_supported_bitrate;
    ss << "\n  MinUnsupportedBitrate: "
       << entry_iter.second.min_unsupported_bitrate;
  }
  ss << "\n========End of Dumping========";

  SB_DLOG(INFO) << ss.str();
}

MimeSupportabilityCache::Entry* MimeSupportabilityCache::GetEntry_Locked(
    const std::string& mime_string) {
  auto entry_iter = entries_.find(mime_string);
  if (entry_iter != entries_.end()) {
    return &entry_iter->second;
  }

  // We can't find anything from the cache. Parse mime string and cache
  // parsed ParsedMimeInfo.
  auto mime_info = ParsedMimeInfo::Create(mime_string);
  if (!mime_info) {
    return nullptr;
  }

  auto insert_result =
      entries_.emplace(mime_string, Entry(std::move(*mime_info)));

  // Keep cached items not exceeding max size.
  fifo_queue_.push(insert_result.first);
  while (fifo_queue_.size() > static_cast<size_t>(max_size_)) {
    entries_.erase(fifo_queue_.front());
    fifo_queue_.pop();
  }
  SB_DCHECK_EQ(entries_.size(), fifo_queue_.size());

  return &insert_result.first->second;
}

Supportability MimeSupportabilityCache::IsBitrateSupported_Locked(
    const Entry& entry,
    int bitrate) const {
  SB_DCHECK_GE(bitrate, 0);

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
  SB_DCHECK_GE(bitrate, 0);
  SB_DCHECK_NE(supportability, kSupportabilityUnknown);

  if (supportability == kSupportabilitySupported) {
    SB_DCHECK_LT(bitrate, entry->min_unsupported_bitrate);
    if (bitrate > entry->max_supported_bitrate) {
      entry->max_supported_bitrate = bitrate;
    }
  } else if (supportability == kSupportabilityNotSupported) {
    SB_DCHECK_GT(bitrate, entry->max_supported_bitrate);
    if (bitrate < entry->min_unsupported_bitrate) {
      entry->min_unsupported_bitrate = bitrate;
    }
  }
}

}  // namespace starboard
