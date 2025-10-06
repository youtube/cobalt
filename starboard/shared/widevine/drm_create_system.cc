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
#include "starboard/shared/widevine/drm_system_widevine.h"

#warning "This implementation is meant to use for testing purpose only."
#warning "|company_name| and |model_name| should be replaced in production."

namespace {
const char kCompanyName[] = "www";
const char kModelName[] = "www";
}  // namespace

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  using starboard::DrmSystemWidevine;
  if (!update_request_callback || !session_updated_callback) {
    return kSbDrmSystemInvalid;
  }
  if (!key_statuses_changed_callback) {
    return kSbDrmSystemInvalid;
  }
  if (!server_certificate_updated_callback || !session_closed_callback) {
    return kSbDrmSystemInvalid;
  }
  if (!DrmSystemWidevine::IsKeySystemSupported()) {
    SB_DLOG(WARNING) << "Invalid key system " << key_system;
    return kSbDrmSystemInvalid;
  }
  SB_LOG(ERROR) << "|company_name| and |model_name| are set to \"www\", "
                << "premium content playback resolution may be limited.";
  return new DrmSystemWidevine(
      context, update_request_callback, session_updated_callback,
      key_statuses_changed_callback, server_certificate_updated_callback,
      session_closed_callback, kCompanyName, kModelName);
}
