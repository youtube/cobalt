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

#include "starboard/android/shared/drm_system.h"

#include <memory>
#include <utility>

#include "starboard/android/shared/media_common.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/thread.h"

namespace {

using starboard::android::shared::DrmSystem;

DECLARE_INSTANCE_COUNTER(AndroidDrmSystem)

}  // namespace

// Declare the function as static instead of putting it in the above anonymous
// namespace so it can be picked up by `std::vector<SbDrmKeyId>::operator==()`
// as functions in anonymous namespace doesn't participate in argument dependent
// lookup.
static bool operator==(const SbDrmKeyId& left, const SbDrmKeyId& right) {
  if (left.identifier_size != right.identifier_size) {
    return false;
  }
  return memcmp(left.identifier, right.identifier, left.identifier_size) == 0;
}

namespace starboard::android::shared {

DrmSystem::DrmSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback)
    : Thread("DrmSystemThread"),
      key_system_(key_system),
      context_(context),
      update_request_callback_(update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      hdcp_lost_(false) {
  ON_INSTANCE_CREATED(AndroidDrmSystem);

  media_drm_bridge_.reset(new MediaDrmBridge(this, key_system));
  if (!media_drm_bridge_->is_valid()) {
    return;
  }

  Start();
}

void DrmSystem::Run() {
  if (media_drm_bridge_->CreateMediaCryptoSession()) {
    created_media_crypto_session_.store(true);
  } else {
    SB_LOG(INFO) << "Could not create media crypto session";
    return;
  }

  ScopedLock scoped_lock(mutex_);
  if (!deferred_session_update_requests_.empty()) {
    for (const auto& update_request : deferred_session_update_requests_) {
      update_request->Generate(media_drm_bridge_.get());
    }
    deferred_session_update_requests_.clear();
  }
}

DrmSystem::~DrmSystem() {
  ON_INSTANCE_RELEASED(AndroidDrmSystem);
  Join();
}

DrmSystem::SessionUpdateRequest::SessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size)
    : ticket_(ticket),
      init_data_(static_cast<const uint8_t*>(initialization_data),
                 static_cast<const uint8_t*>(initialization_data) +
                     initialization_data_size),
      mime_(type) {}

void DrmSystem::SessionUpdateRequest::Generate(
    const MediaDrmBridge* media_drm_bridge) const {
  SB_DCHECK(media_drm_bridge);
  media_drm_bridge->CreateSession(ticket_, init_data_, mime_);
}

void DrmSystem::GenerateSessionUpdateRequest(int ticket,
                                             const char* type,
                                             const void* initialization_data,
                                             int initialization_data_size) {
  std::unique_ptr<SessionUpdateRequest> session_update_request(
      new SessionUpdateRequest(ticket, type, initialization_data,
                               initialization_data_size));
  if (created_media_crypto_session_.load()) {
    session_update_request->Generate(media_drm_bridge_.get());
  } else {
    // Defer generating the update request.
    ScopedLock scoped_lock(mutex_);
    deferred_session_update_requests_.push_back(
        std::move(session_update_request));
  }
  // |update_request_callback_| will be called by Java calling into
  // |onSessionMessage|.
}

void DrmSystem::UpdateSession(int ticket,
                              const void* key,
                              int key_size,
                              const void* session_id,
                              int session_id_size) {
  std::string error_msg;
  bool update_success = media_drm_bridge_->UpdateSession(
      ticket, key, key_size, session_id, session_id_size, &error_msg);
  session_updated_callback_(
      this, context_, ticket,
      update_success ? kSbDrmStatusSuccess : kSbDrmStatusUnknownError,
      error_msg.c_str(), session_id, session_id_size);
}

void DrmSystem::CloseSession(const void* session_id, int session_id_size) {
  std::string session_id_as_string(static_cast<const char*>(session_id),
                                   session_id_size);

  {
    ScopedLock scoped_lock(mutex_);
    auto iter = cached_drm_key_ids_.find(session_id_as_string);
    if (iter != cached_drm_key_ids_.end()) {
      cached_drm_key_ids_.erase(iter);
    }
  }
  media_drm_bridge_->CloseSession(session_id_as_string);
}

DrmSystem::DecryptStatus DrmSystem::Decrypt(InputBuffer* buffer) {
  SB_DCHECK(buffer);
  SB_DCHECK(buffer->drm_info());
  SB_DCHECK(media_drm_bridge_->GetMediaCrypto());
  // The actual decryption will take place by calling |queueSecureInputBuffer|
  // in the decoders.  Our existence implies that there is enough information
  // to perform the decryption.
  // TODO: Returns kRetry when |UpdateSession| is not called at all to allow the
  //       player worker to handle the retry logic.
  return kSuccess;
}

const void* DrmSystem::GetMetrics(int* size) {
  return media_drm_bridge_->GetMetrics(size);
}

void DrmSystem::CallUpdateRequestCallback(int ticket,
                                          SbDrmSessionRequestType request_type,
                                          const void* session_id,
                                          int session_id_size,
                                          const void* content,
                                          int content_size,
                                          const char* url) {
  update_request_callback_(this, context_, ticket, kSbDrmStatusSuccess,
                           request_type, nullptr, session_id, session_id_size,
                           content, content_size, url);
}

void DrmSystem::CallDrmSessionKeyStatusesChangedCallback(
    const void* session_id,
    int session_id_size,
    const std::vector<SbDrmKeyId>& drm_key_ids,
    const std::vector<SbDrmKeyStatus>& drm_key_statuses) {
  SB_DCHECK(drm_key_ids.size() == drm_key_statuses.size());

  std::string session_id_as_string(static_cast<const char*>(session_id),
                                   session_id_size);

  {
    ScopedLock scoped_lock(mutex_);
    if (cached_drm_key_ids_[session_id_as_string] != drm_key_ids) {
      cached_drm_key_ids_[session_id_as_string] = drm_key_ids;
      if (hdcp_lost_) {
        CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
        return;
      }
    }
  }

  key_statuses_changed_callback_(this, context_, session_id, session_id_size,
                                 static_cast<int>(drm_key_ids.size()),
                                 drm_key_ids.data(), drm_key_statuses.data());
}

void DrmSystem::OnInsufficientOutputProtection() {
  // HDCP has lost, update the statuses of all keys in all known sessions to be
  // restricted.
  ScopedLock scoped_lock(mutex_);
  if (hdcp_lost_) {
    return;
  }
  hdcp_lost_ = true;
  CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
}

void DrmSystem::CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked() {
  mutex_.DCheckAcquired();

  for (auto& iter : cached_drm_key_ids_) {
    const std::string& session_id = iter.first;
    const std::vector<SbDrmKeyId>& drm_key_ids = iter.second;
    std::vector<SbDrmKeyStatus> drm_key_statuses(drm_key_ids.size(),
                                                 kSbDrmKeyStatusRestricted);

    key_statuses_changed_callback_(this, context_, session_id.data(),
                                   session_id.size(),
                                   static_cast<int>(drm_key_ids.size()),
                                   drm_key_ids.data(), drm_key_statuses.data());
  }
}

}  // namespace starboard::android::shared
