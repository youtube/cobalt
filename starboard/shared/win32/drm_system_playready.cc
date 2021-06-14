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

#include "starboard/shared/win32/drm_system_playready.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

#include "starboard/common/instance_counter.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/once.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace {

const char kPlayReadyKeySystem[] = "com.youtube.playready";
const bool kLogPlayreadyChallengeResponse = false;

DECLARE_INSTANCE_COUNTER(DrmSystemPlayready);

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

class ActiveDrmSystems {
 public:
  ::starboard::Mutex mutex_;
  std::vector<starboard::shared::win32::DrmSystemPlayready*> active_systems_;
};

SB_ONCE_INITIALIZE_FUNCTION(ActiveDrmSystems, GetActiveDrmSystems);

}  // namespace

namespace starboard {
namespace shared {
namespace win32 {

void DrmSystemOnUwpResume() {
  ::starboard::ScopedLock lock(GetActiveDrmSystems()->mutex_);
  for (DrmSystemPlayready* item : GetActiveDrmSystems()->active_systems_) {
    item->OnUwpResume();
  }
}

DrmSystemPlayready::DrmSystemPlayready(
    void* context,
    EnableOutputProtectionFunc enable_output_protection_func,
    SbDrmSessionUpdateRequestFunc session_update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmSessionClosedFunc session_closed_callback)
    : context_(context),
      enable_output_protection_func_(enable_output_protection_func),
      session_update_request_callback_(session_update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      session_closed_callback_(session_closed_callback) {
  SB_DCHECK(enable_output_protection_func);
  SB_DCHECK(session_update_request_callback);
  SB_DCHECK(session_updated_callback);
  SB_DCHECK(key_statuses_changed_callback);
  SB_DCHECK(session_closed_callback);

  ON_INSTANCE_CREATED(DrmSystemPlayready);

  ScopedLock lock(GetActiveDrmSystems()->mutex_);
  GetActiveDrmSystems()->active_systems_.push_back(this);
}

DrmSystemPlayready::~DrmSystemPlayready() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  ON_INSTANCE_RELEASED(DrmSystemPlayready);

  ScopedLock lock(GetActiveDrmSystems()->mutex_);
  auto& active_systems = GetActiveDrmSystems()->active_systems_;
  active_systems.erase(std::remove(
      active_systems.begin(), active_systems.end(), this));
}

bool DrmSystemPlayready::IsKeySystemSupported(const char* key_system) {
  SB_DCHECK(key_system);

  // It is possible that the |key_system| comes with extra attributes, like
  // `com.youtube.playready; encryptionscheme="cenc"`.  We prepend "key_system/"
  // to it, so it can be parsed by MimeType.
  starboard::media::MimeType mime_type(std::string("key_system/") + key_system);

  if (!mime_type.is_valid()) {
    return false;
  }
  SB_DCHECK(mime_type.type() == "key_system");
  if (mime_type.subtype() != kPlayReadyKeySystem) {
    return false;
  }

  for (int i = 0; i < mime_type.GetParamCount(); ++i) {
    if (mime_type.GetParamName(i) == "encryptionscheme") {
      auto value = mime_type.GetParamStringValue(i);
      if (value != "cenc") {
        return false;
      }
    }
  }

  return true;
}

void DrmSystemPlayready::GenerateSessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (strcmp("cenc", type) != 0) {
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
    session_update_request_callback_(
        this, context_, ticket, kSbDrmStatusUnknownError,
        kSbDrmSessionRequestTypeLicenseRequest, NULL, NULL, 0, NULL, 0, NULL);
    return;
  }

  SB_LOG(INFO) << "Send challenge for key id "
               << GetHexRepresentation(license->key_id()) << " in session "
               << session_id;
  SB_LOG_IF(INFO, kLogPlayreadyChallengeResponse)
      << GetHexRepresentation(challenge.data(), challenge.size());

