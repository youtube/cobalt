//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#include <string>

#if defined(HAS_OCDM)
#include "third_party/starboard/rdk/shared/drm/drm_system_ocdm.h"
#else
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#endif

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  if (!update_request_callback || !session_updated_callback) {
    return kSbDrmSystemInvalid;
  }
  if (!key_statuses_changed_callback) {
    return kSbDrmSystemInvalid;
  }
  if (!server_certificate_updated_callback || !session_closed_callback) {
    return kSbDrmSystemInvalid;
  }
#if defined(HAS_OCDM)
  using third_party::starboard::rdk::shared::drm::DrmSystemOcdm;
  std::string empty;
  if (!DrmSystemOcdm::IsKeySystemSupported(key_system, empty.c_str())) {
    SB_DLOG(WARNING) << "Invalid key system " << key_system;
    return kSbDrmSystemInvalid;
  }

  auto *cdm = new DrmSystemOcdm(
      key_system, context, update_request_callback, session_updated_callback,
      key_statuses_changed_callback, server_certificate_updated_callback,
      session_closed_callback);
  cdm->AddRef();
  return cdm;
#else
  return nullptr;
#endif
}
