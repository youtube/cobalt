// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/widevine_drm_system_registry.h"

#include <set>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/once.h"
#include "starboard/drm.h"

namespace starboard {
namespace shared {
namespace uikit {

class WidevineDrmSystemRegistryImpl {
 public:
  void Register(SbDrmSystem drm_system) {
    SB_DCHECK(SbDrmSystemIsValid(drm_system));
    ScopedLock scoped_lock(mutex_);
    SB_DCHECK(!IsWidevineDrmSystem_Locked(drm_system));
    drm_systems_.insert(drm_system);
  }

  void Unregister(SbDrmSystem drm_system) {
    ScopedLock scoped_lock(mutex_);
    SB_DCHECK(IsWidevineDrmSystem_Locked(drm_system));
    drm_systems_.erase(drm_system);
  }

  bool IsWidevineDrmSystem(SbDrmSystem drm_system) const {
    ScopedLock scoped_lock(mutex_);
    return IsWidevineDrmSystem_Locked(drm_system);
  }

 private:
  bool IsWidevineDrmSystem_Locked(SbDrmSystem drm_system) const {
    mutex_.DCheckAcquired();
    return drm_systems_.find(drm_system) != drm_systems_.end();
  }

  Mutex mutex_;
  std::set<SbDrmSystem> drm_systems_;
};

SB_ONCE_INITIALIZE_FUNCTION(WidevineDrmSystemRegistryImpl,
                            GetWidevineDrmSystemRegistryImpl);

// static
void WidevineDrmSystemRegistry::Register(SbDrmSystem drm_system) {
  GetWidevineDrmSystemRegistryImpl()->Register(drm_system);
}

// static
void WidevineDrmSystemRegistry::Unregister(SbDrmSystem drm_system) {
  GetWidevineDrmSystemRegistryImpl()->Unregister(drm_system);
}

// static
bool WidevineDrmSystemRegistry::IsWidevineDrmSystem(SbDrmSystem drm_system) {
  return GetWidevineDrmSystemRegistryImpl()->IsWidevineDrmSystem(drm_system);
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
