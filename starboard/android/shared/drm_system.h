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

#ifndef STARBOARD_ANDROID_SHARED_DRM_SYSTEM_H_
#define STARBOARD_ANDROID_SHARED_DRM_SYSTEM_H_

// clang-format off
#include "starboard/shared/starboard/drm/drm_system_internal.h"
// clang-format on

#include <jni.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/media_drm_bridge.h"
#include "starboard/common/thread.h"
#include "starboard/types.h"

namespace starboard::android::shared {

class DrmSystem : public ::SbDrmSystemPrivate,
                  public MediaDrmBridge::Host,
                  private Thread {
 public:
  DrmSystem(const char* key_system,
            void* context,
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
                     int session_id_size) override;
  void CloseSession(const void* session_id, int session_id_size) override;
  DecryptStatus Decrypt(InputBuffer* buffer) override;

  bool IsServerCertificateUpdatable() override { return false; }
  void UpdateServerCertificate(int ticket,
                               const void* certificate,
                               int certificate_size) override {}
  const void* GetMetrics(int* size) override;

  jobject GetMediaCrypto() const { return media_drm_bridge_->GetMediaCrypto(); }

  // MediaDrmBridge::Host methods.
  void OnSessionUpdate(int ticket,
                       SbDrmSessionRequestType request_type,
                       std::string_view session_id,
                       std::string_view content,
                       const char* url) override;
  void OnKeyStatusChange(
      std::string_view session_id,
      const std::vector<SbDrmKeyId>& drm_key_ids,
      const std::vector<SbDrmKeyStatus>& drm_key_statuses) override;

  void OnInsufficientOutputProtection();

  bool is_valid() const { return media_drm_bridge_->is_valid(); }
  bool require_secured_decoder() const {
    return IsWidevineL1(key_system_.c_str());
  }

  // Return true when the drm system is ready for secure input buffers.
  bool IsReady() { return created_media_crypto_session_.load(); }

 private:
  class SessionUpdateRequest {
   public:
    SessionUpdateRequest(int ticket,
                         const char* type,
                         const void* initialization_data,
                         int initialization_data_size);
    ~SessionUpdateRequest() = default;

    void Generate(const MediaDrmBridge* media_drm_bridge) const;

   private:
    const int ticket_;
    const std::vector<const uint8_t> init_data_;
    const std::string mime_;
  };

  void CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();

  // From Thread.
  void Run() override;

  const std::string key_system_;
  void* context_;
  SbDrmSessionUpdateRequestFunc update_request_callback_;
  SbDrmSessionUpdatedFunc session_updated_callback_;
  // TODO: Update key statuses to Cobalt.
  SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback_;

  std::vector<std::unique_ptr<SessionUpdateRequest>>
      deferred_session_update_requests_;

  std::mutex mutex_;
  std::unordered_map<std::string, std::vector<SbDrmKeyId>> cached_drm_key_ids_;
  bool hdcp_lost_;
  std::atomic_bool created_media_crypto_session_{false};

  std::unique_ptr<MediaDrmBridge> media_drm_bridge_;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_DRM_SYSTEM_H_
