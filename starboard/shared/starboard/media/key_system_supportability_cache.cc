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

#include "starboard/shared/starboard/media/key_system_supportability_cache.h"

#include <cstring>
#include <map>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/media.h"
#include "starboard/once.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

template <typename T>
class KeySystemSupportabilityContainer {
 public:
  Supportability GetKeySystemSupportability(T codec, const char* key_system) {
    SB_DCHECK(key_system);
    SB_DCHECK(strlen(key_system) > 0);

    ScopedLock scoped_lock(mutex_);
    auto map_iter = key_system_supportabilities_.find(codec);
    if (map_iter == key_system_supportabilities_.end()) {
      return kSupportabilityUnknown;
    }
    KeySystemToSupportabilityMap& map = map_iter->second;
    auto supportability_iter = map.find(std::string(key_system));
    if (supportability_iter == map.end()) {
      return kSupportabilityUnknown;
    }
    return supportability_iter->second;
  }

  void CacheKeySystemSupportability(T codec,
                                    const char* key_system,
                                    Supportability supportability) {
    SB_DCHECK(key_system);
    SB_DCHECK(strlen(key_system) > 0);
    SB_DCHECK(supportability != kSupportabilityUnknown);

    ScopedLock scoped_lock(mutex_);
    key_system_supportabilities_[codec][key_system] = supportability;
  }

  void ClearContainer() {
    ScopedLock scoped_lock(mutex_);
    key_system_supportabilities_.clear();
  }

 private:
  typedef std::map<std::string, Supportability> KeySystemToSupportabilityMap;

  Mutex mutex_;
  std::map<T, KeySystemToSupportabilityMap> key_system_supportabilities_;
};

template <typename T>
SB_ONCE_INITIALIZE_FUNCTION(KeySystemSupportabilityContainer<T>, GetContainer);

}  // namespace

// static
SB_ONCE_INITIALIZE_FUNCTION(KeySystemSupportabilityCache,
                            KeySystemSupportabilityCache::GetInstance);

Supportability KeySystemSupportabilityCache::GetKeySystemSupportability(
    SbMediaAudioCodec codec,
    const char* key_system) {
  SB_DCHECK(key_system);

  // Empty key system is always supported.
  if (strlen(key_system) == 0) {
    return kSupportabilitySupported;
  }

  if (!is_enabled_) {
    return kSupportabilityUnknown;
  }

  return GetContainer<SbMediaAudioCodec>()->GetKeySystemSupportability(
      codec, key_system);
}

Supportability KeySystemSupportabilityCache::GetKeySystemSupportability(
    SbMediaVideoCodec codec,
    const char* key_system) {
  SB_DCHECK(key_system);

  // Empty key system is always supported.
  if (strlen(key_system) == 0) {
    return kSupportabilitySupported;
  }

  if (!is_enabled_) {
    return kSupportabilityUnknown;
  }

  return GetContainer<SbMediaVideoCodec>()->GetKeySystemSupportability(
      codec, key_system);
}

void KeySystemSupportabilityCache::CacheKeySystemSupportability(
    SbMediaAudioCodec codec,
    const char* key_system,
    Supportability supportability) {
  SB_DCHECK(key_system);
  SB_DCHECK(supportability != kSupportabilityUnknown);

  if (!is_enabled_) {
    return;
  }

  if (strlen(key_system) == 0) {
    SB_LOG(WARNING) << "Rejected empty key system as it's always supported.";
  }

  GetContainer<SbMediaAudioCodec>()->CacheKeySystemSupportability(
      codec, key_system, supportability);
}

void KeySystemSupportabilityCache::CacheKeySystemSupportability(
    SbMediaVideoCodec codec,
    const char* key_system,
    Supportability supportability) {
  SB_DCHECK(key_system);
  SB_DCHECK(strlen(key_system) > 0);
  SB_DCHECK(supportability != kSupportabilityUnknown);

  if (!is_enabled_) {
    return;
  }

  if (strlen(key_system) == 0) {
    SB_LOG(WARNING) << "Rejected empty key system as it's always supported.";
    return;
  }

  GetContainer<SbMediaVideoCodec>()->CacheKeySystemSupportability(
      codec, key_system, supportability);
}

void KeySystemSupportabilityCache::ClearCache() {
  GetContainer<SbMediaAudioCodec>()->ClearContainer();
  GetContainer<SbMediaVideoCodec>()->ClearContainer();
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
