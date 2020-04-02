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

#ifndef COBALT_MEDIA_BASE_DRM_SYSTEM_H_
#define COBALT_MEDIA_BASE_DRM_SYSTEM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "starboard/drm.h"

namespace cobalt {
namespace media {

// A C++ wrapper around |SbDrmSystem|.
//
// Ensures that callbacks are always asynchronous and performed
// from the same thread where |DrmSystem| was instantiated.
class DrmSystem : public base::RefCounted<DrmSystem> {
 public:
  typedef base::Callback<void(SbDrmSessionRequestType type,
                              std::unique_ptr<uint8[]> message,
                              int message_size)>
      SessionUpdateRequestGeneratedCallback;
  typedef base::Callback<void(SbDrmStatus status,
                              const std::string& error_message)>
      SessionUpdateRequestDidNotGenerateCallback;
  typedef base::Callback<void()> SessionUpdatedCallback;
  typedef base::Callback<void(SbDrmStatus status,
                              const std::string& error_message)>
      SessionDidNotUpdateCallback;
  typedef base::Callback<void(const std::vector<std::string>& key_ids,
                              const std::vector<SbDrmKeyStatus>& key_statuses)>
      SessionUpdateKeyStatusesCallback;
  typedef base::Callback<void()> SessionClosedCallback;
  typedef base::Callback<void(SbDrmStatus status,
                              const std::string& error_message)>
      ServerCertificateUpdatedCallback;

  // Flyweight that provides RAII semantics for sessions.
  // Most of logic is implemented by |DrmSystem| and thus sessions must be
  // destroyed before |DrmSystem|.
  class Session {
   public:
    ~Session();

    const base::Optional<std::string>& id() const { return id_; }

    // Wraps |SbDrmGenerateSessionUpdateRequest|.
    //
    // |session_update_request_generated_callback| is called upon a successful
    //     request generation. IMPORTANT: It may be called multiple times after
    //     a single call to |CreateSessionAndGenerateUpdateRequest|, for example
    //     when the underlying DRM system needs to update a license.
    //
    // |session_update_request_did_not_generate_callback| is called upon a
    //     failure during request generation. Unlike its successful counterpart,
    //     never called spontaneously.
    void GenerateUpdateRequest(
        const std::string& type, const uint8* init_data, int init_data_length,
        const SessionUpdateRequestGeneratedCallback&
            session_update_request_generated_callback,
        const SessionUpdateRequestDidNotGenerateCallback&
            session_update_request_did_not_generate_callback);

    // Wraps |SbDrmUpdateSession|.
    //
    // |session_updated_callback| is called upon a successful session update.
    // |session_did_not_update_callback| is called upon a failure during session
    //     update.
    void Update(
        const uint8* key, int key_length,
        const SessionUpdatedCallback& session_updated_callback,
        const SessionDidNotUpdateCallback& session_did_not_update_callback);

    // Wraps |SbDrmCloseSession|.
    void Close();
    bool is_closed() const { return closed_; }

   private:
    // Private API for |DrmSystem|.
    Session(DrmSystem* drm_system,
            SessionUpdateKeyStatusesCallback update_key_statuses_callback,
            SessionClosedCallback session_closed_callback);
    void set_id(const std::string& id) { id_ = id; }
    const SessionUpdateRequestGeneratedCallback&
    update_request_generated_callback() const {
      return update_request_generated_callback_;
    }
    const SessionUpdateKeyStatusesCallback& update_key_statuses_callback()
        const {
      return update_key_statuses_callback_;
    }
    const SessionClosedCallback& session_closed_callback() const {
      return session_closed_callback_;
    }

    DrmSystem* const drm_system_;
    SessionUpdateKeyStatusesCallback update_key_statuses_callback_;
    SessionClosedCallback session_closed_callback_;
    bool closed_;
    base::Optional<std::string> id_;
    // Supports spontaneous invocations of |SbDrmSessionUpdateRequestFunc|.
    SessionUpdateRequestGeneratedCallback update_request_generated_callback_;

    base::WeakPtrFactory<Session> weak_factory_;

    friend class DrmSystem;

    DISALLOW_COPY_AND_ASSIGN(Session);
  };

  explicit DrmSystem(const char* key_system);
  ~DrmSystem();

  SbDrmSystem wrapped_drm_system() { return wrapped_drm_system_; }

  bool is_valid() const { return SbDrmSystemIsValid(wrapped_drm_system_); }

  std::unique_ptr<Session> CreateSession(
      SessionUpdateKeyStatusesCallback session_update_key_statuses_callback,
      SessionClosedCallback session_closed_callback);

  bool IsServerCertificateUpdatable();
  void UpdateServerCertificate(
      const uint8_t* certificate, int certificate_size,
      ServerCertificateUpdatedCallback server_certificate_updated_callback);
  bool GetMetrics(std::vector<uint8_t>* metrics);

