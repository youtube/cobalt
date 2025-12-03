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

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "starboard/android/shared/drm_session_id_mapper.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/media_drm_bridge.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/thread.h"
#include "starboard/shared/starboard/features.h"

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

namespace starboard {
namespace {

constexpr char kNoUrl[] = "";

DECLARE_INSTANCE_COUNTER(AndroidDrmSystem)

}  // namespace

DrmSystem::DrmSystem(
    std::string_view key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback)
    : Thread("DrmSystemThread"),
      key_system_(key_system),
      enable_app_provisioning_(
          features::FeatureList::IsEnabled(features::kEnableAppProvisioning)),
      context_(context),
      update_request_callback_(update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      hdcp_lost_(false),
      session_id_mapper_(enable_app_provisioning_
                             ? std::make_unique<DrmSessionIdMapper>()
                             : nullptr) {
  ON_INSTANCE_CREATED(AndroidDrmSystem);

  media_drm_bridge_ = std::make_unique<MediaDrmBridge>(
      base::raw_ref<MediaDrmBridge::Host>(*this), key_system_,
      enable_app_provisioning_);
  if (!media_drm_bridge_->is_valid()) {
    return;
  }
  SB_LOG(INFO) << "Creating DrmSystem: key_system=" << key_system
               << ", enable_app_provisioning="
               << to_string(enable_app_provisioning_);

  if (!enable_app_provisioning_) {
    Start();
  }
}

void DrmSystem::Run() {
  SB_CHECK(!enable_app_provisioning_);

  if (media_drm_bridge_->CreateMediaCryptoSession()) {
    created_media_crypto_session_.store(true);
  } else {
    SB_LOG(INFO) << "Could not create media crypto session";
    return;
  }

  std::lock_guard scoped_lock(mutex_);
  if (!deferred_session_update_requests_.empty()) {
    for (const auto& update_request : deferred_session_update_requests_) {
      update_request->Generate(media_drm_bridge_.get());
    }
    deferred_session_update_requests_.clear();
  }
}

DrmSystem::~DrmSystem() {
  ON_INSTANCE_RELEASED(AndroidDrmSystem);
  // |Thread| requires a call to |Join()|, regardless of whether |Start()| was
  // executed.
  Join();
}

DrmSystem::SessionUpdateRequest::SessionUpdateRequest(
    int ticket,
    std::string_view mime_type,
    std::string_view initialization_data)
    : ticket_(ticket), init_data_(initialization_data), mime_(mime_type) {}

int DrmSystem::SessionUpdateRequest::TakeTicket() {
  // NOTE: The same SessionUpdateRequest can be used multiple times if the
  // device requires multiple rounds of provisioning (See
  // http://b/419320804#comment1).
  //
  // When TakeTicket() is called for the first time, it returns a valid
  // ticket and sets 'ticket_' to `kSbDrmTicketInvalid`.
  //
  // Subsequent calls to TakeTicket() for the same request will return
  // `kSbDrmTicketInvalid`. This is expected behavior and means that
  // this is a "spontaneous" message for an existing session (i.e., a retry or
  // deferred license request) rather than an initial request.
  //
  // See StarboardCdm's logic for "spontaneous" message:
  // https://source.corp.google.com/h/github/youtube/cobalt/+/main:media/starboard/starboard_cdm.cc;l=336-345;drc=274beb9dd42dc6c7d7dd3ff9415a41e1393f0133
  return std::exchange(ticket_, kSbDrmTicketInvalid);
}

void DrmSystem::SessionUpdateRequest::Generate(
    const MediaDrmBridge* media_drm_bridge) const {
  SB_LOG(INFO) << __func__;
  SB_CHECK(media_drm_bridge);
  media_drm_bridge->CreateSession(ticket_, init_data_, mime_);
}

MediaDrmBridge::OperationResult
DrmSystem::SessionUpdateRequest::GenerateWithAppProvisioning(
    const MediaDrmBridge* media_drm_bridge) const {
  SB_LOG(INFO) << __func__;
  SB_CHECK(media_drm_bridge);
  return media_drm_bridge->CreateSessionWithAppProvisioning(ticket_, init_data_,
                                                            mime_);
}

void DrmSystem::GenerateSessionUpdateRequest(int ticket,
                                             const char* type,
                                             const void* initialization_data,
                                             int initialization_data_size) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  auto session_update_request = std::make_unique<SessionUpdateRequest>(
      ticket, type,
      std::string_view(static_cast<const char*>(initialization_data),
                       initialization_data_size));
  if (enable_app_provisioning_) {
    GenerateSessionUpdateRequestWithAppProvisioning(
        std::move(session_update_request));
    return;
  }

  SB_LOG(INFO) << __func__;
  if (created_media_crypto_session_.load()) {
    session_update_request->Generate(media_drm_bridge_.get());
  } else {
    // Defer generating the update request.
    std::lock_guard scoped_lock(mutex_);
    deferred_session_update_requests_.push_back(
        std::move(session_update_request));
  }
  // |update_request_callback_| will be called by Java calling into
  // |onSessionMessage|.
}

void DrmSystem::GenerateSessionUpdateRequestWithAppProvisioning(
    std::unique_ptr<SessionUpdateRequest> request) {
  SB_CHECK(enable_app_provisioning_);

  {
    std::lock_guard scoped_lock(mutex_);
    if (!deferred_session_update_requests_.empty()) {
      SB_LOG(INFO)
          << "A provisioning request is in progress. Defer this request.";
      deferred_session_update_requests_.push_back(std::move(request));
      return;
    }
  }

  SB_LOG(INFO) << __func__;
  // |update_request_callback_| will be called asynchronously by Java calling
  // into |OnSessionUpdate| or |OnProvisioningRequest|.
  MediaDrmBridge::OperationResult result =
      request->GenerateWithAppProvisioning(media_drm_bridge_.get());
  switch (result.status) {
    case DRM_OPERATION_STATUS_SUCCESS:
      created_media_crypto_session_ = true;
      return;
    case DRM_OPERATION_STATUS_NOT_PROVISIONED:
      SB_LOG(INFO) << "Device is not provisioned. Generating provision request";
      {
        std::lock_guard scoped_lock(mutex_);
        deferred_session_update_requests_.push_back(std::move(request));
      }
      OnProvisioningRequest(media_drm_bridge_->GenerateProvisionRequest());
      return;
    case DRM_OPERATION_STATUS_OPERATION_FAILED:
    default:
      SB_LOG(ERROR) << "GenerateWithAppProvisioning failed: " << result;
      return;
  }
}

void DrmSystem::UpdateSession(int ticket,
                              const void* key,
                              int key_size,
                              const void* session_id,
                              int session_id_size) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  if (enable_app_provisioning_) {
    UpdateSessionWithAppProvisioning(
        ticket, std::string_view(static_cast<const char*>(key), key_size),
        std::string_view(static_cast<const char*>(session_id),
                         session_id_size));
    return;
  }

