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

#include "starboard/android/shared/drm_system.h"

#include "starboard/android/shared/media_common.h"

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  using starboard::android::shared::DrmSystem;
  using starboard::android::shared::IsWidevineL1;
  using starboard::android::shared::IsWidevineL3;

  if (!update_request_callback || !session_updated_callback ||
      !key_statuses_changed_callback || !server_certificate_updated_callback ||
      !session_closed_callback) {
    return kSbDrmSystemInvalid;
  }

  if (!IsWidevineL1(key_system) && !IsWidevineL3(key_system)) {
    return kSbDrmSystemInvalid;
  }

  DrmSystem* drm_system =
      new DrmSystem(key_system, context, update_request_callback,
                    session_updated_callback, key_statuses_changed_callback);
  if (!drm_system->is_valid()) {
    delete drm_system;
    return kSbDrmSystemInvalid;
  }
  return drm_system;
}