 private:
  // Stores context of |GenerateSessionUpdateRequest|.
  struct SessionUpdateRequest {
    base::WeakPtr<Session> session;
    SessionUpdateRequestGeneratedCallback generated_callback;
    SessionUpdateRequestDidNotGenerateCallback did_not_generate_callback;
  };
  typedef base::hash_map<int, SessionUpdateRequest>
      TicketToSessionUpdateRequestMap;

  typedef base::hash_map<std::string, base::WeakPtr<Session>> IdToSessionMap;

  typedef base::hash_map<int, ServerCertificateUpdatedCallback>
      TicketToServerCertificateUpdatedMap;

  // Stores context of |Session::Update|.
  struct SessionUpdate {
    SessionUpdatedCallback updated_callback;
    SessionDidNotUpdateCallback did_not_update_callback;
  };
  typedef base::hash_map<int, SessionUpdate> TicketToSessionUpdateMap;

  // Defined to work around the limitation on number of parameters of
  // base::Bind().
  struct SessionTicketAndOptionalId {
    int ticket;
    base::Optional<std::string> id;
  };

  // Private API for |Session|.
  void GenerateSessionUpdateRequest(
      const base::WeakPtr<Session>& session, const std::string& type,
      const uint8_t* init_data, int init_data_length,
      const SessionUpdateRequestGeneratedCallback&
          session_update_request_generated_callback,
      const SessionUpdateRequestDidNotGenerateCallback&
          session_update_request_did_not_generate_callback);
  void UpdateSession(
      const std::string& session_id, const uint8_t* key, int key_length,
      const SessionUpdatedCallback& session_updated_callback,
      const SessionDidNotUpdateCallback& session_did_not_update_callback);
  void CloseSession(const std::string& session_id);

  // Called on the constructor thread, parameters are copied and owned by these
  // methods.
  void OnSessionUpdateRequestGenerated(
      SessionTicketAndOptionalId ticket_and_optional_id, SbDrmStatus status,
      SbDrmSessionRequestType type, const std::string& error_message,
      std::unique_ptr<uint8[]> message, int message_size);
  void OnSessionUpdated(int ticket, SbDrmStatus status,
                        const std::string& error_message);
  void OnSessionKeyStatusChanged(
      const std::string& session_id, const std::vector<std::string>& key_ids,
      const std::vector<SbDrmKeyStatus>& key_statuses);
  void OnServerCertificateUpdated(int ticket, SbDrmStatus status,
                                  const std::string& error_message);
  void OnSessionClosed(const std::string& session_id);

  // Called on any thread, parameters need to be copied immediately.
  static void OnSessionUpdateRequestGeneratedFunc(
      SbDrmSystem wrapped_drm_system, void* context, int ticket,
      SbDrmStatus status, SbDrmSessionRequestType type,
      const char* error_message, const void* session_id, int session_id_size,
      const void* content, int content_size, const char* url);
  static void OnSessionUpdatedFunc(SbDrmSystem wrapped_drm_system,
                                   void* context, int ticket,
                                   SbDrmStatus status,
                                   const char* error_message,
                                   const void* session_id,
                                   int session_id_length);

  static void OnSessionKeyStatusesChangedFunc(
      SbDrmSystem wrapped_drm_system, void* context, const void* session_id,
      int session_id_size, int number_of_keys, const SbDrmKeyId* key_ids,
      const SbDrmKeyStatus* key_statuses);

  static void OnSessionClosedFunc(SbDrmSystem wrapped_drm_system, void* context,
                                  const void* session_id, int session_id_size);

  static void OnServerCertificateUpdatedFunc(SbDrmSystem wrapped_drm_system,
                                             void* context, int ticket,
                                             SbDrmStatus status,
                                             const char* error_message);

  const SbDrmSystem wrapped_drm_system_;
  scoped_refptr<base::SingleThreadTaskRunner> const message_loop_;

  // Factory should only be used to create the initial weak pointer. All
  // subsequent weak pointers are created by copying the initial one. This is
  // required to keep weak pointers bound to the constructor thread.
  base::WeakPtrFactory<DrmSystem> weak_ptr_factory_;
  base::WeakPtr<DrmSystem> weak_this_;

  int next_ticket_ = 0;
  // Supports concurrent calls to |GenerateSessionUpdateRequest|.
  TicketToSessionUpdateRequestMap ticket_to_session_update_request_map_;

  // Supports spontaneous invocations of |SbDrmSessionUpdateRequestFunc|.
  IdToSessionMap id_to_session_map_;

  TicketToServerCertificateUpdatedMap ticket_to_server_certificate_updated_map_;

  // Supports concurrent calls to |Session::Update|.
  TicketToSessionUpdateMap ticket_to_session_update_map_;

  DISALLOW_COPY_AND_ASSIGN(DrmSystem);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DRM_SYSTEM_H_
