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

#include "starboard/common/string.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/widevine/drm_system_widevine.h"
#import "starboard/tvos/shared/media/drm_manager.h"
#include "starboard/tvos/shared/media/drm_system_platform.h"
#include "starboard/tvos/shared/media/oemcrypto_engine_device_properties_uikit.h"
#import "starboard/tvos/shared/starboard_application.h"

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  if (!update_request_callback || !session_updated_callback ||
      !key_statuses_changed_callback || !server_certificate_updated_callback ||
      !session_closed_callback) {
    return kSbDrmSystemInvalid;
  }

  if (!starboard::MediaIsSupported(kSbMediaVideoCodecNone,
                                   kSbMediaAudioCodecNone, key_system)) {
    return kSbDrmSystemInvalid;
  }

  using starboard::shared::widevine::DrmSystemWidevine;
  if (strstr(key_system, "widevine")) {
    auto drm_system = new DrmSystemWidevine(
        context, update_request_callback, session_updated_callback,
        key_statuses_changed_callback, server_certificate_updated_callback,
        session_closed_callback, "Apple", "Apple TV");

    if (strstr(key_system, "forcehdcp")) {
      wvoec_mock::OEMCrypto_AlwaysEnforceOutputProtection();
    }
    return drm_system;
  }

  using starboard::shared::uikit::DrmSystemPlatform;
  if (DrmSystemPlatform::IsKeySystemSupported(key_system)) {
    return DrmSystemPlatform::Create(
        context, update_request_callback, session_updated_callback,
        key_statuses_changed_callback, server_certificate_updated_callback,
        session_closed_callback);
  }

  if (!strstr(key_system, DrmSystemPlatform::GetName().c_str())) {
    SB_DLOG(WARNING) << "Invalid key system " << key_system;
    return kSbDrmSystemInvalid;
  }

  @autoreleasepool {
    SBDDrmManager* drmManager = SBDGetApplication().drmManager;
    SBDApplicationDrmSystem* drmSystem =
        [drmManager drmSystemWithContext:context
                sessionUpdateRequestFunc:update_request_callback
                      sessionUpdatedFunc:session_updated_callback];
    return [drmManager starboardDrmSystemForApplicationDrmSystem:drmSystem];
  }
}
