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
#include "starboard/thread.h"
#include "third_party/cdm/cdm/include/content_decryption_module.h"

namespace starboard {
namespace shared {
namespace widevine {

// Adapts Widevine's |ContentDecryptionModule| to Starboard's |SbDrmSystem|.
//
// When called through |cdm::Host| interface, this class is thread-safe.
// All |SbDrmSystemPrivate| methods must be called from the constructor thread.
class SbDrmSystemWidevine : public SbDrmSystemPrivate, public cdm::Host {
 public:
  SbDrmSystemWidevine(
      void* context,
      SbDrmSessionUpdateRequestFunc session_update_request_callback,
      SbDrmSessionUpdatedFunc session_updated_callback
#if SB_HAS(DRM_KEY_STATUSES)
      ,
      SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback
#endif    // SB_HAS(DRM_KEY_STATUSES)
      );  // NOLINT(whitespace/parens)
  ~SbDrmSystemWidevine() override;

  // From |SbDrmSystemPrivate|.
  void GenerateSessionUpdateRequest(
      int ticket,
      const char* type,
      const void* initialization_data,
      int initialization_data_size) override;
  void UpdateSession(
      int ticket,
      const void* key,
      int key_size,
      const void* session_id,
      int session_id_size) override;
  void CloseSession(const void* session_id, int session_id_size) override;
  DecryptStatus Decrypt(InputBuffer* buffer) override;

  // From |cdm::Host|.
  //
  // At least |SendKeyMessage| and |SendKeyError| are known to be called
  // by the CDM from both constructor and timer threads.
  cdm::Buffer* Allocate(int32_t capacity) override;
  void SetTimer(int64_t delay_in_milliseconds, void* context) override;
  double GetCurrentWallTimeInSeconds() override;
  void SendKeyMessage(const char* web_session_id,
                      int32_t web_session_id_length,
                      const char* message,
                      int32_t message_length,
                      const char* default_url,
                      int32_t default_url_length) override;
  void SendKeyError(const char* web_session_id,
                    int32_t web_session_id_length,
                    cdm::MediaKeyError error_code,
                    uint32_t system_code) override;
  void GetPlatformString(const std::string& name,
                         std::string* value) override;
  void SetPlatformString(const std::string& name,
                         const std::string& value) override;

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

  void SetTicket(int ticket);
  int GetTicket() const;

  void* const context_;
  const SbDrmSessionUpdateRequestFunc session_update_request_callback_;
  const SbDrmSessionUpdatedFunc session_updated_callback_;
#if SB_HAS(DRM_KEY_STATUSES)
  const SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback_;
#endif  // SB_HAS(DRM_KEY_STATUSES)

  // Ticket is is expected to be set before each call to |GenerateKeyRequest|
  // and |AddKey|, so that it can be passed back through
  // |session_update_request_callback_| and |session_updated_callback_|
  // correspondingly.
  int ticket_;
  // |ticket_| is only valid on the constructor thread within the duration of
  // call to |GenerateKeyRequest| or |AddKey|, but CDM may invoke host's methods
  // spontaneously from the timer thread. In that case |GetTicket| need to
  // return |kSbDrmTicketInvalid|.
  const SbThreadId ticket_thread_id_;

  BufferImpl* const buffer_;
  cdm::ContentDecryptionModule* const cdm_;

  volatile bool quitting_;
  Queue<Timer> timer_queue_;
  const SbThread timer_thread_;
};

}  // namespace widevine
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIDEVINE_DRM_SYSTEM_WIDEVINE_H_
