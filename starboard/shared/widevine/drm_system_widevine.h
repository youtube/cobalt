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

#ifndef STARBOARD_SHARED_WIDEVINE_DRM_SYSTEM_WIDEVINE_H_
#define STARBOARD_SHARED_WIDEVINE_DRM_SYSTEM_WIDEVINE_H_

#include <string>

#include "starboard/queue.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "third_party/cdm/cdm/include/content_decryption_module.h"

namespace starboard {
namespace shared {
namespace widevine {

class SbDrmSystemWidevine : public SbDrmSystemPrivate, public cdm::Host {
 public:
  SbDrmSystemWidevine(
      void* context,
      SbDrmSessionUpdateRequestFunc session_update_request_callback,
      SbDrmSessionUpdatedFunc session_updated_callback);
  ~SbDrmSystemWidevine() SB_OVERRIDE;

  // From |SbDrmSystemPrivate|.
  void GenerateSessionUpdateRequest(
#if SB_API_VERSION >= 4
      int ticket,
#endif  // SB_API_VERSION >= 4
      const char* type,
      const void* initialization_data,
      int initialization_data_size) SB_OVERRIDE;
  void UpdateSession(const void* key,
                     int key_size,
                     const void* session_id,
                     int session_id_size) SB_OVERRIDE;
  void CloseSession(const void* session_id, int session_id_size) SB_OVERRIDE;
  DecryptStatus Decrypt(InputBuffer* buffer) SB_OVERRIDE;

  // From |cdm::Host|.
  cdm::Buffer* Allocate(int32_t capacity) SB_OVERRIDE;
  void SetTimer(int64_t delay_in_milliseconds, void* context) SB_OVERRIDE;
  double GetCurrentWallTimeInSeconds() SB_OVERRIDE;
  void SendKeyMessage(const char* web_session_id,
                      int32_t web_session_id_length,
                      const char* message,
                      int32_t message_length,
                      const char* default_url,
                      int32_t default_url_length) SB_OVERRIDE;
  void SendKeyError(const char* web_session_id,
                    int32_t web_session_id_length,
                    cdm::MediaKeyError error_code,
                    uint32_t system_code) SB_OVERRIDE;
  void GetPlatformString(const std::string& name,
                         std::string* value) SB_OVERRIDE;
  void SetPlatformString(const std::string& name,
                         const std::string& value) SB_OVERRIDE;

 private:
  class BufferImpl;
  class DecryptedBlockImpl;

  struct Timer {
    SbTimeMonotonic time_to_fire;
    void* context;

    Timer() : time_to_fire(0), context(NULL) {}
    Timer(int64_t delay_in_milliseconds, void* context)
        : time_to_fire(SbTimeGetMonotonicNow() +
                       delay_in_milliseconds * kSbTimeMillisecond),
          context(context) {}
  };

  static void* GetHostInterface(int host_interface_version, void* user_data);

  void TimerThread();
  static void* TimerThreadFunc(void* context);

  void* context_;
  SbDrmSessionUpdateRequestFunc session_update_request_callback_;
  SbDrmSessionUpdatedFunc session_updated_callback_;

  BufferImpl* buffer_;
  cdm::ContentDecryptionModule* cdm_;

  // |ContentDecryptionModule_1| has no way to associate a key request
  // with a newly created session. To work around that
  // |session_update_request_ticket_| is expected to be set before each call
  // to |GenerateKeyRequest|.
  bool session_update_request_ticket_set_;
  int session_update_request_ticket_;

  bool quitting_;
  Queue<Timer> timer_queue_;
  SbThread timer_thread_;
};

}  // namespace widevine
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIDEVINE_DRM_SYSTEM_WIDEVINE_H_
