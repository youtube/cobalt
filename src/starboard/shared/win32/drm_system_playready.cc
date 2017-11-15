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

#include "starboard/shared/win32/drm_system_playready.h"

#include <cctype>
#include <sstream>

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

const bool kLogPlayreadyChallengeResponse = false;

std::string GetHexRepresentation(const void* data, size_t size) {
  const char kHex[] = "0123456789ABCDEF";

  std::stringstream representation;
  std::stringstream ascii;
  const uint8_t* binary = static_cast<const uint8_t*>(data);
  bool new_line = true;
  for (size_t i = 0; i < size; ++i) {
    if (new_line) {
      new_line = false;
    } else {
      representation << ' ';
    }
    ascii << (std::isprint(binary[i]) ? static_cast<char>(binary[i]) : '?');
    representation << kHex[binary[i] / 16] << kHex[binary[i] % 16];
    if (i % 16 == 15 && i != size - 1) {
      representation << " (" << ascii.str() << ')' << std::endl;
      std::stringstream empty;
      ascii.swap(empty);  // Clear the ascii stream
      new_line = true;
    }
  }

  if (!ascii.str().empty()) {
    representation << '(' << ascii.str() << ')' << std::endl;
  }

  return representation.str();
}

template <typename T>
std::string GetHexRepresentation(const T& value) {
  return GetHexRepresentation(&value, sizeof(T));
}

}  // namespace

SbDrmSystemPlayready::SbDrmSystemPlayready(
    void* context,
    SbDrmSessionUpdateRequestFunc session_update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback)
    : context_(context),
      session_update_request_callback_(session_update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      current_session_id_(1) {
  SB_DCHECK(session_update_request_callback);
  SB_DCHECK(session_updated_callback);
  SB_DCHECK(key_statuses_changed_callback);
}

SbDrmSystemPlayready::~SbDrmSystemPlayready() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
}

void SbDrmSystemPlayready::GenerateSessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (SbStringCompareAll("cenc", type) != 0) {
    SB_NOTREACHED() << "Invalid initialization data type " << type;
    return;
  }

  std::string session_id = GenerateAndAdvanceSessionId();
  scoped_refptr<License> license =
      License::Create(initialization_data, initialization_data_size);
  const std::string& challenge = license->license_challenge();
  if (challenge.empty()) {
    // Signal an error with |session_id| as NULL.
    SB_LOG(ERROR) << "Failed to generate license challenge";
    session_update_request_callback_(this, context_, ticket, NULL, 0, NULL, 0,
                                     NULL);
    return;
  }

  SB_LOG(INFO) << "Send challenge for key id "
               << GetHexRepresentation(license->key_id()) << " in session "
               << session_id;
  SB_LOG_IF(INFO, kLogPlayreadyChallengeResponse)
      << GetHexRepresentation(challenge.data(), challenge.size());

  session_update_request_callback_(this, context_, ticket, session_id.c_str(),
                                   static_cast<int>(session_id.size()),
                                   challenge.c_str(),
                                   static_cast<int>(challenge.size()), NULL);
  pending_requests_[session_id] = license;
}

