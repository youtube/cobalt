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

#include <mutex>
#include <optional>
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

// The cache key and bitrate parsed from a mime string. See
// ParseCacheKeyAndBitrate().
struct CacheKeyAndBitrate {
  std::string key;
  int bitrate;
};

// Parses |mime| with MimeType, and returns the cache key, which is the
// normalized mime string with the "bitrate" parameter removed, and the
// bitrate (0 if absent). Removing the bitrate lets mime strings that only
// differ in bitrate share a cache entry. Returns std::nullopt if |mime| is not
// a valid mime string, or if its bitrate is not a non-negative integer.
std::optional<CacheKeyAndBitrate> ParseCacheKeyAndBitrate(const char* mime) {
  std::optional<MimeType> mime_type = MimeType::Create(mime);
  if (!mime_type || !mime_type->ValidateIntParameter(kMimeParamBitrate)) {
    return std::nullopt;
  }
  int bitrate = mime_type->GetParamIntValue(kMimeParamBitrate, 0);
  if (bitrate < 0) {
    return std::nullopt;
  }
  return CacheKeyAndBitrate{mime_type->ToStringWithout(kMimeParamBitrate),
                            bitrate};
}

}  // namespace

// static
SB_ONCE_INITIALIZE_FUNCTION(MimeSupportabilityCache,
                            MimeSupportabilityCache::GetInstance)

MimeSupportabilityCache::MimeSupportabilityResult
MimeSupportabilityCache::GetMimeSupportability(const char* mime) {
  SB_DCHECK(mime);

  // Look up the cache without the bitrate, and check the bitrate separately.
  std::optional<CacheKeyAndBitrate> key_and_bitrate =
      ParseCacheKeyAndBitrate(mime);
  if (!key_and_bitrate) {
    // The mime string or its bitrate attribute is invalid. In that case, we
    // return an empty mime_info with kSupportabilityNotSupported.
    return {kSupportabilityNotSupported, std::nullopt};
  }
  const int bitrate = key_and_bitrate->bitrate;

  std::lock_guard scoped_lock(mutex_);
  Entry* entry = GetEntry_Locked(key_and_bitrate->key);

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

  // Use the same cache key as GetMimeSupportability().
  std::optional<CacheKeyAndBitrate> key_and_bitrate =
      ParseCacheKeyAndBitrate(mime);
  if (!key_and_bitrate) {
    // The mime string or its bitrate attribute is invalid.
    return;
  }

  std::lock_guard scoped_lock(mutex_);
  Entry* entry = GetEntry_Locked(key_and_bitrate->key);

  if (entry) {
    UpdateBitrateSupportability_Locked(entry, key_and_bitrate->bitrate,
                                       supportability);
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