  MediaDrmBridge::OperationResult result = media_drm_bridge_->UpdateSession(
      ticket, std::string_view(static_cast<const char*>(key), key_size),
      std::string_view(static_cast<const char*>(session_id), session_id_size));
  session_updated_callback_(
      this, context_, ticket,
      result.ok() ? kSbDrmStatusSuccess : kSbDrmStatusUnknownError,
      result.error_message.c_str(), session_id, session_id_size);
}

void DrmSystem::UpdateSessionWithAppProvisioning(int ticket,
                                                 std::string_view key,
                                                 std::string_view session_id) {
  SB_CHECK(enable_app_provisioning_);

  const auto media_drm_session_id =
      [this, &session_id]() -> std::optional<std::string> {
    std::lock_guard lock(mutex_);
    if (!deferred_session_update_requests_.empty()) {
      return std::nullopt;
    }
    return std::string(session_id_mapper_->GetMediaDrmSessionId(session_id));
  }();

  bool provisioning_ok = false;
  const MediaDrmBridge::OperationResult result = [this, ticket, key,
                                                  media_drm_session_id,
                                                  &provisioning_ok]() {
    if (!media_drm_session_id) {
      SB_LOG(INFO) << " >  Handles the given key as provision response.";
      auto result = media_drm_bridge_->ProvideProvisionResponse(key);
      if (result.ok()) {
        provisioning_ok = true;
      }
      return result;
    }

    return media_drm_bridge_->UpdateSession(ticket, key, *media_drm_session_id);
  }();

  SB_LOG_IF(ERROR, !result.ok()) << "UpdateSession failed: " << result;

  session_updated_callback_(
      this, context_, ticket,
      result.ok() ? kSbDrmStatusSuccess : kSbDrmStatusUnknownError,
      result.error_message.c_str(), session_id.data(), session_id.size());

  if (provisioning_ok) {
    HandlePendingRequests();
  }
}

void DrmSystem::HandlePendingRequests() {
  SB_LOG(INFO) << __func__;
  std::vector<std::unique_ptr<SessionUpdateRequest>> pending_requests;
  {
    std::lock_guard scoped_lock(mutex_);
    pending_requests.swap(deferred_session_update_requests_);
  }

  for (auto& request : pending_requests) {
    GenerateSessionUpdateRequestWithAppProvisioning(std::move(request));
  }
}