  session_update_request_callback_(
      this, context_, ticket, kSbDrmStatusSuccess,
      kSbDrmSessionRequestTypeLicenseRequest, NULL, session_id.c_str(),
      static_cast<int>(session_id.size()), challenge.c_str(),
      static_cast<int>(challenge.size()), NULL);
  pending_requests_[session_id] = license;
}

void DrmSystemPlayready::UpdateSession(int ticket,
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
      successful_requests_[iter->first] =
          LicenseInfo(kSbDrmKeyStatusUsable, license);
    }
    session_updated_callback_(this, context_, ticket, kSbDrmStatusSuccess, NULL,
                              session_id, session_id_size);

    {
      ScopedLock lock(mutex_);
      ReportKeyStatusChanged_Locked(session_id_copy);
    }
    pending_requests_.erase(iter);
  } else {
    SB_LOG(INFO) << "Failed to add key for session " << session_id_copy;
    // Don't report it as a failure as otherwise the JS player is going to
    // terminate the video.
    session_updated_callback_(this, context_, ticket, kSbDrmStatusSuccess, NULL,
                              session_id, session_id_size);
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

    // We have to use |kSbDrmTicketInvalid| as the license challenge is not a
    // result of GenerateSessionUpdateRequest().
    session_update_request_callback_(
        this, context_, kSbDrmTicketInvalid, kSbDrmStatusSuccess,
        kSbDrmSessionRequestTypeLicenseRequest, NULL, session_id,
        session_id_size, challenge.c_str(), static_cast<int>(challenge.size()),
        NULL);
  }
}

void DrmSystemPlayready::CloseSession(const void* session_id,
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

SbDrmSystemPrivate::DecryptStatus DrmSystemPlayready::Decrypt(
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
    if (item.second.license_->key_id() == key_id) {
      if (buffer->sample_type() == kSbMediaTypeAudio) {
        return kSuccess;
      }

      if (item.second.license_->IsHDCPRequired()) {
        if (!enable_output_protection_func_()) {
          SB_LOG(INFO) << "HDCP required but not available";
          item.second.status_ = kSbDrmKeyStatusRestricted;
          ReportKeyStatusChanged_Locked(item.first);
          return kRetry;
        }
      }

      return kSuccess;
    }
  }

  return kRetry;
}

void DrmSystemPlayready::ReportKeyStatusChanged_Locked(
    const std::string& session_id) {
  // mutex_ must be held by caller
  ::starboard::ScopedTryLock lock_should_fail(mutex_);
  SB_DCHECK(!lock_should_fail.is_locked());

  LicenseInfo& item = successful_requests_[session_id];

  GUID key_id = item.license_->key_id();
  SbDrmKeyId drm_key_id;
  SB_DCHECK(sizeof(drm_key_id.identifier) >= sizeof(key_id));
  memcpy(&(drm_key_id.identifier), &key_id, sizeof(key_id));
  drm_key_id.identifier_size = sizeof(key_id);

  key_statuses_changed_callback_(this, context_,
      session_id.data(), static_cast<int>(session_id.size()),
      1, &drm_key_id, &(item.status_));
}

scoped_refptr<DrmSystemPlayready::License> DrmSystemPlayready::GetLicense(
    const uint8_t* key_id,
    int key_id_size) {
  GUID key_id_copy;
  if (key_id_size != sizeof(key_id_copy)) {
    return NULL;
  }
  key_id_copy = *reinterpret_cast<const GUID*>(key_id);

  ScopedLock lock(mutex_);

  for (auto& item : successful_requests_) {
    if (item.second.license_->key_id() == key_id_copy) {
      return item.second.license_;
    }
  }

  return NULL;
}

void DrmSystemPlayready::OnUwpResume() {
  for (auto& item : successful_requests_) {
    session_closed_callback_(this, context_,
        item.first.data(), static_cast<int>(item.first.size()));
  }
}

std::string DrmSystemPlayready::GenerateAndAdvanceSessionId() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  std::stringstream ss;
  ss << current_session_id_;
  ++current_session_id_;
  return ss.str();
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
