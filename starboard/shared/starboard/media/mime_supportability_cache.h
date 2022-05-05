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
#include <string>

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

// TODO: add unit tests for MimeSupportabilityCache
class MimeSupportabilityCache {
 public:
  static MimeSupportabilityCache* GetInstance();

  // When cache is not enabled, GetMimeSupportability() will always return
  // kSupportabilityUnknown, and CacheMimeSupportability() will do nothing,
  // but GetMimeSupportability() will still return parsed ParsedMimeInfo.
  bool IsEnabled() const { return is_enabled_; }
  void SetCacheEnabled(bool enabled) { is_enabled_ = enabled; }

  void SetCacheMaxSize(size_t size);

  // Get cached mime supportability. The parsed mime information would be
  // returned via |mime_info| if it is not NULL.
  Supportability GetMimeSupportability(const std::string& mime,
                                       ParsedMimeInfo* mime_info);
  // Cache mime supportability. If there's no cached parsed mime info and
  // supportability for the mime, the function will parse the mime first and
  // then update its supportability.
  void CacheMimeSupportability(const std::string& mime,
                               Supportability supportability);

  // Clear all cached supportabilities. Note that it will not remove cached
  // parsed mime infos.
  void ClearCachedMimeSupportabilities();

 private:
  // Class can only be instanced via the singleton
  MimeSupportabilityCache() {}
  ~MimeSupportabilityCache() {}

  MimeSupportabilityCache(const MimeSupportabilityCache&) = delete;
  MimeSupportabilityCache& operator=(const MimeSupportabilityCache&) = delete;

  std::atomic_bool is_enabled_{false};
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MIME_SUPPORTABILITY_CACHE_H_
