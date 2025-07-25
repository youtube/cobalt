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

#include <string>
#include <string_view>

#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/media_drm_bridge.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/thread.h"
#include "starboard/shared/starboard/media/drm_session_id_mapper.h"

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
namespace {
using starboard::android::shared::DrmSystem;

// TODO: b/79941850 - Use base::Feature instead for the experimentation.
constexpr bool kEnableAppProvisioning = false;

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
      context_(context),
      update_request_callback_(update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      hdcp_lost_(false),
      session_id_mapper_(
          kEnableAppProvisioning
              ? std::make_unique<
                    starboard::shared::starboard::media::DrmSessionIdMapper>()
              : nullptr) {
  ON_INSTANCE_CREATED(AndroidDrmSystem);

  media_drm_bridge_ = std::make_unique<MediaDrmBridge>(
      base::raw_ref<MediaDrmBridge::Host>(*this), key_system_,
      kEnableAppProvisioning);
  if (!media_drm_bridge_->is_valid()) {
    return;
  }
  SB_LOG(INFO) << "Creating DrmSystem: key_system=" << key_system
               << ", enable_app_provisioning="
               << (kEnableAppProvisioning ? "true" : "false");

  Start();
}

void DrmSystem::Run() {
  if (!kEnableAppProvisioning) {
    InitializeMediaCryptoSession();
    return;
  }

  job_queue_ =
      std::make_unique<starboard::shared::starboard::player::JobQueue>();
  job_queue_->RunUntilStopped();

  // job_queue_ should be destroyed at the same thread that it was created.
  job_queue_.reset();
}

void DrmSystem::InitializeMediaCryptoSession() {
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
  if (job_queue_ != nullptr) {
    job_queue_->StopSoon();
  }
  Join();
  SB_DCHECK_EQ(job_queue_, nullptr);
}

DrmSystem::SessionUpdateRequest::SessionUpdateRequest(
    int ticket,
    std::string_view mime_type,
    std::string_view initialization_data)
    : ticket_(ticket), init_data_(initialization_data), mime_(mime_type) {}

DrmSystem::SessionUpdateRequest
DrmSystem::SessionUpdateRequest::CloneWithoutTicket() const {
  return DrmSystem::SessionUpdateRequest(kSbDrmTicketInvalid, mime_,
                                         init_data_);
}

void DrmSystem::SessionUpdateRequest::Generate(
    const MediaDrmBridge* media_drm_bridge) const {
  SB_LOG(INFO) << __func__;
  SB_DCHECK(media_drm_bridge);
  media_drm_bridge->CreateSession(ticket_, init_data_, mime_);
}

MediaDrmBridge::OperationResult
DrmSystem::SessionUpdateRequest::GenerateWithAppProvisioning(
    const MediaDrmBridge* media_drm_bridge) const {
  SB_CHECK(kEnableAppProvisioning);

  SB_LOG(INFO) << __func__;
  SB_DCHECK(media_drm_bridge);
  return media_drm_bridge->CreateSessionWithAppProvisioning(ticket_, init_data_,
                                                            mime_);
}

void DrmSystem::GenerateSessionUpdateRequest(int ticket,
                                             const char* type,
                                             const void* initialization_data,
                                             int initialization_data_size) {
  auto session_update_request = std::make_unique<SessionUpdateRequest>(
      ticket, type,
      std::string_view(static_cast<const char*>(initialization_data),
                       initialization_data_size));
  if (kEnableAppProvisioning) {
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
  SB_CHECK(kEnableAppProvisioning);

  SB_LOG(INFO) << __func__;
  MediaDrmBridge::OperationResult status =
      request->GenerateWithAppProvisioning(media_drm_bridge_.get());
  switch (status.status) {
    case DRM_OPERATION_STATUS_SUCCESS:
      return;
    case DRM_OPERATION_STATUS_NOT_PROVISIONED:
      SB_LOG(INFO) << "Device is not provisioned. Generating provision request";
      {
        std::lock_guard scoped_lock(mutex_);
        pending_ticket_ = request->ticket();
        deferred_session_update_requests_.push_back(
            std::make_unique<SessionUpdateRequest>(
                request->CloneWithoutTicket()));
      }
      media_drm_bridge_->GenerateProvisionRequest();
      return;
    case DRM_OPERATION_STATUS_OPERATION_FAILED:
    default:
      SB_LOG(ERROR) << "GenerateWithAppProvisioning failed: " << status;
      return;
  }
}

void DrmSystem::UpdateSession(int ticket,
                              const void* key,
                              int key_size,
                              const void* session_id,
                              int session_id_size) {
  if (kEnableAppProvisioning) {
    UpdateSessionWithAppProvisioning(ticket, key, key_size, session_id,
                                     session_id_size);
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
                                                 const void* key,
                                                 int key_size,
                                                 const void* session_id,
                                                 int session_id_size) {
  SB_CHECK(kEnableAppProvisioning);
  const std::string_view key_view(static_cast<const char*>(key), key_size);
  const std::string_view cdm_session_id(static_cast<const char*>(session_id),
                                        session_id_size);

  std::string_view media_drm_session_id;
  {
    std::lock_guard lock(mutex_);
    media_drm_session_id =
        session_id_mapper_->GetMediaDrmSessionId(cdm_session_id);
  }

  MediaDrmBridge::OperationResult result;
  if (media_drm_session_id.empty()) {
    SB_LOG(INFO) << __func__
                 << "Handle the given key as provision response, since "
                    "MediaDrm session is not created yet for "
                 << cdm_session_id;
    result = media_drm_bridge_->ProvideProvisionResponse(key_view);
  } else {
    result = media_drm_bridge_->UpdateSession(ticket, key_view,
                                              media_drm_session_id);
  }

  if (!result.ok()) {
    SB_LOG(ERROR) << "UpdateSession failed: " << result;
  }

  session_updated_callback_(
      this, context_, ticket,
      result.ok() ? kSbDrmStatusSuccess : kSbDrmStatusUnknownError,
      result.error_message.c_str(), cdm_session_id.data(),
      cdm_session_id.size());

  if (result.ok()) {
    job_queue_->Schedule([this] { HandlePendingRequests(); });
  }
}

void DrmSystem::HandlePendingRequests() {
  SB_LOG(INFO) << __func__;
  std::unique_ptr<SessionUpdateRequest> request;
  {
    std::lock_guard scoped_lock(mutex_);
    if (deferred_session_update_requests_.empty()) {
      return;
    }

    request = std::move(deferred_session_update_requests_.front());
    deferred_session_update_requests_.erase(
        deferred_session_update_requests_.begin());
  }

  GenerateSessionUpdateRequestWithAppProvisioning(std::move(request));
}

void DrmSystem::CloseSession(const void* session_id, int session_id_size) {
  std::string session_id_as_string(static_cast<const char*>(session_id),
                                   session_id_size);
  {
    std::lock_guard scoped_lock(mutex_);
    auto iter = cached_drm_key_ids_.find(session_id_as_string);
    if (iter != cached_drm_key_ids_.end()) {
      cached_drm_key_ids_.erase(iter);
    }
  }

  std::string media_drm_session_id;
  if (kEnableAppProvisioning) {
    std::lock_guard lock(mutex_);
    media_drm_session_id =
        session_id_mapper_->GetMediaDrmSessionId(session_id_as_string);
  } else {
    media_drm_session_id = session_id_as_string;
  }

  if (media_drm_session_id.empty()) {
    // There is no MediaDrmSession to close.
    return;
  }
  media_drm_bridge_->CloseSession(media_drm_session_id);
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
  return media_drm_bridge_->GetMetrics(size);
}

void DrmSystem::OnSessionUpdate(int ticket,
                                SbDrmSessionRequestType request_type,
                                std::string_view session_id,
                                std::string_view content) {
  std::string cdm_session_id_str;
  std::string_view cdm_session_id;
  if (kEnableAppProvisioning) {
    std::lock_guard lock(mutex_);
    session_id_mapper_->RegisterMediaDrmSessionIdIfNotSet(session_id);
    cdm_session_id_str = session_id_mapper_->GetCdmSessionId(session_id);
    cdm_session_id = cdm_session_id_str;
  } else {
    cdm_session_id = session_id;
  }

  update_request_callback_(this, context_, ticket, kSbDrmStatusSuccess,
                           request_type, /*error_message=*/nullptr,
                           cdm_session_id.data(), cdm_session_id.size(),
                           content.data(), content.size(), kNoUrl);
}

void DrmSystem::OnProvisioningRequest(std::string_view content) {
  SB_CHECK(kEnableAppProvisioning);

  SB_LOG(INFO) << __func__;
  std::string cdm_session_id;
  {
    std::lock_guard lock(mutex_);
    cdm_session_id = session_id_mapper_->GetBridgeCdmSessionId();
    SB_DCHECK(!cdm_session_id.empty());

    int ticket = kSbDrmTicketInvalid;
    if (pending_ticket_.has_value()) {
      ticket = *pending_ticket_;
      SB_LOG(INFO) << "Return provision request using pending_ticket="
                   << ticket;
      pending_ticket_ = std::nullopt;
    }
  }

  update_request_callback_(this, context_, ticket, kSbDrmStatusSuccess,
                           kSbDrmSessionRequestTypeIndividualizationRequest,
                           /*error_message=*/nullptr, cdm_session_id.data(),
                           cdm_session_id.size(), content.data(),
                           content.size(), kNoUrl);
}

void DrmSystem::OnKeyStatusChange(
    std::string_view session_id,
    const std::vector<SbDrmKeyId>& drm_key_ids,
    const std::vector<SbDrmKeyStatus>& drm_key_statuses) {
  SB_DCHECK_EQ(drm_key_ids.size(), drm_key_statuses.size());

  {
    std::string session_id_str(session_id);
    std::lock_guard scoped_lock(mutex_);
    if (cached_drm_key_ids_[session_id_str] != drm_key_ids) {
      cached_drm_key_ids_[session_id_str] = drm_key_ids;
      if (hdcp_lost_) {
        CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
        return;
      }
    }
  }

  is_key_provided_ = std::any_of(
      drm_key_statuses.cbegin(), drm_key_statuses.cend(),
      [](const auto& status) { return status == kSbDrmKeyStatusUsable; });

  key_statuses_changed_callback_(this, context_, session_id.data(),
                                 session_id.size(),
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
  if (kEnableAppProvisioning) {
    return is_key_provided_;
  }

  return created_media_crypto_session_.load();
}

}  // namespace starboard::android::shared
