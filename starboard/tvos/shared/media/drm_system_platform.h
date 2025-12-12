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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_DRM_SYSTEM_PLATFORM_H_
#define STARBOARD_TVOS_SHARED_MEDIA_DRM_SYSTEM_PLATFORM_H_

#import <AVFoundation/AVFoundation.h>

#include "starboard/drm.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"

namespace starboard {

class DrmSystemPlatform : public SbDrmSystemPrivate {
 public:
  static DrmSystemPlatform* Create(
      void* context,
      SbDrmSessionUpdateRequestFunc update_request_callback,
      SbDrmSessionUpdatedFunc session_updated_callback,
      SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
      SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
      SbDrmSessionClosedFunc session_closed_callback);
  ~DrmSystemPlatform() override;

  static bool IsKeySystemSupported(const char* key_system);
  static bool IsSupported(SbDrmSystem drm_system);
  static std::string GetName();
  static std::string GetKeySystemName();

  // SbDrmSystemPrivate impl.
  void GenerateSessionUpdateRequest(int ticket,
                                    const char* type,
                                    const void* initialization_data,
                                    int initialization_data_size) override;
  void UpdateSession(int ticket,
                     const void* key,
                     int key_size,
                     const void* session_id,
                     int session_id_size) override;
  void CloseSession(const void* session_id, int session_id_size) override;
  DecryptStatus Decrypt(InputBuffer* buffer) override;
  bool IsServerCertificateUpdatable() override;
  void UpdateServerCertificate(int ticket,
                               const void* certificate,
                               int certificate_size) override;
  const void* GetMetrics(int* size) override;

  AVContentKey* GetContentKey(const uint8_t* key_id, int key_id_size)
      API_AVAILABLE(tvos(14.5));
  void OnOutputObscuredChanged(bool is_obscured);

 private:
  DrmSystemPlatform(
      void* context,
      SbDrmSessionUpdateRequestFunc update_request_callback,
      SbDrmSessionUpdatedFunc session_updated_callback,
      SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
      SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
      SbDrmSessionClosedFunc session_closed_callback);
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_DRM_SYSTEM_PLATFORM_H_
