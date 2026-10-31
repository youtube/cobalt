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
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <string_view>
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

// Strips the "bitrate" attribute from |mime| into |mime_without_bitrate| and
// returns its parsed non-negative value (0 if absent), or std::nullopt if
// bitrate parsing failed.
std::optional<int> StripAndParseBitrate(const char* mime,
                                        std::string* mime_without_bitrate) {
  SB_CHECK(mime_without_bitrate);

  mime_without_bitrate->clear();
  std::optional<int> bitrate = 0;
  for (const char* pos = mime; *pos;) {
    pos += strspn(pos, " \t\n\v\f\r");
    size_t len = strcspn(pos, ";");
    std::string_view name(pos, strcspn(pos, " =;\t\n\v\f\r"));
    if (kMimeParamBitrate.EqualsCaseInsensitive(name)) {
      MimeType::Param param;
      if (!MimeType::ParseParamString(std::string(pos, len), &param) ||
          param.type != MimeType::kParamTypeInteger || param.int_value < 0) {
        return std::nullopt;
      }
      bitrate = param.int_value;
    } else if (len > 0) {
      if (!mime_without_bitrate->empty()) {
        *mime_without_bitrate += "; ";
      }
      mime_without_bitrate->append(pos, len);
    }
    pos += len + (pos[len] ? 1 : 0);
  }
  return bitrate;
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
  std::optional<int> bitrate =
      StripAndParseBitrate(mime, &mime_without_bitrate);

  if (!bitrate) {
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
  ParsedMimeInfo mime_info = entry->mime_info.WithBitrate(*bitrate);

  Supportability supportability =
      is_enabled_ ? IsBitrateSupported_Locked(*entry, *bitrate)
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
  std::optional<int> bitrate =
      StripAndParseBitrate(mime, &mime_without_bitrate);

  if (!bitrate) {
    // The mime string contains an invalid bitrate attribute.
    return;
  }

  std::lock_guard scoped_lock(mutex_);
  Entry* entry = GetEntry_Locked(mime_without_bitrate);

  if (entry) {
    UpdateBitrateSupportability_Locked(entry, *bitrate, supportability);
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
