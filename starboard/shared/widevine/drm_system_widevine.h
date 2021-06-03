// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <limits>
#include <map>
#include <string>
#include <vector>

#include "starboard/atomic.h"
#include "starboard/common/optional.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/thread.h"
#include "third_party/ce_cdm/cdm/include/cdm.h"

namespace starboard {
namespace shared {
namespace widevine {

// Adapts Widevine's |Content Decryption Module v 3.5| to Starboard's
// |SbDrmSystem|.
//
// All |SbDrmSystemPrivate| methods except Decrypt() must be called from the
// constructor thread.
class DrmSystemWidevine : public SbDrmSystemPrivate,
                          private ::widevine::Cdm::IEventListener {
 public:
  DrmSystemWidevine(
      void* context,
      SbDrmSessionUpdateRequestFunc session_update_request_callback,
      SbDrmSessionUpdatedFunc session_updated_callback,
      SbDrmSessionKeyStatusesChangedFunc session_key_statuses_changed_callback,
      SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
      SbDrmSessionClosedFunc session_closed_callback,
      const std::string& company_name,
      const std::string& model_name);

  ~DrmSystemWidevine() override;

  static bool IsKeySystemSupported(const char* key_system);
  static bool IsDrmSystemWidevine(SbDrmSystem drm_system);

  // From |SbDrmSystemPrivate|.
  void GenerateSessionUpdateRequest(int ticket,
                                    const char* type,
                                    const void* initialization_data,
                                    int initialization_data_size) override;

  void UpdateSession(int ticket,
                     const void* key,
                     int key_size,
                     const void* sb_drm_session_id,
                     int sb_drm_session_id_size) override;

  void CloseSession(const void* sb_drm_session_id,
                    int sb_drm_session_id_size) override;

  DecryptStatus Decrypt(InputBuffer* buffer) override;

  bool IsServerCertificateUpdatable() override { return true; }

  // This function is called by the app to explicitly set the server
  // certificate.  For an app that supports this feature, it should call this
  // function before calling any other functions like
  // GenerateSessionUpdateRequest().  So we needn't process pending requests in
  // this function.  Note that it is benign if this function is called in
  // parallel with a server certificate request.
  void UpdateServerCertificate(int ticket,
                               const void* certificate,
                               int certificate_size) override;

  const void* GetMetrics(int* size) override { return NULL; }

 private:
  // Stores the data necessary to call GenerateSessionUpdateRequestInternal().
  struct GenerateSessionUpdateRequestData {
    int ticket;
    ::widevine::Cdm::InitDataType init_data_type;
    std::string initialization_data;
  };

  // An unique id to identify the first SbDrm session id when server
  // certificate is not ready.  This is necessary to send the server
  // certificate request, as EME requires a session id while wvcdm cannot
  // generate a session id before having a server certificate.
  // This works with |first_wvcdm_session_id_| to map the first session id
  // between wvcdm and SbDrm.
  static const char kFirstSbDrmSessionId[];

  void GenerateSessionUpdateRequestInternal(
      int ticket,
      ::widevine::Cdm::InitDataType init_data_type,
      const std::string& initialization_data,
      bool is_first_session);

  // From |cdm::IEventListener|.
  // A message (license request, renewal, etc.) to be dispatched to the
  // application's license server. The response, if successful, should be
  // provided back to the CDM via a call to Cdm::update().
  void onMessage(const std::string& session_id,
                 ::widevine::Cdm::MessageType message_type,
                 const std::string& message) override;
  // There has been a change in the keys in the session or their status.
  void onKeyStatusesChange(const std::string& wvcdm_session_id) override;
  // A remove() operation has been completed.
  void onRemoveComplete(const std::string& wvcdm_session_id) override;
  // Called when a deferred action has completed.
  void onDeferredComplete(const std::string& wvcdm_session_id,
                          ::widevine::Cdm::Status result) override;
  // Called when the CDM requires a new device certificate.
  void onDirectIndividualizationRequest(const std::string& wvcdm_session_id,
                                        const std::string& request) override;

  void SetTicket(const std::string& sb_drm_session_id, int ticket);
  int GetAndResetTicket(const std::string& sb_drm_session_id);
  std::string WvdmSessionIdToSbDrmSessionId(
      const std::string& wvcdm_session_id);
  bool SbDrmSessionIdToWvdmSessionId(const void* sb_drm_session_id,
                                     int sb_drm_session_id_size,
                                     std::string* wvcdm_session_id);

  // Generates a special key message to ask for the server certificate.  When
  // the license server receives the request, it will send back the server
  // certificate.
  void SendServerCertificateRequest(int ticket);
  // When this function is called, the update contains the server certificate.
  // The function parses the special update and pass the server certificate to
  // the cdm.
  // Note that the app shouldn't persist the server certificate across playback
  // or across application instances.
  ::widevine::Cdm::Status ProcessServerCertificateResponse(
      const std::string& response);
  // If server certificate has been set, send all pending requests.
  void TrySendPendingGenerateSessionUpdateRequests();
  void SendSessionUpdateRequest(SbDrmSessionRequestType type,
                                const std::string& sb_drm_session_id,
                                const std::string& message);

  ::starboard::shared::starboard::ThreadChecker thread_checker_;
  void* const context_;
  const SbDrmSessionUpdateRequestFunc session_update_request_callback_;
  const SbDrmSessionUpdatedFunc session_updated_callback_;
  const SbDrmSessionKeyStatusesChangedFunc
      session_key_statuses_changed_callback_;
  const SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback_;
  const SbDrmSessionClosedFunc session_closed_callback_;

  // Store a map from session id generated by the cdm to its associated ticket
  // id.  The ticket is a unique id passed to GenerateSessionUpdateRequest() to
  // allow the caller of GenerateSessionUpdateRequest() to associate the
  // session id with the session related data specified by the ticket, as both
  // of them will be passed via session_update_request_callback_ when it is
  // called for the first time for this particular session id.  As this is only
  // necessary for the first time the callback is called on the particular
  // session, every time an entry is used, it will be removed from the map.
  // Note that the first callback is always accessed on the thread specified
  // by |ticket_thread_id_|.
  std::map<std::string, int> sb_drm_session_id_to_ticket_map_;

  // |ticket_| is only valid on the constructor thread within the duration of
  // call to |GenerateKeyRequest| or |AddKey|, but CDM may invoke host's methods
  // spontaneously from the timer thread. In that case |GetTicket| need to
  // return |kSbDrmTicketInvalid|.
  const SbThreadId ticket_thread_id_;

  std::vector<GenerateSessionUpdateRequestData>
      pending_generate_session_update_requests_;
  std::string first_wvcdm_session_id_;

  scoped_ptr<::widevine::Cdm> cdm_;

  bool is_server_certificate_set_ = false;

  volatile bool quitting_ = false;

  Mutex unblock_key_retry_mutex_;
  optional<SbTimeMonotonic> unblock_key_retry_start_time_;

#if !defined(COBALT_BUILD_TYPE_GOLD)
  int number_of_session_updates_sent_ = 0;
  int maximum_number_of_session_updates_ = std::numeric_limits<int>::max();
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  atomic_bool first_update_session_received_{false};
};

}  // namespace widevine
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIDEVINE_DRM_SYSTEM_WIDEVINE_H_
