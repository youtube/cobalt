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

#include "base/message_loop.h"
#include "cobalt/media/base/drm_system_client.h"
#include "starboard/drm.h"

#if SB_API_VERSION < 4
#error "Cobalt media stack requires Starboard 4 or above."
#endif  // SB_API_VERSION < 4

namespace cobalt {
namespace media {

// A C++ wrapper around |SbDrmSystem|.
//
// Ensures that calls to |DrmSystemClient| are always asynchronous and performed
// from the same thread where |DrmSystem| was instantiated.
class DrmSystem {
 public:
  DrmSystem(const char* key_system, DrmSystemClient* client);
  ~DrmSystem();

  SbDrmSystem wrapped_drm_system() { return wrapped_drm_system_; }

  // Wraps |SbDrmGenerateSessionUpdateRequest|.
  void GenerateSessionUpdateRequest(int ticket, const std::string& type,
                                    const uint8* init_data,
                                    int init_data_length);
  // Wraps |SbDrmUpdateSession|.
  void UpdateSession(int ticket, const std::string& session_id,
                     const uint8* key, int key_length);
  // Wraps |SbDrmCloseSession|.
  void CloseSession(const std::string& session_id);

 private:
  void OnSessionUpdateRequestGenerated(int ticket, const void* session_id,
                                       int session_id_size, const void* content,
                                       int content_size);
  void OnSessionUpdated(int ticket, const void* session_id, int session_id_size,
                        bool succeeded);

  static void OnSessionUpdateRequestGeneratedFunc(
      SbDrmSystem wrapped_drm_system, void* context, int ticket,
      const void* session_id, int session_id_size, const void* content,
      int content_size, const char* url);
  static void OnSessionUpdatedFunc(SbDrmSystem wrapped_drm_system,
                                   void* context, int ticket,
                                   const void* session_id,
                                   int session_id_length, bool succeeded);

  DrmSystemClient* const client_;
  const SbDrmSystem wrapped_drm_system_;
  MessageLoop* const message_loop_;

  DISALLOW_COPY_AND_ASSIGN(DrmSystem);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DRM_SYSTEM_H_
