// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/media/drm_system_platform.h"

namespace starboard {

// static
DrmSystemPlatform* DrmSystemPlatform::Create(
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  return new DrmSystemPlatform(
      context, update_request_callback, session_updated_callback,
      key_statuses_changed_callback, server_certificate_updated_callback,
      session_closed_callback);
}

DrmSystemPlatform::~DrmSystemPlatform() = default;

// static
bool DrmSystemPlatform::IsKeySystemSupported(const char* key_system) {
  return false;
}

// static
bool DrmSystemPlatform::IsSupported(SbDrmSystem drm_system) {
  return false;
}

// static
std::string DrmSystemPlatform::GetName() {
  return "";
}

// static
std::string DrmSystemPlatform::GetKeySystemName() {
  return "";
}

void DrmSystemPlatform::GenerateSessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {}

void DrmSystemPlatform::UpdateSession(int ticket,
                                      const void* key,
                                      int key_size,
                                      const void* session_id,
                                      int session_id_size) {}

void DrmSystemPlatform::CloseSession(const void* session_id,
                                     int session_id_size) {}

SbDrmSystemPrivate::DecryptStatus DrmSystemPlatform::Decrypt(
    InputBuffer* buffer) {
  return kFailure;
}

bool DrmSystemPlatform::IsServerCertificateUpdatable() {
  return false;
}

void DrmSystemPlatform::UpdateServerCertificate(int ticket,
                                                const void* certificate,
                                                int certificate_size) {}

const void* DrmSystemPlatform::GetMetrics(int* size) {
  return nullptr;
}

AVContentKey* DrmSystemPlatform::GetContentKey(const uint8_t* key_id,
                                               int key_id_size) {
  return nullptr;
}

void DrmSystemPlatform::OnOutputObscuredChanged(bool is_obscured) {}

DrmSystemPlatform::DrmSystemPlatform(
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback) {}

}  // namespace starboard
