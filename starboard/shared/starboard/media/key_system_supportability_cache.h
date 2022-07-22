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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_KEY_SYSTEM_SUPPORTABILITY_CACHE_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_KEY_SYSTEM_SUPPORTABILITY_CACHE_H_

#include <atomic>

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// KeySystemSupportabilityCache caches the supportabilities of the combinations
// of codec and key system.
//
// Note: anytime the platform key system capabilities have changed, please
// call KeySystemSupportabilityCache::ClearCache() to clear the outdated
// results.
//
// TODO: add unit tests for KeySystemSupportabilityCache.
class KeySystemSupportabilityCache {
 public:
  static KeySystemSupportabilityCache* GetInstance();

  // When cache is not enabled, GetKeySystemSupportability() will always return
  // kSupportabilityUnknown, and CacheKeySystemSupportability() will do nothing.
  bool IsEnabled() const { return is_enabled_; }
  void SetCacheEnabled(bool enabled) { is_enabled_ = enabled; }

  // Get & cache key system supportability.
  Supportability GetKeySystemSupportability(SbMediaAudioCodec codec,
                                            const char* key_system);
  Supportability GetKeySystemSupportability(SbMediaVideoCodec codec,
                                            const char* key_system);
  void CacheKeySystemSupportability(SbMediaAudioCodec codec,
                                    const char* key_system,
                                    Supportability supportability);
  void CacheKeySystemSupportability(SbMediaVideoCodec codec,
                                    const char* key_system,
                                    Supportability supportability);

  // Clear all cached supportabilities.
  void ClearCache();

 private:
  // Class can only be instanced via the singleton
  KeySystemSupportabilityCache() {}
  ~KeySystemSupportabilityCache() {}

  KeySystemSupportabilityCache(const KeySystemSupportabilityCache&) = delete;
  KeySystemSupportabilityCache& operator=(const KeySystemSupportabilityCache&) =
      delete;

  std::atomic_bool is_enabled_{false};
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_KEY_SYSTEM_SUPPORTABILITY_CACHE_H_
