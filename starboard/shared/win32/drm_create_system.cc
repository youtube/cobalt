// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/common/string.h"
#include "starboard/shared/win32/drm_system_playready.h"

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  using ::starboard::shared::win32::DrmSystemPlayready;

  if (!update_request_callback || !session_updated_callback ||
      !key_statuses_changed_callback || !server_certificate_updated_callback ||
      !session_closed_callback) {
    SB_DLOG(WARNING) << "Callback functions not set on key system: "
                     << key_system;
    return kSbDrmSystemInvalid;
  }

  if (DrmSystemPlayready::IsKeySystemSupported(key_system)) {
    return new DrmSystemPlayready(
        context, []() { return false; },  // Output protection not supported
        update_request_callback, session_updated_callback,
        key_statuses_changed_callback, session_closed_callback);
  }

  SB_DLOG(WARNING) << "Invalid key system " << key_system;
  return kSbDrmSystemInvalid;
}
