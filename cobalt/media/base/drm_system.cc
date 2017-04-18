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

DrmSystem::DrmSystem(const char* key_system, DrmSystemClient* client)
    : client_(client),
      wrapped_drm_system_(SbDrmCreateSystem(key_system, this,
                                            OnSessionUpdateRequestGeneratedFunc,
                                            OnSessionUpdatedFunc)),
      message_loop_(MessageLoop::current()) {
  DCHECK_NE(kSbDrmSystemInvalid, wrapped_drm_system_);
}

DrmSystem::~DrmSystem() { SbDrmDestroySystem(wrapped_drm_system_); }

void DrmSystem::GenerateSessionUpdateRequest(int ticket,
                                             const std::string& type,
                                             const uint8_t* init_data,
                                             int init_data_length) {
  SbDrmGenerateSessionUpdateRequest(wrapped_drm_system_, ticket, type.c_str(),
                                    init_data, init_data_length);
}

void DrmSystem::UpdateSession(int ticket, const std::string& session_id,
                              const uint8_t* key, int key_length) {
  SbDrmUpdateSession(wrapped_drm_system_, ticket, key, key_length,
                     session_id.c_str(), session_id.size());
}

void DrmSystem::CloseSession(const std::string& session_id) {
  SbDrmCloseSession(wrapped_drm_system_, session_id.c_str(), session_id.size());
}

void DrmSystem::OnSessionUpdateRequestGenerated(int ticket,
                                                const void* session_id,
                                                int session_id_size,
                                                const void* content,
                                                int content_size) {
  if (session_id) {
    std::string session_id_copy(
        static_cast<const char*>(session_id),
        static_cast<const char*>(session_id) + session_id_size);

    scoped_array<uint8> content_copy(new uint8[content_size]);
    SbMemoryCopy(content_copy.get(), content, content_size);

    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&DrmSystemClient::OnSessionUpdateRequestGenerated,
                   base::Unretained(client_), ticket, session_id_copy,
                   base::Passed(&content_copy), content_size));
  } else {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&DrmSystemClient::OnSessionUpdateRequestDidNotGenerate,
                   base::Unretained(client_), ticket));
  }
}

void DrmSystem::OnSessionUpdated(int ticket, const void* /*session_id*/,
                                 int /*session_id_size*/, bool succeeded) {
  if (succeeded) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&DrmSystemClient::OnSessionUpdated,
                                       base::Unretained(client_), ticket));
  } else {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&DrmSystemClient::OnSessionDidNotUpdate,
                                       base::Unretained(client_), ticket));
  }
}

// static
void DrmSystem::OnSessionUpdateRequestGeneratedFunc(
    SbDrmSystem wrapped_drm_system, void* context, int ticket,
    const void* session_id, int session_id_size, const void* content,
    int content_size, const char* url) {
  DCHECK(context);
  DrmSystem* drm_system = static_cast<DrmSystem*>(context);
  DCHECK_EQ(wrapped_drm_system, drm_system->wrapped_drm_system_);

  drm_system->OnSessionUpdateRequestGenerated(
      ticket, session_id, session_id_size, content, content_size);
}

// static
void DrmSystem::OnSessionUpdatedFunc(SbDrmSystem wrapped_drm_system,
                                     void* context, int ticket,
                                     const void* session_id,
                                     int session_id_size, bool succeeded) {
  DCHECK(context);
  DrmSystem* drm_system = static_cast<DrmSystem*>(context);
  DCHECK_EQ(wrapped_drm_system, drm_system->wrapped_drm_system_);

  drm_system->OnSessionUpdated(ticket, session_id, session_id_size, succeeded);
}

}  // namespace media
}  // namespace cobalt
