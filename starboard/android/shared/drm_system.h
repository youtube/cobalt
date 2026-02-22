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

#include "starboard/android/shared/drm_session_id_mapper.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/media_drm_bridge.h"
#include "starboard/common/thread.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

class DrmSystem : public ::SbDrmSystemPrivate,
                  public MediaDrmBridge::Host,
                  private Thread {
 public:
  DrmSystem(std::string_view key_system,
            void* context,
            SbDrmSessionUpdateRequestFunc update_request_callback,
            SbDrmSessionUpdatedFunc session_updated_callback,
            SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback);

  ~DrmSystem() override;
  // SbDrmSystemPrivate override begins
  void GenerateSessionUpdateRequest(int ticket,
                                    const char* type,
                                    const void* initialization_data,
                                    int initialization_data_size) override;
  void UpdateSession(int ticket,
                     const void* key,
                     int key_size,
                     const void* session_id,
                     int session_id_size) override;
  void CloseSession(const void* session_id_data, int session_id_size) override;
  DecryptStatus Decrypt(InputBuffer* buffer) override;

  bool IsServerCertificateUpdatable() override { return false; }
  void UpdateServerCertificate(int ticket,
                               const void* certificate,
                               int certificate_size) override {}
  const void* GetMetrics(int* size) override;
  // SbDrmSystemPrivate override ends.

  jobject GetMediaCrypto() const { return media_drm_bridge_->GetMediaCrypto(); }

  // MediaDrmBridge::Host methods.
  void OnSessionUpdate(int ticket,
                       SbDrmSessionRequestType request_type,
                       std::string_view session_id,
                       std::string_view content) override;
  void OnProvisioningRequest(std::string_view content) override;
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
  bool IsReady();

 private:
  class SessionUpdateRequest {
   public:
    SessionUpdateRequest(int ticket,
                         std::string_view mime_type,
                         std::string_view initialization_data);
    ~SessionUpdateRequest() = default;

    void Generate(const MediaDrmBridge* media_drm_bridge) const;
    MediaDrmBridge::OperationResult GenerateWithAppProvisioning(
        const MediaDrmBridge* media_drm_bridge) const;

    // Returns the ticket. On the first call, it returns a valid ticket and
    // resets its internal state to kSbDrmTicketInvalid. Subsequent calls will
    // return `kSbDrmTicketInvalid`, which means a spontaneous drm request.
    int TakeTicket();

   private:
    int ticket_;
    const std::string init_data_;
    const std::string mime_;
  };

  void CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
  void HandlePendingRequests();
  void GenerateSessionUpdateRequestWithAppProvisioning(
      std::unique_ptr<SessionUpdateRequest> request);
  void UpdateSessionWithAppProvisioning(int ticket,
                                        std::string_view key,
                                        std::string_view session_id);

  // From Thread.
  void Run() override;

  const std::string key_system_;
  const bool enable_app_provisioning_;
  void* context_;
  SbDrmSessionUpdateRequestFunc update_request_callback_;
  SbDrmSessionUpdatedFunc session_updated_callback_;
  // TODO: Update key statuses to Cobalt.
  SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback_;

  std::mutex mutex_;

  std::vector<std::unique_ptr<SessionUpdateRequest>>
      deferred_session_update_requests_;  // Guarded by |mutex_|.

  std::unordered_map<std::string, std::vector<SbDrmKeyId>> cached_drm_key_ids_;
  bool hdcp_lost_;
  std::atomic_bool created_media_crypto_session_{false};

  std::unique_ptr<MediaDrmBridge> media_drm_bridge_;

  ThreadChecker thread_checker_;

  // Manages the mapping between the EME session ID in the C++ layer and the
  // MediaDrm session ID in the Java layer. Most of the time, we can use the
  // MediaDrm session ID as the EME session ID. However, there are some cases
  // where we cannot, as the lifecycle of the EME session ID can diverge from
  // the MediaDrm session ID's lifecycle. For example, when a device isn't
  // provisioned, we can't get a MediaDrm session ID (which can be generated
  // only after provisioning). In such scenarios, `DrmSystem` still needs an EME
  // session ID to interact with the Cobalt CDM module.
  const std::unique_ptr<DrmSessionIdMapper>
      session_id_mapper_;  //  Guarded by |mutex_|.
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DRM_SYSTEM_H_
