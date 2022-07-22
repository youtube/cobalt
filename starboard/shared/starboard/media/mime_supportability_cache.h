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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MIME_SUPPORTABILITY_CACHE_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MIME_SUPPORTABILITY_CACHE_H_

#include <atomic>
#include <queue>
#include <string>
#include <unordered_map>

#include "starboard/common/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/parsed_mime_info.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

typedef enum Supportability {
  kSupportabilityUnknown,
  kSupportabilitySupported,
  kSupportabilityNotSupported,
} Supportability;

// MimeSupportabilityCache caches the supportabilities of raw mime strings.
// To increase cache hit rate, it strips bitrate from the raw mime string, and
// stores a supported bitrate range for mime strings with same other attributes.
//
// Note: MimeSupportabilityCache leverage the assumption that if the
// platform can support a codec with bitrate of n, the codec should also support
// any bitrate less than n. If that assumption is not true, please do NOT enable
// MimeSupportabilityCache.
//
// Note: anytime the platform codec capabilities have changed, please call
// MimeSupportabilityCache::ClearCachedMimeSupportabilities() to clear the
// outdated results.
//
// TODO: add unit tests for MimeSupportabilityCache.
class MimeSupportabilityCache {
 public:
  static MimeSupportabilityCache* GetInstance();

  // When cache is not enabled, GetMimeSupportability() will return cached
  // ParsedMimeInfo with kSupportabilityUnknown, and CacheMimeSupportability()
  // will do nothing.
  bool IsEnabled() const { return is_enabled_; }
  void SetCacheEnabled(bool enabled) { is_enabled_ = enabled; }

  // Set the max number of the cached ParsedMimeInfos and its supportabilities.
  void SetCacheMaxSize(size_t size) { max_size_ = size; }

  // Get cached ParsedMimeInfo and mime supportability. If there's no cached
  // ParsedMimeInfo, it will parse the mime string and cache the result.
  // If we cannot get a valid ParsedMimeInfo from |mime|,
  // GetMimeSupportability() will return kSupportabilityNotSupported with an
  // invalid ParsedMimeInfo. Ideally, we should decouple mime parsing and
  // supportability cache, but considering that the cache is only for internal
  // use, to avoid repeated lookups, we do parsing in this function for now.
  // Note that |mime| and |mime_info| cannot be null.
  Supportability GetMimeSupportability(const char* mime,
                                       ParsedMimeInfo* mime_info);

  // Update cached supportability of the mime string.
  // Note that if |supportability| is kSupportabilityUnknown or we cannot
  // get a valid ParsedMimeInfo from |mime|, CacheMimeSupportability()
  // will not cache the supportability.
  void CacheMimeSupportability(const char* mime, Supportability supportability);

  // Clear all cached supportabilities. But it will not remove cached
  // ParsedMimeInfos, as for the same mime string, the parsed results should be
  // always the same.
  void ClearCachedMimeSupportabilities();

  void DumpCache();

 private:
  const int kDefaultCacheMaxSize = 2000;

  struct Entry {
    ParsedMimeInfo mime_info;
    int max_supported_bitrate = -1;
    int min_unsupported_bitrate = INT_MAX;

    explicit Entry(const std::string& mime) : mime_info(mime) {}
  };

  // Class can only be instanced via the singleton
  MimeSupportabilityCache() {}
  ~MimeSupportabilityCache() {}

  MimeSupportabilityCache(const MimeSupportabilityCache&) = delete;
  MimeSupportabilityCache& operator=(const MimeSupportabilityCache&) = delete;

  Entry& GetEntry_Locked(const std::string& mime_string);
  Supportability IsBitrateSupported_Locked(const Entry& entry,
                                           int bitrate) const;
  void UpdateBitrateSupportability_Locked(Entry* entry,
                                          int bitrate,
                                          Supportability supportability);

  typedef std::unordered_map<std::string, Entry> Entries;

  Mutex mutex_;
  Entries entries_;
  std::queue<Entries::iterator> fifo_queue_;
  std::atomic_int max_size_{kDefaultCacheMaxSize};
  std::atomic_bool is_enabled_{false};
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MIME_SUPPORTABILITY_CACHE_H_
