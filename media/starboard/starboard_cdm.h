// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_STARBOARD_CDM_H_
#define MEDIA_STARBOARD_STARBOARD_CDM_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"
#include "starboard/drm.h"

namespace media {

class MEDIA_EXPORT StarboardCdm : public ContentDecryptionModule,
                                  public CdmContext {
 public:
  // TODO(b/406263478) - Cobalt: Check DRM contents support on linux.
  StarboardCdm(const CdmConfig& cdm_config,
               const SessionMessageCB& session_message_cb,
               const SessionClosedCB& session_closed_cb,
               const SessionKeysChangeCB& session_keys_change_cb,
               const SessionExpirationUpdateCB& session_expiration_update_cb);
  ~StarboardCdm();

  StarboardCdm(const StarboardCdm&) = delete;
  StarboardCdm& operator=(const StarboardCdm&) = delete;

  // ContentDecryptionModule implementation.
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            std::unique_ptr<SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      CdmSessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<NewSessionCdmPromise> promise) override;
  void LoadSession(CdmSessionType session_type,
                   const std::string& session_id,
                   std::unique_ptr<NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<SimpleCdmPromise> promise) override;
  CdmContext* GetCdmContext() override;

  // CdmContext implementation.
  std::unique_ptr<CallbackRegistration> RegisterEventCB(
      EventCB event_cb) override;

  SbDrmSystem GetSbDrmSystem() override;

  bool HasValidSbDrm();

 private:
  // Stores context of |GenerateSessionUpdateRequest|.
  struct SessionUpdateRequest {
    uint32_t promise_id;
    CdmSessionType session_type;
  };
  typedef std::unordered_map<int, SessionUpdateRequest>
      TicketToSessionUpdateRequestMap;

  typedef std::unordered_map<int, uint32_t> TicketToServerCertificateUpdatedMap;

  // Stores context of |Session::Update|.
  struct SessionUpdate {
    uint32_t promise_id;
  };
  typedef std::unordered_map<int, SessionUpdate> TicketToSessionUpdateMap;

  struct SessionTicketAndOptionalId {
    const int ticket;
    const std::optional<std::string> id;

    SessionTicketAndOptionalId(int ticket, const std::string& session_id);
    // Create SessionTicketAndOptionalId with null session id.
    SessionTicketAndOptionalId(int ticket);

    std::string ToString() const;
  };

  // Called on the constructor thread, parameters are copied and owned by these
  // methods.
  void OnSessionUpdateRequestGenerated(
      SessionTicketAndOptionalId ticket_and_optional_id,
      SbDrmStatus status,
      SbDrmSessionRequestType type,
      const std::string& error_message,
      std::vector<uint8_t> message,
      int message_size);
  void OnSessionUpdated(int ticket,
                        SbDrmStatus status,
                        const std::string& error_message);
  void OnSessionKeyStatusChanged(const std::string& session_id,
                                 CdmKeysInfo keys_info,
                                 bool has_additional_usable_keys);

  void OnServerCertificateUpdated(int ticket,
                                  SbDrmStatus status,
                                  const std::string& error_message);
  void OnSessionClosed(const std::string& session_id);

  // SbDrm functions
  static void OnSessionUpdateRequestGeneratedFunc(
      SbDrmSystem wrapped_drm_system,
      void* context,
      int ticket,
      SbDrmStatus status,
      SbDrmSessionRequestType type,
      const char* error_message,
      const void* session_id,
      int session_id_size,
      const void* content,
      int content_size,
      const char* url);

  static void OnSessionUpdatedFunc(SbDrmSystem wrapped_drm_system,
                                   void* context,
                                   int ticket,
                                   SbDrmStatus status,
                                   const char* error_message,
                                   const void* session_id,
                                   int session_id_length);

  static void OnSessionKeyStatusesChangedFunc(
      SbDrmSystem wrapped_drm_system,
      void* context,
      const void* session_id,
      int session_id_size,
      int number_of_keys,
      const SbDrmKeyId* key_ids,
      const SbDrmKeyStatus* key_statuses);

  static void OnSessionClosedFunc(SbDrmSystem wrapped_drm_system,
                                  void* context,
                                  const void* session_id,
                                  int session_id_size);

  static void OnServerCertificateUpdatedFunc(SbDrmSystem wrapped_drm_system,
                                             void* context,
                                             int ticket,
                                             SbDrmStatus status,
                                             const char* error_message);

  // Default task runner.
  scoped_refptr<base::SequencedTaskRunner> const task_runner_;
  const SbDrmSystem sb_drm_;
  SessionMessageCB message_cb_;
  SessionClosedCB closed_cb_;
  SessionKeysChangeCB keys_change_cb_;
  SessionExpirationUpdateCB expiration_update_cb_;

  int next_ticket_ = 0;
  TicketToSessionUpdateRequestMap ticket_to_session_update_request_map_;
  TicketToSessionUpdateMap ticket_to_session_update_map_;
  TicketToServerCertificateUpdatedMap ticket_to_server_certificate_updated_map_;

  CdmPromiseAdapter promises_;

  std::list<std::string> session_list_;

  CallbackRegistry<EventCB::RunType> event_callbacks_;

  // NOTE: Do not add member variables after weak_factory_
  // It should be the first one destroyed among all members.
  // See base/memory/weak_ptr.h.
  base::WeakPtrFactory<StarboardCdm> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_CDM_H_
