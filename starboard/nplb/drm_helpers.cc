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

#include "starboard/nplb/drm_helpers.h"

#include "starboard/drm.h"

namespace starboard {
namespace nplb {

void DummySessionUpdateRequestFunc(SbDrmSystem drm_system,
                                   void* context,
                                   int ticket,
                                   SbDrmStatus status,
                                   SbDrmSessionRequestType type,
                                   const char* error_message,
                                   const void* session_id,
                                   int session_id_size,
                                   const void* content,
                                   int content_size,
                                   const char* url) {}

void DummySessionUpdatedFunc(SbDrmSystem drm_system,
                             void* context,
                             int ticket,
                             SbDrmStatus status,
                             const char* error_message,
                             const void* session_id,
                             int session_id_size) {}

void DummyServerCertificateUpdatedFunc(SbDrmSystem drm_system,
                                       void* context,
                                       int ticket,
                                       SbDrmStatus status,
                                       const char* error_message) {}

void DummySessionKeyStatusesChangedFunc(SbDrmSystem drm_system,
                                        void* context,
                                        const void* session_id,
                                        int session_id_size,
                                        int number_of_keys,
                                        const SbDrmKeyId* key_ids,
                                        const SbDrmKeyStatus* key_statuses) {}

void DummySessionClosedFunc(SbDrmSystem drm_system,
                            void* context,
                            const void* session_id,
                            int session_id_size) {}

SbDrmSystem CreateDummyDrmSystem(const char* key_system) {
  return SbDrmCreateSystem(
      key_system, NULL /* context */, DummySessionUpdateRequestFunc,
      DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc,
      DummyServerCertificateUpdatedFunc, DummySessionClosedFunc);
}

}  // namespace nplb
}  // namespace starboard
