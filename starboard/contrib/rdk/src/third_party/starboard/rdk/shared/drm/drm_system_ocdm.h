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
#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_DRM_DRM_SYSTEM_OCDM_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_DRM_DRM_SYSTEM_OCDM_H_

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "starboard/event.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/common/ref_counted.h"

struct _GstCaps;
struct _GstBuffer;
struct OpenCDMSystem;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace drm {

namespace session {
class Session;
}

class DrmSystemOcdm : public SbDrmSystemPrivate, public ::starboard::RefCountedThreadSafe<DrmSystemOcdm> {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnKeyReady(const uint8_t* key, size_t key_len) = 0;
  };

  struct KeyWithStatus {
    SbDrmKeyId key;
    SbDrmKeyStatus status;
  };

  using KeysWithStatus = std::vector<KeyWithStatus>;

  DrmSystemOcdm(
      const char* key_system,
      void* context,
      SbDrmSessionUpdateRequestFunc update_request_callback,
      SbDrmSessionUpdatedFunc session_updated_callback,
      SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
      SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
      SbDrmSessionClosedFunc session_closed_callback);

  ~DrmSystemOcdm() override;

  static bool IsKeySystemSupported(const char* key_system,
                                   const char* mime_type);

  // SbDrmSystemPrivate
  void GenerateSessionUpdateRequest(int ticket,
                                    const char* type,
                                    const void* initialization_data,
                                    int initialization_data_size) override;
  void CloseSession(const void* session_id, int session_id_size) override;
  void UpdateSession(int ticket,
                     const void* key,
                     int key_size,
                     const void* session_id,
                     int session_id_size) override;
  DecryptStatus Decrypt(::starboard::InputBuffer* buffer) override;
  bool IsServerCertificateUpdatable() override { return false; }
  void UpdateServerCertificate(int ticket,
                               const void* certificate,
                               int certificate_size) override;

  const void* GetMetrics(int* size) override;

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);
  void OnKeyUpdated(const std::string& session_id,
                    SbDrmKeyId&& key_id,
                    SbDrmKeyStatus status);
  void OnAllKeysUpdated();
  std::string SessionIdByKeyId(const uint8_t* key, uint8_t key_len);
  int  Decrypt(const std::string& id,
               _GstBuffer* buffer,
               _GstBuffer* sub_sample,
               uint32_t sub_sample_count,
               _GstBuffer* iv,
               _GstBuffer* key_id,
               _GstCaps* caps);
  std::set<std::string> GetReadyKeys();
  KeysWithStatus GetSessionKeys(const std::string& session_id) const;

  void Invalidate();

 private:
  session::Session* GetSessionById(const std::string& id);
  void AnnounceKeys();

  std::set<std::string> GetReadyKeysUnlocked() const;

  std::string key_system_;
  void* context_;
  std::vector<std::unique_ptr<session::Session>> sessions_;

  const SbDrmSessionUpdateRequestFunc session_update_request_callback_;
  const SbDrmSessionUpdatedFunc session_updated_callback_;
  const SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback_;
  const SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback_;
  const SbDrmSessionClosedFunc session_closed_callback_;

  OpenCDMSystem* ocdm_system_;
  std::vector<Observer*> observers_;
  std::unordered_map<std::string, KeysWithStatus> session_keys_;
  mutable std::set<std::string> cached_ready_keys_;
  SbEventId event_id_;
  std::mutex mutex_;

  std::vector<uint8_t> metrics_;
};

}  // namespace drm
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_DRM_DRM_SYSTEM_OCDM_H_
