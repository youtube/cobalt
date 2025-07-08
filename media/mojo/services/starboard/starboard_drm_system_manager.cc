// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "media/mojo/services/starboard/starboard_drm_system_manager.h"

#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace media {

// static
StarboardDrmSystemManager* StarboardDrmSystemManager::GetInstance() {
  static base::NoDestructor<StarboardDrmSystemManager> instance;
  return instance.get();
}

void StarboardDrmSystemManager::Register(const base::UnguessableToken& token,
                                         SbDrmSystem drm_system) {
  base::AutoLock auto_lock(lock_);
  DCHECK(drm_system);
  DCHECK(!token.is_empty());

  auto result = drm_system_map_.emplace(token, drm_system);
  DCHECK(result.second) << "Token collision is highly unlikely.";
}

void StarboardDrmSystemManager::Unregister(
    const base::UnguessableToken& token) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!token.is_empty());

  size_t erased_count = drm_system_map_.erase(token);
  DCHECK_EQ(erased_count, 1u);
}

absl::optional<SbDrmSystem> StarboardDrmSystemManager::GetDrmSystem(
    const base::UnguessableToken& token) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!token.is_empty());

  auto it = drm_system_map_.find(token);
  if (it == drm_system_map_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

StarboardDrmSystemManager::StarboardDrmSystemManager() = default;
StarboardDrmSystemManager::~StarboardDrmSystemManager() = default;

}  // namespace media
