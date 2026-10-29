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
#include <stdint.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "starboard/android/shared/drm_session_id_mapper.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/media_drm_bridge.h"
#include "starboard/common/pass_key.h"
#include "starboard/common/span.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

class DrmSystem : public ::SbDrmSystemPrivate, public MediaDrmBridge::Host {
 public:
  struct Callbacks {
    SbDrmSessionUpdateRequestFunc update_request;
    SbDrmSessionUpdatedFunc session_updated;
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed;
    SbDrmSessionClosedFunc session_closed;
  };
  using KeyStateChangedCallback = std::function<void()>;

  static std::unique_ptr<DrmSystem> Create(std::string_view key_system,
                                           void* context,
                                           Callbacks callbacks);

  DrmSystem(PassKey<DrmSystem>,
            std::string_view key_system,
            void* context,
            Callbacks callbacks);

  ~DrmSystem() override;
  // SbDrmSystemPrivate override begins
  void GenerateSessionUpdateRequest(
      int ticket,
      std::string_view type,
      span<const uint8_t> initialization_data) override;
  void UpdateSession(int ticket,
                     span<const uint8_t> key,
                     std::string_view session_id) override;
  void CloseSession(std::string_view session_id) override;
  DecryptStatus Decrypt(InputBuffer* buffer) override;

  bool IsServerCertificateUpdatable() override { return false; }
  void UpdateServerCertificate(int ticket,
                               span<const uint8_t> certificate) override {}
  std::optional<std::string_view> GetMetrics() override;
  // SbDrmSystemPrivate override ends.

  const jni_zero::JavaRef<jobject>& GetMediaCrypto() const {
    return media_drm_bridge_->GetMediaCrypto();
  }

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

  bool require_secured_decoder() const {
    return IsWidevineL1(key_system_.c_str());
  }

  // Return true when the drm system is ready for secure input buffers, i.e.
  // once the media crypto session is created. That normally happens in the
  // constructor, or otherwise on the first successful session update request
  // (e.g. after provisioning).
  bool IsReady();

  // Observers of DRM key state changes (e.g. when IsReady() becomes true, a
  // license is provided, or key statuses change).  Callbacks run on arbitrary
  // threads under an internal lock, so they must be lightweight and must not
  // call back into DrmSystem.  UnregisterKeyStateChangedCallback() blocks until
  // any in-flight callback returns.
  void RegisterKeyStateChangedCallback(const void* observer,
                                       KeyStateChangedCallback callback);
  void UnregisterKeyStateChangedCallback(const void* observer);
  // Incremented before registered callbacks run so observers snapshotting
  // before an operation cannot miss an intervening key state change.
  uint64_t GetKeyStateGeneration() const {
    return key_state_generation_.load(std::memory_order_acquire);
  }

 private:
  class SessionUpdateRequest {
   public:
    SessionUpdateRequest(int ticket,
                         std::string_view mime_type,
                         span<const uint8_t> initialization_data);
    ~SessionUpdateRequest() = default;

    MediaDrmBridge::OperationResult Generate(
        const MediaDrmBridge* media_drm_bridge) const;

    // Returns the ticket. On the first call, it returns a valid ticket and
    // resets its internal state to kSbDrmTicketInvalid. Subsequent calls will
    // return `kSbDrmTicketInvalid`, which means a spontaneous drm request.
    int TakeTicket();

   private:
    int ticket_;
    const std::vector<uint8_t> init_data_;
    const std::string mime_;
  };

  void CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
  void HandlePendingRequests();
  void GenerateSessionUpdateRequest(
      std::unique_ptr<SessionUpdateRequest> request);
  // Bumps |key_state_generation_| and runs registered callbacks; must not be
  // called with |mutex_| held.
  void NotifyKeyStateChanged();

  const std::string key_system_;
  const raw_ptr<void> context_;
  const Callbacks callbacks_;

  std::mutex mutex_;

  std::vector<std::unique_ptr<SessionUpdateRequest>>
      deferred_session_update_requests_;  // Guarded by |mutex_|.

  std::map<std::string, std::vector<SbDrmKeyId>, std::less<>>
      cached_drm_key_ids_;
  bool hdcp_lost_;
  std::atomic_bool created_media_crypto_session_{false};

  // Manages the mapping between the EME session ID in the C++ layer and the
  // MediaDrm session ID in the Java layer. Most of the time, we can use the
  // MediaDrm session ID as the EME session ID. However, there are some cases
  // where we cannot, as the lifecycle of the EME session ID can diverge from
  // the MediaDrm session ID's lifecycle. For example, when a device isn't
  // provisioned, we can't get a MediaDrm session ID (which can be generated
  // only after provisioning). In such scenarios, `DrmSystem` still needs an EME
  // session ID to interact with the Cobalt CDM module.
  DrmSessionIdMapper session_id_mapper_;  //  Guarded by |mutex_|.

  // Guards |key_state_callbacks_| and is held while callbacks run; never
  // acquired while |mutex_| is held.
  std::mutex key_state_callbacks_mutex_;
  std::map<const void*, KeyStateChangedCallback>
      key_state_callbacks_;  // Guarded by |key_state_callbacks_mutex_|.

  std::atomic_bool has_key_state_callbacks_{false};
  // Incremented before callbacks run so observers snapshotting before an
  // operation cannot miss an intervening key state change.
  std::atomic<uint64_t> key_state_generation_{0};

  // Guaranteed to be non-null for any instance returned by the factory method.
  // However, it can be null during destruction if the factory method fails
  // to create the bridge and destroys the instance.
  const std::unique_ptr<MediaDrmBridge> media_drm_bridge_;

  ThreadChecker thread_checker_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DRM_SYSTEM_H_
