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

#ifndef STARBOARD_SHARED_STARBOARD_DRM_DRM_SYSTEM_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_DRM_DRM_SYSTEM_INTERNAL_H_

#include <optional>
#include <string_view>

#include "starboard/drm.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

struct SbDrmSystemPrivate {
 public:
  enum DecryptStatus { kSuccess, kRetry, kFailure };

  virtual ~SbDrmSystemPrivate() {}

  virtual void GenerateSessionUpdateRequest(
      int ticket,
      std::string_view type,
      std::string_view initialization_data) = 0;

  virtual void UpdateSession(int ticket,
                             std::string_view key,
                             std::string_view session_id) = 0;

  virtual void CloseSession(std::string_view session_id) = 0;

  virtual DecryptStatus Decrypt(starboard::InputBuffer* buffer) = 0;

  virtual bool IsServerCertificateUpdatable() = 0;

  virtual void UpdateServerCertificate(int ticket,
                                       std::string_view certificate) = 0;

  virtual std::optional<std::string_view> GetMetrics() = 0;
};

#endif  // STARBOARD_SHARED_STARBOARD_DRM_DRM_SYSTEM_INTERNAL_H_
