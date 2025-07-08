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

#ifndef MEDIA_MOJO_SERVICES_STARBOARD_STARBOARD_DRM_SYSTEM_MANAGER_H_
#define MEDIA_MOJO_SERVICES_STARBOARD_STARBOARD_DRM_SYSTEM_MANAGER_H_

#include <map>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "starboard/drm.h"

namespace media {

// A thread-safe singleton to manage mappings from an UnguessableToken to a
// SbDrmSystem pointer. This keeps raw pointers within the secure process.
class StarboardDrmSystemManager {
 public:
  // Gets the singleton instance.
  static StarboardDrmSystemManager* GetInstance();

  StarboardDrmSystemManager(const StarboardDrmSystemManager&) = delete;
  StarboardDrmSystemManager& operator=(const StarboardDrmSystemManager&) =
      delete;

  // Associates a token with a SbDrmSystem.
  void Register(const base::UnguessableToken& token, SbDrmSystem drm_system);

  // Removes a mapping. Call this when the CDM is destroyed.
  void Unregister(const base::UnguessableToken& token);

  // Retrieves a SbDrmSystem given a token. Returns absl::nullopt if not found.
  absl::optional<SbDrmSystem> GetDrmSystem(const base::UnguessableToken& token);

 private:
  friend class base::NoDestructor<StarboardDrmSystemManager>;

  StarboardDrmSystemManager();
  ~StarboardDrmSystemManager();

  // Guards access to the map below.
  base::Lock lock_;

  std::map<base::UnguessableToken, SbDrmSystem> drm_system_map_
      GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STARBOARD_STARBOARD_DRM_SYSTEM_MANAGER_H_
