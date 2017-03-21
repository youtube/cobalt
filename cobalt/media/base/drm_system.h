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

#ifndef COBALT_MEDIA_BASE_DRM_SYSTEM_H_
#define COBALT_MEDIA_BASE_DRM_SYSTEM_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/optional.h"
#include "starboard/drm.h"

#if SB_API_VERSION < 4
#error "Cobalt media stack requires Starboard 4 or above."
#endif  // SB_API_VERSION < 4

namespace cobalt {
namespace media {

// A C++ wrapper around |SbDrmSystem|.
//
// Ensures that callbacks are always asynchronous and performed
// from the same thread where |DrmSystem| was instantiated.
class DrmSystem {
 public:
  typedef base::Callback<void(scoped_array<uint8> message, int message_size)>
      SessionUpdateRequestGeneratedCallback;
  typedef base::Callback<void()> SessionUpdateRequestDidNotGenerateCallback;
  typedef base::Callback<void()> SessionUpdatedCallback;
  typedef base::Callback<void()> SessionDidNotUpdateCallback;

  // Flyweight that provides RAII semantics for sessions.
  // Most of logic is implemented by |DrmSystem|.
  class Session {
   public:
    ~Session();

    const base::optional<std::string>& id() const { return id_; }

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
    explicit Session(DrmSystem* drm_system);
    void set_id(const std::string& id) { id_ = id; }
    const SessionUpdateRequestGeneratedCallback&
    update_request_generated_callback() const {
      return update_request_generated_callback_;
    }

    DrmSystem* const drm_system_;
    bool closed_;
    base::optional<std::string> id_;
    // Supports spontaneous invocations of |SbDrmSessionUpdateRequestFunc|.
    SessionUpdateRequestGeneratedCallback update_request_generated_callback_;

    friend class DrmSystem;

    DISALLOW_COPY_AND_ASSIGN(Session);
  };

  explicit DrmSystem(const char* key_system);
  ~DrmSystem();

  SbDrmSystem wrapped_drm_system() { return wrapped_drm_system_; }

  scoped_ptr<Session> CreateSession();

 private:
  // Stores context of |GenerateSessionUpdateRequest|.
  struct SessionUpdateRequest {
    Session* session;
    SessionUpdateRequestGeneratedCallback generated_callback;
    SessionUpdateRequestDidNotGenerateCallback did_not_generate_callback;
  };
  typedef base::hash_map<int, SessionUpdateRequest>
      TicketToSessionUpdateRequestMap;

  typedef base::hash_map<std::string, Session*> IdToSessionMap;

  // Stores context of |Session::Update|.
  struct SessionUpdate {
    SessionUpdatedCallback updated_callback;
    SessionDidNotUpdateCallback did_not_update_callback;
  };
  typedef base::hash_map<int, SessionUpdate> TicketToSessionUpdateMap;

  // Private API for |Session|.
  void GenerateSessionUpdateRequest(
      Session* session, const std::string& type, const uint8_t* init_data,
      int init_data_length, const SessionUpdateRequestGeneratedCallback&
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
      int ticket, const base::optional<std::string>& session_id,
      scoped_array<uint8> message, int message_size);
  void OnSessionUpdated(int ticket, bool succeeded);

  // Called on any thread, parameters need to be copied immediately.
  static void OnSessionUpdateRequestGeneratedFunc(
      SbDrmSystem wrapped_drm_system, void* context, int ticket,
      const void* session_id, int session_id_size, const void* content,
      int content_size, const char* url);
  static void OnSessionUpdatedFunc(SbDrmSystem wrapped_drm_system,
                                   void* context, int ticket,
                                   const void* session_id,
                                   int session_id_length, bool succeeded);

  const SbDrmSystem wrapped_drm_system_;
  MessageLoop* const message_loop_;

  // Factory should only be used to create the initial weak pointer. All
  // subsequent weak pointers are created by copying the initial one. This is
  // required to keep weak pointers bound to the constructor thread.
  base::WeakPtrFactory<DrmSystem> weak_ptr_factory_;
  base::WeakPtr<DrmSystem> weak_this_;

  // Supports concurrent calls to |GenerateSessionUpdateRequest|.
  int next_session_update_request_ticket_;
  TicketToSessionUpdateRequestMap ticket_to_session_update_request_map_;

  // Supports spontaneous invocations of |SbDrmSessionUpdateRequestFunc|.
  IdToSessionMap id_to_session_map_;

  // Supports concurrent calls to |Session::Update|.
  int next_session_update_ticket_;
  TicketToSessionUpdateMap ticket_to_session_update_map_;

  DISALLOW_COPY_AND_ASSIGN(DrmSystem);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DRM_SYSTEM_H_
