// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_DRM_DRM_SYSTEM_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_DRM_DRM_SYSTEM_INTERNAL_H_

#include "starboard/drm.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

struct SbDrmSystemPrivate {
 public:
  typedef starboard::shared::starboard::player::InputBuffer InputBuffer;
  enum DecryptStatus { kSuccess, kRetry, kFailure };

  virtual ~SbDrmSystemPrivate() {}

  virtual void GenerateSessionUpdateRequest(
#if SB_API_VERSION >= 4
      int ticket,
#endif  // SB_API_VERSION >= 4
      const char* type,
      const void* initialization_data,
      int initialization_data_size) = 0;
  virtual void UpdateSession(const void* key,
                             int key_size,
                             const void* session_id,
                             int session_id_size) = 0;
  virtual void CloseSession(const void* session_id, int session_id_size) = 0;

  virtual DecryptStatus Decrypt(InputBuffer* buffer) = 0;
};

#endif  // STARBOARD_SHARED_STARBOARD_DRM_DRM_SYSTEM_INTERNAL_H_
