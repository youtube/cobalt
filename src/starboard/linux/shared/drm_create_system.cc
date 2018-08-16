// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/log.h"
#include "starboard/shared/widevine/drm_system_widevine.h"
#include "starboard/string.h"

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback
#if SB_HAS(DRM_KEY_STATUSES)
    ,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback
#endif  // SB_HAS(DRM_KEY_STATUSES)
#if SB_API_VERSION >= 10
    ,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback
#endif  // SB_API_VERSION >= 10
    ) {
  using starboard::shared::widevine::DrmSystemWidevine;
  if (!update_request_callback || !session_updated_callback) {
    return kSbDrmSystemInvalid;
  }
#if SB_HAS(DRM_KEY_STATUSES)
  if (!key_statuses_changed_callback) {
    return kSbDrmSystemInvalid;
  }
#endif  // SB_HAS(DRM_KEY_STATUSES)
#if SB_API_VERSION >= 10
  if (!server_certificate_updated_callback || !session_closed_callback) {
    return kSbDrmSystemInvalid;
  }
#endif  // SB_API_VERSION >= 10
  if (!DrmSystemWidevine::IsKeySystemSupported(key_system)) {
    SB_DLOG(WARNING) << "Invalid key system " << key_system;
    return kSbDrmSystemInvalid;
  }
  return new DrmSystemWidevine(context, update_request_callback,
                               session_updated_callback
#if SB_HAS(DRM_KEY_STATUSES)
                               ,
                               key_statuses_changed_callback
#endif  // SB_HAS(DRM_KEY_STATUSES)
#if SB_API_VERSION >= 10
                               ,
                               server_certificate_updated_callback
#endif  // SB_API_VERSION >= 10
#if SB_HAS(DRM_SESSION_CLOSED)
                               ,
                               session_closed_callback
#endif  // SB_HAS(DRM_SESSION_CLOSED)
                               ,
                               "Linux", "Linux");
}
