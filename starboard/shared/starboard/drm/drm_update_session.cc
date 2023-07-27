// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/drm.h"

#include "starboard/common/log.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"

void SbDrmUpdateSession(SbDrmSystem drm_system,
                        int ticket,
                        const void* key,
                        int key_size,
                        const void* session_id,
                        int session_id_size) {
  if (!SbDrmSystemIsValid(drm_system)) {
    SB_DLOG(WARNING) << "Invalid drm system";
    return;
  }

  drm_system->UpdateSession(
      ticket,
      key, key_size, session_id, session_id_size);
}
