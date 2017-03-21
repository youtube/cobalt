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

#include "cobalt/media/base/drm_system.h"

#include "base/bind.h"
#include "base/logging.h"

namespace cobalt {
namespace media {

DrmSystem::Session::Session(DrmSystem* drm_system)
    : drm_system_(drm_system), closed_(false) {}

DrmSystem::Session::~Session() {
  if (id_ && !closed_) {
    // Auto-closing semantics is derived from EME spec.
    //
    // If a MediaKeySession object is not closed when it becomes inaccessible
    // to the page, the CDM shall close the key session associated with
    // the object.
    //   https://www.w3.org/TR/encrypted-media/#mediakeysession-interface
    Close();
  }
}

void DrmSystem::Session::GenerateUpdateRequest(
    const std::string& type, const uint8* init_data, int init_data_length,
    const SessionUpdateRequestGeneratedCallback&
        session_update_request_generated_callback,
    const SessionUpdateRequestDidNotGenerateCallback&
        session_update_request_did_not_generate_callback) {
  update_request_generated_callback_ =
      session_update_request_generated_callback;
  drm_system_->GenerateSessionUpdateRequest(
      this, type, init_data, init_data_length,
      session_update_request_generated_callback,
      session_update_request_did_not_generate_callback);
}

void DrmSystem::Session::Update(
    const uint8* key, int key_length,
    const SessionUpdatedCallback& session_updated_callback,
    const SessionDidNotUpdateCallback& session_did_not_update_callback) {
  drm_system_->UpdateSession(*id_, key, key_length, session_updated_callback,
                             session_did_not_update_callback);
}

void DrmSystem::Session::Close() {
  drm_system_->CloseSession(*id_);
  closed_ = true;
}

DrmSystem::DrmSystem(const char* key_system)
    : wrapped_drm_system_(SbDrmCreateSystem(key_system, this,
                                            OnSessionUpdateRequestGeneratedFunc,
                                            OnSessionUpdatedFunc)),
      message_loop_(MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()),
      next_session_update_request_ticket_(0),
      next_session_update_ticket_(0) {
  DCHECK_NE(kSbDrmSystemInvalid, wrapped_drm_system_);
}

DrmSystem::~DrmSystem() { SbDrmDestroySystem(wrapped_drm_system_); }

scoped_ptr<DrmSystem::Session> DrmSystem::CreateSession() {
  return make_scoped_ptr(new Session(this));
}

void DrmSystem::GenerateSessionUpdateRequest(
    Session* session, const std::string& type, const uint8_t* init_data,
    int init_data_length, const SessionUpdateRequestGeneratedCallback&
                              session_update_request_generated_callback,
    const SessionUpdateRequestDidNotGenerateCallback&
        session_update_request_did_not_generate_callback) {
  // Store the context of the call.
  SessionUpdateRequest session_update_request;
  session_update_request.session = session;
  session_update_request.generated_callback =
      session_update_request_generated_callback;
  session_update_request.did_not_generate_callback =
      session_update_request_did_not_generate_callback;
  int ticket = next_session_update_request_ticket_++;
  ticket_to_session_update_request_map_.insert(
      std::make_pair(ticket, session_update_request));

  SbDrmGenerateSessionUpdateRequest(wrapped_drm_system_, ticket, type.c_str(),
                                    init_data, init_data_length);
}

void DrmSystem::UpdateSession(
    const std::string& session_id, const uint8_t* key, int key_length,
    const SessionUpdatedCallback& session_updated_callback,
    const SessionDidNotUpdateCallback& session_did_not_update_callback) {
  // Store the context of the call.
  SessionUpdate session_update;
  session_update.updated_callback = session_updated_callback;
  session_update.did_not_update_callback = session_did_not_update_callback;
  int ticket = next_session_update_ticket_++;
  ticket_to_session_update_map_.insert(std::make_pair(ticket, session_update));

  SbDrmUpdateSession(wrapped_drm_system_, ticket, key, key_length,
                     session_id.c_str(), session_id.size());
}

void DrmSystem::CloseSession(const std::string& session_id) {
  id_to_session_map_.erase(session_id);
  SbDrmCloseSession(wrapped_drm_system_, session_id.c_str(), session_id.size());
}

void DrmSystem::OnSessionUpdateRequestGenerated(
    int ticket, const base::optional<std::string>& session_id,
    scoped_array<uint8> message, int message_size) {
  if (SbDrmTicketIsValid(ticket)) {
    // Called back as a result of |SbDrmGenerateSessionUpdateRequest|.

    // Restore the context of |GenerateSessionUpdateRequest|.
    TicketToSessionUpdateRequestMap::iterator session_update_request_iterator =
        ticket_to_session_update_request_map_.find(ticket);
    if (session_update_request_iterator ==
        ticket_to_session_update_request_map_.end()) {
      LOG(ERROR) << "Unknown session update request ticket: " << ticket << ".";
      return;
    }
    const SessionUpdateRequest& session_update_request =
        session_update_request_iterator->second;

    // Interpret the result.
    if (session_id) {
      // Successful request generation.

      // Enable session lookup by id which is used by spontaneous callbacks.
      session_update_request.session->set_id(*session_id);
      id_to_session_map_.insert(
          std::make_pair(*session_id, session_update_request.session));

      session_update_request.generated_callback.Run(message.Pass(),
                                                    message_size);
    } else {
      // Failure during request generation.
      session_update_request.did_not_generate_callback.Run();
    }

    // Sweep the context of |GenerateSessionUpdateRequest|.
    ticket_to_session_update_request_map_.erase(
        session_update_request_iterator);
  } else {
    // Called back spontaneously by the underlying DRM system.

    // Find the session by ID.
    IdToSessionMap::iterator session_iterator =
        id_to_session_map_.find(*session_id);
    if (session_iterator == id_to_session_map_.end()) {
      LOG(ERROR) << "Unknown session id: " << *session_id << ".";
      return;
    }
    Session* session = session_iterator->second;

    session->update_request_generated_callback().Run(message.Pass(),
                                                     message_size);
  }
}

void DrmSystem::OnSessionUpdated(int ticket, bool succeeded) {
  // Restore the context of |UpdateSession|.
  TicketToSessionUpdateMap::iterator session_update_iterator =
      ticket_to_session_update_map_.find(ticket);
  if (session_update_iterator == ticket_to_session_update_map_.end()) {
    LOG(ERROR) << "Unknown session update ticket: " << ticket << ".";
    return;
  }
  const SessionUpdate& session_update = session_update_iterator->second;

  // Interpret the result.
  if (succeeded) {
    session_update.updated_callback.Run();
  } else {
    session_update.did_not_update_callback.Run();
  }

  // Sweep the context of |UpdateSession|.
  ticket_to_session_update_map_.erase(session_update_iterator);
}

// static
void DrmSystem::OnSessionUpdateRequestGeneratedFunc(
    SbDrmSystem wrapped_drm_system, void* context, int ticket,
    const void* session_id, int session_id_size, const void* content,
    int content_size, const char* url) {
  DCHECK(context);
  DrmSystem* drm_system = static_cast<DrmSystem*>(context);
  DCHECK_EQ(wrapped_drm_system, drm_system->wrapped_drm_system_);

  base::optional<std::string> session_id_copy;
  scoped_array<uint8> content_copy;
  if (session_id) {
    session_id_copy =
        std::string(static_cast<const char*>(session_id),
                    static_cast<const char*>(session_id) + session_id_size);

    content_copy.reset(new uint8[content_size]);
    SbMemoryCopy(content_copy.get(), content, content_size);
  }

  drm_system->message_loop_->PostTask(
      FROM_HERE, base::Bind(&DrmSystem::OnSessionUpdateRequestGenerated,
                            drm_system->weak_this_, ticket, session_id_copy,
                            base::Passed(&content_copy), content_size));
}

// static
void DrmSystem::OnSessionUpdatedFunc(SbDrmSystem wrapped_drm_system,
                                     void* context, int ticket,
                                     const void* /*session_id*/,
                                     int /*session_id_size*/, bool succeeded) {
  DCHECK(context);
  DrmSystem* drm_system = static_cast<DrmSystem*>(context);
  DCHECK_EQ(wrapped_drm_system, drm_system->wrapped_drm_system_);

  drm_system->message_loop_->PostTask(
      FROM_HERE, base::Bind(&DrmSystem::OnSessionUpdated,
                            drm_system->weak_this_, ticket, succeeded));
}

}  // namespace media
}  // namespace cobalt