void DrmSystem::CloseSession(const void* session_id_data, int session_id_size) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  std::string session_id(static_cast<const char*>(session_id_data),
                         session_id_size);
  {
    std::lock_guard scoped_lock(mutex_);
    auto iter = cached_drm_key_ids_.find(session_id);
    if (iter != cached_drm_key_ids_.end()) {
      cached_drm_key_ids_.erase(iter);
    }
  }

  if (enable_app_provisioning_) {
    std::string media_drm_session_id = [this, &session_id] {
      std::lock_guard lock(mutex_);
      return std::string(session_id_mapper_->GetMediaDrmSessionId(session_id));
    }();
    if (media_drm_session_id.empty()) {
      // Skip closing the session because it's a provisioning session, which
      // doesn't have a corresponding session in MediaDrm.
      return;
    }
    media_drm_bridge_->CloseSession(media_drm_session_id);
    return;
  }

  media_drm_bridge_->CloseSession(session_id);
}

DrmSystem::DecryptStatus DrmSystem::Decrypt(InputBuffer* buffer) {
  SB_DCHECK(buffer);
  SB_DCHECK(buffer->drm_info());
  // The actual decryption will take place by calling |queueSecureInputBuffer|
  // in the decoders.  Our existence implies that there is enough information
  // to perform the decryption.
  // TODO: Returns kRetry when |UpdateSession| is not called at all to allow the
  //       player worker to handle the retry logic.
  return kSuccess;
}

const void* DrmSystem::GetMetrics(int* size) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  return media_drm_bridge_->GetMetrics(size);
}

void DrmSystem::OnSessionUpdate(int ticket,
                                SbDrmSessionRequestType request_type,
                                std::string_view session_id,
                                std::string_view content) {
  std::string_view eme_session_id;
  if (enable_app_provisioning_) {
    std::lock_guard lock(mutex_);
    if (session_id_mapper_->IsMediaDrmSessionIdForProvisioningRequired()) {
      session_id_mapper_->RegisterMediaDrmSessionIdForProvisioning(session_id);
    }
    eme_session_id = session_id_mapper_->GetEmeSessionId(session_id);
  } else {
    eme_session_id = session_id;
  }

  update_request_callback_(this, context_, ticket, kSbDrmStatusSuccess,
                           request_type, /*error_message=*/nullptr,
                           eme_session_id.data(), eme_session_id.size(),
                           content.data(), content.size(), kNoUrl);
}

void DrmSystem::OnProvisioningRequest(std::string_view content) {
  SB_CHECK(enable_app_provisioning_);

  SB_LOG(INFO) << __func__;
  std::string eme_session_id;
  int ticket;
  {
    std::lock_guard lock(mutex_);
    SB_CHECK(!deferred_session_update_requests_.empty())
        << "Provisioning request is sent, even though there is no pending "
           "session update request.";
    eme_session_id = session_id_mapper_->CreateOrGetBridgeEmeSessionId();
    SB_CHECK(!eme_session_id.empty());
    ticket = deferred_session_update_requests_.front()->TakeTicket();
  }

  SB_LOG(INFO) << "Return provision request using pending ticket=" << ticket;
  update_request_callback_(this, context_, ticket, kSbDrmStatusSuccess,
                           kSbDrmSessionRequestTypeIndividualizationRequest,
                           /*error_message=*/nullptr, eme_session_id.data(),
                           eme_session_id.size(), content.data(),
                           content.size(), kNoUrl);
}

void DrmSystem::OnKeyStatusChange(
    std::string_view session_id,
    const std::vector<SbDrmKeyId>& drm_key_ids,
    const std::vector<SbDrmKeyStatus>& drm_key_statuses) {
  SB_CHECK_EQ(drm_key_ids.size(), drm_key_statuses.size());

  std::string eme_session_id;
  if (enable_app_provisioning_) {
    std::lock_guard lock(mutex_);
    eme_session_id = session_id_mapper_->GetEmeSessionId(session_id);
  } else {
    eme_session_id = session_id;
  }

  {
    std::lock_guard scoped_lock(mutex_);
    if (cached_drm_key_ids_[eme_session_id] != drm_key_ids) {
      cached_drm_key_ids_[eme_session_id] = drm_key_ids;
      if (hdcp_lost_) {
        CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
        return;
      }
    }
  }

  key_statuses_changed_callback_(this, context_, eme_session_id.data(),
                                 eme_session_id.size(),
                                 static_cast<int>(drm_key_ids.size()),
                                 drm_key_ids.data(), drm_key_statuses.data());
}

void DrmSystem::OnInsufficientOutputProtection() {
  // HDCP has lost, update the statuses of all keys in all known sessions to be
  // restricted.
  std::lock_guard scoped_lock(mutex_);
  if (hdcp_lost_) {
    return;
  }
  hdcp_lost_ = true;
  CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
}

void DrmSystem::CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked() {
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

bool DrmSystem::IsReady() {
  return created_media_crypto_session_.load();
}

}  // namespace starboard
