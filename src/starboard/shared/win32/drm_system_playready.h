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

#ifndef STARBOARD_SHARED_WIN32_DRM_SYSTEM_PLAYREADY_H_
#define STARBOARD_SHARED_WIN32_DRM_SYSTEM_PLAYREADY_H_

#include <mfapi.h>
#include <mfidl.h>
#include <wrl.h>
#include <wrl/client.h>

#include <map>
#include <string>

#include "starboard/common/ref_counted.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace win32 {

// Adapts PlayReady decryption module to Starboard's |SbDrmSystem|.
class SbDrmSystemPlayready : public SbDrmSystemPrivate {
 public:
  class License : public RefCountedThreadSafe<License> {
   public:
    static scoped_refptr<License> Create(const void* initialization_data,
                                         int initialization_data_size);

    virtual ~License() {}

    virtual GUID key_id() const = 0;
    virtual bool usable() const = 0;
    virtual std::string license_challenge() const = 0;
    virtual Microsoft::WRL::ComPtr<IMFTransform> decryptor() = 0;
    virtual void UpdateLicense(const void* license, int license_size) = 0;
    virtual bool IsHDCPRequired() = 0;
  };

  SbDrmSystemPlayready(
      void* context,
      SbDrmSessionUpdateRequestFunc session_update_request_callback,
      SbDrmSessionUpdatedFunc session_updated_callback,
      SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback);

  ~SbDrmSystemPlayready() override;

  // From |SbDrmSystemPrivate|.
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

  // Used by audio and video decoders to retrieve the decryptors.
  scoped_refptr<License> GetLicense(const uint8_t* key_id, int key_id_size);

 private:
  std::string GenerateAndAdvanceSessionId();
  // Note: requires mutex_ to be held
  void ReportKeyStatusChanged_Locked(const std::string& session_id);

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  void* context_;
  SbDrmSessionUpdateRequestFunc session_update_request_callback_;
  SbDrmSessionUpdatedFunc session_updated_callback_;
  SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback_;
  int current_session_id_;

  std::map<std::string, scoped_refptr<License> > pending_requests_;

  // |successful_requests_| can be accessed from more than one thread.  Guard
  // it by a mutex.
  Mutex mutex_;

  struct LicenseInfo {
    LicenseInfo(SbDrmKeyStatus status, const scoped_refptr<License>& license)
        : status_(status), license_(license) {}
    LicenseInfo() : status_(kSbDrmKeyStatusError) {}

    SbDrmKeyStatus status_;
    scoped_refptr<License> license_;
  };

  std::map<std::string, LicenseInfo> successful_requests_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_DRM_SYSTEM_PLAYREADY_H_