void SbDrmSystemPlayready::UpdateSession(int ticket,
                                         const void* key,
                                         int key_size,
                                         const void* session_id,
                                         int session_id_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  std::string session_id_copy(static_cast<const char*>(session_id),
                              session_id_size);
  auto iter = pending_requests_.find(session_id_copy);
  SB_DCHECK(iter != pending_requests_.end());
  if (iter == pending_requests_.end()) {
    SB_NOTREACHED() << "Invalid session id " << session_id_copy;
    return;
  }

  scoped_refptr<License> license = iter->second;

  SB_LOG(INFO) << "Adding playready response for key id "
               << GetHexRepresentation(license->key_id());
  SB_LOG_IF(INFO, kLogPlayreadyChallengeResponse)
      << GetHexRepresentation(key, key_size);

  license->UpdateLicense(key, key_size);

  if (license->usable()) {
    SB_LOG(INFO) << "Successfully add key for key id "
                 << GetHexRepresentation(license->key_id()) << " in session "
                 << session_id_copy;
    {
      ScopedLock lock(mutex_);
      successful_requests_[iter->first] = license;
    }
    session_updated_callback_(this, context_, ticket, session_id,
                              session_id_size, true);

    GUID key_id = license->key_id();
    SbDrmKeyId drm_key_id;
    SB_DCHECK(sizeof(drm_key_id.identifier) >= sizeof(key_id));
    SbMemoryCopy(&drm_key_id, &key_id, sizeof(key_id));
    drm_key_id.identifier_size = sizeof(key_id);
    SbDrmKeyStatus key_status = kSbDrmKeyStatusUsable;
    key_statuses_changed_callback_(this, context_, session_id, session_id_size,
                                   1, &drm_key_id, &key_status);
    pending_requests_.erase(iter);
  } else {
    SB_LOG(INFO) << "Failed to add key for session " << session_id_copy;
    // Don't report it as a failure as otherwise the JS player is going to
    // terminate the video.
    session_updated_callback_(this, context_, ticket, session_id,
                              session_id_size, true);
    // When UpdateLicense() fails, the |license| must have generated a new
    // challenge internally.  Send this challenge again.
    const std::string& challenge = license->license_challenge();
    if (challenge.empty()) {
      SB_NOTREACHED();
      return;
    }

    SB_LOG(INFO) << "Send challenge again for key id "
                 << GetHexRepresentation(license->key_id()) << " in session "
                 << session_id;
    SB_LOG_IF(INFO, kLogPlayreadyChallengeResponse)
        << GetHexRepresentation(challenge.data(), challenge.size());

    // We have use |kSbDrmTicketInvalid| as the license challenge is not a
    // result of GenerateSessionUpdateRequest().
    session_update_request_callback_(
        this, context_, kSbDrmTicketInvalid, session_id, session_id_size,
        challenge.c_str(), static_cast<int>(challenge.size()), NULL);
  }
}

void SbDrmSystemPlayready::CloseSession(const void* session_id,
                                        int session_id_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  key_statuses_changed_callback_(this, context_, session_id, session_id_size, 0,
                                 nullptr, nullptr);

  std::string session_id_copy(static_cast<const char*>(session_id),
                              session_id_size);
  pending_requests_.erase(session_id_copy);

  ScopedLock lock(mutex_);
  successful_requests_.erase(session_id_copy);
}

SbDrmSystemPrivate::DecryptStatus SbDrmSystemPlayready::Decrypt(
    InputBuffer* buffer) {
  const SbDrmSampleInfo* drm_info = buffer->drm_info();

  if (drm_info == NULL || drm_info->initialization_vector_size == 0) {
    return kSuccess;
  }

  GUID key_id;
  if (drm_info->identifier_size != sizeof(key_id)) {
    return kRetry;
  }
  key_id = *reinterpret_cast<const GUID*>(drm_info->identifier);

  ScopedLock lock(mutex_);
  for (auto& item : successful_requests_) {
    if (item.second->key_id() == key_id) {
      if (buffer->sample_type() == kSbMediaTypeAudio) {
        return kSuccess;
      }

      if (item.second->IsHDCPRequired()) {
        if (!SbMediaSetOutputProtection(true)) {
          return kFailure;
        }
      }

      return kSuccess;
    }
  }

  return kRetry;
}

scoped_refptr<SbDrmSystemPlayready::License> SbDrmSystemPlayready::GetLicense(
    const uint8_t* key_id,
    int key_id_size) {
  GUID key_id_copy;
  if (key_id_size != sizeof(key_id_copy)) {
    return NULL;
  }
  key_id_copy = *reinterpret_cast<const GUID*>(key_id);

  ScopedLock lock(mutex_);

  for (auto& item : successful_requests_) {
    if (item.second->key_id() == key_id_copy) {
      return item.second;
    }
  }

  return NULL;
}

std::string SbDrmSystemPlayready::GenerateAndAdvanceSessionId() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  std::stringstream ss;
  ss << current_session_id_;
  ++current_session_id_;
  return ss.str();
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
