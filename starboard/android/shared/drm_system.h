// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_DRM_SYSTEM_H_
#define STARBOARD_ANDROID_SHARED_DRM_SYSTEM_H_

#include "starboard/shared/starboard/drm/drm_system_internal.h"

#include <jni.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "starboard/log.h"
#include "starboard/mutex.h"

namespace starboard {
namespace android {
namespace shared {

class DrmSystem : public ::SbDrmSystemPrivate {
 public:
  DrmSystem(void* context,
            SbDrmSessionUpdateRequestFunc update_request_callback,
            SbDrmSessionUpdatedFunc session_updated_callback,
            SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback);

  ~DrmSystem() override;
  void GenerateSessionUpdateRequest(int ticket,
                                    const char* type,
                                    const void* initialization_data,
                                    int initialization_data_size) override;
  void UpdateSession(int ticket,
                     const void* key,
                     int key_size,
                     const void* session_id,
                     int session_id_size);
  void CloseSession(const void* session_id, int session_id_size) override;
  DecryptStatus Decrypt(InputBuffer* buffer) override;

  jobject GetMediaCrypto() const { return j_media_crypto_; }
  void CallUpdateRequestCallback(int ticket,
                                 const void* session_id,
                                 int session_id_size,
                                 const void* content,
                                 int content_size,
                                 const char* url);
  void CallDrmSessionKeyStatusesChangedCallback(
      const void* session_id,
      int session_id_size,
      const std::vector<SbDrmKeyId>& drm_key_ids,
      const std::vector<SbDrmKeyStatus>& drm_key_statuses);
  void OnInsufficientOutputProtection();

  bool is_valid() const {
    return j_media_drm_bridge_ != NULL && j_media_crypto_ != NULL;
  }

 private:
  void* context_;
  SbDrmSessionUpdateRequestFunc update_request_callback_;
  SbDrmSessionUpdatedFunc session_updated_callback_;
  // TODO: Update key statuses to Cobalt.
  SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback_;

  jobject j_media_drm_bridge_;
  jobject j_media_crypto_;

  Mutex mutex_;
  std::unordered_map<std::string, std::vector<SbDrmKeyId> > cached_drm_key_ids_;
  bool hdcp_lost_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DRM_SYSTEM_H_
