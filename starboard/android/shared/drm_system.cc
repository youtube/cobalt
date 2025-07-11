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
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "media_drm_bridge.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/thread.h"

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

DECLARE_INSTANCE_COUNTER(AndroidDrmSystem)

constexpr char kNoUrl[] = "";

std::string GenerateBridgeSesssionId() {
  static int counter = 0;
  return "cobalt.sid." + std::to_string(counter++);
}
}  // namespace

std::ostream& operator<<(std::ostream& os, const DrmSystem::SessionIdMap& map) {
  os << "{cdm_id: " << map.cdm_id << ", media_drm_id: " << map.media_drm_id
     << "}";
  return os;
}

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

  media_drm_bridge_ = std::make_unique<MediaDrmBridge>(
      base::raw_ref<MediaDrmBridge::Host>(*this), key_system);
  if (!media_drm_bridge_->is_valid()) {
    return;
  }

  Start();
}

void DrmSystem::Run() {
  job_queue_ =
      std::make_unique<starboard::shared::starboard::player::JobQueue>();
  job_queue_->RunUntilStopped();

  // job_queue_ should be destroyed at the same thread that it was created.
  job_queue_.reset();
}

DrmSystem::~DrmSystem() {
  ON_INSTANCE_RELEASED(AndroidDrmSystem);
  job_queue_->StopSoon();
  Join();
  SB_DCHECK_EQ(job_queue_, nullptr);
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

DrmSystem::SessionUpdateRequest
DrmSystem::SessionUpdateRequest::CloneWithoutTicket() const {
  return DrmSystem::SessionUpdateRequest(kSbDrmTicketInvalid, mime_.c_str(),
                                         init_data_.data(), init_data_.size());
}

void DrmSystem::SessionUpdateRequest::Generate(
    const MediaDrmBridge* media_drm_bridge) const {
  SB_DCHECK(media_drm_bridge);
  media_drm_bridge->CreateSession(ticket_, init_data_, mime_);
}

MediaDrmBridge::Status DrmSystem::SessionUpdateRequest::GenerateNoProvisioning(
    const MediaDrmBridge* media_drm_bridge) const {
  SB_DCHECK(media_drm_bridge);
  return media_drm_bridge->CreateSessionNoProvisioning(ticket_, init_data_,
                                                       mime_);
}

void DrmSystem::GenerateSessionUpdateRequest(int ticket,
                                             const char* type,
                                             const void* initialization_data,
                                             int initialization_data_size) {
  GenerateSessionUpdateRequestInternal(std::make_unique<SessionUpdateRequest>(
      ticket, type, initialization_data, initialization_data_size));
}

void DrmSystem::GenerateSessionUpdateRequestInternal(
    std::unique_ptr<SessionUpdateRequest> request) {
  MediaDrmBridge::Status status =
      request->GenerateNoProvisioning(media_drm_bridge_.get());
  switch (status.type) {
    case MediaDrmBridge::Status::kSuccess:
      return;
    case MediaDrmBridge::Status::kNotProvisionedError:
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
    case MediaDrmBridge::Status::kOperationError:
    default:
      SB_LOG(ERROR) << "GenerateNoProvisioning failed: "
                    << status.error_message;
      return;
  }
}

void DrmSystem::UpdateSession(int ticket,
                              const void* key,
                              int key_size,
                              const void* session_id,
                              int session_id_size) {
  std::string error_msg;
  const std::string_view cdm_session_id(static_cast<const char*>(session_id),
                                        session_id_size);
  std::string_view media_drm_session_id = cdm_session_id;
  std::optional<MediaDrmBridge::Status> completed_status;
  SB_LOG(INFO) << __func__ << ": cdm_session_id=" << cdm_session_id;
  if (bridge_session_id_map_.has_value() &&
      bridge_session_id_map_->cdm_id == cdm_session_id) {
    if (bridge_session_id_map_->media_drm_id.empty()) {
      SB_LOG(INFO) << "Calling ProvideProvisionResponse, since MediaDrmSession "
                      "is not created yet";
      completed_status.emplace(
          media_drm_bridge_->ProvideProvisionResponse(key, key_size));
    } else {
      media_drm_session_id = bridge_session_id_map_->media_drm_id;
    }
  }

  if (!completed_status.has_value()) {
    completed_status.emplace(media_drm_bridge_->UpdateSession(
        ticket, key, key_size, media_drm_session_id.data(),
        media_drm_session_id.size(), &error_msg));
  }
  SB_CHECK(completed_status.has_value());

  bool update_success = completed_status->ok();
  session_updated_callback_(
      this, context_, ticket,
      update_success ? kSbDrmStatusSuccess : kSbDrmStatusUnknownError,
      error_msg.c_str(), cdm_session_id.data(), cdm_session_id.size());

  if (update_success) {
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

  GenerateSessionUpdateRequestInternal(std::move(request));
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

  const std::string media_drm_session_id =
      bridge_session_id_map_.has_value() &&
              bridge_session_id_map_->cdm_id == session_id_as_string
          ? bridge_session_id_map_->media_drm_id
          : session_id_as_string;

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
  // TODO: Returns kRetry when |UpdateSession| is not called at all to allow
  // the
  //       player worker to handle the retry logic.
  return kSuccess;
}

const void* DrmSystem::GetMetrics(int* size) {
  return media_drm_bridge_->GetMetrics(size);
}

void DrmSystem::OnSessionUpdate(int ticket,
                                SbDrmSessionRequestType request_type,
                                const std::string& session_id,
                                const std::string& content) {
  std::string cdm_session_id = session_id;
  if (bridge_session_id_map_.has_value()) {
    if (bridge_session_id_map_->media_drm_id.empty()) {
      bridge_session_id_map_->media_drm_id = session_id;
    }

    if (bridge_session_id_map_->media_drm_id == session_id) {
      cdm_session_id = bridge_session_id_map_->cdm_id;
    }
  }

  update_request_callback_(this, context_, ticket, kSbDrmStatusSuccess,
                           request_type, /*error_message=*/nullptr,
                           cdm_session_id.data(), cdm_session_id.size(),
                           content.data(), content.size(), kNoUrl);
}

void DrmSystem::OnProvisioningRequest(const std::string& content) {
  SB_LOG(INFO) << __func__;
  if (!bridge_session_id_map_.has_value()) {
    bridge_session_id_map_.emplace(
        SessionIdMap{.cdm_id = GenerateBridgeSesssionId()});
    SB_LOG(INFO) << "bridge session is created: map="
                 << *bridge_session_id_map_;
  };
  int ticket = kSbDrmTicketInvalid;
  {
    std::lock_guard lock(mutex_);
    if (pending_ticket_.has_value()) {
      ticket = *pending_ticket_;
      SB_LOG(INFO) << "Return provision request using pending_ticket="
                   << ticket;
      pending_ticket_ = std::nullopt;
    }
  }

  update_request_callback_(this, context_, ticket, kSbDrmStatusSuccess,
                           kSbDrmSessionRequestTypeIndividualizationRequest,
                           /*error_message=*/nullptr,
                           bridge_session_id_map_->cdm_id.data(),
                           bridge_session_id_map_->cdm_id.size(),
                           content.data(), content.size(), kNoUrl);
}

void DrmSystem::OnKeyStatusChange(
    const std::string& session_id,
    const std::vector<SbDrmKeyId>& drm_key_ids,
    const std::vector<SbDrmKeyStatus>& drm_key_statuses) {
  SB_DCHECK_EQ(drm_key_ids.size(), drm_key_statuses.size());

  std::string session_id_str(session_id);
  {
    std::lock_guard scoped_lock(mutex_);
    if (cached_drm_key_ids_[session_id_str] != drm_key_ids) {
      cached_drm_key_ids_[session_id_str] = drm_key_ids;
      if (hdcp_lost_) {
        CallKeyStatusesChangedCallbackWithKeyStatusRestricted_Locked();
        return;
      }
    }
  }

  /*
  std::string log;
  for (int i = 0; i < drm_key_ids.size(); i++) {
    log += (i > 0 ? ", " : "") + drm_key_ids[i] + "=" +
           BytesToString(drm_key_statuses[i];
  }
  SB_LOG(INFO) << "Key status changed: session_id=" << session_id_as_string
               << ", key_ids={" << log + "}";
  */

  is_key_provided_ = std::any_of(
      drm_key_statuses.cbegin(), drm_key_statuses.cend(),
      [](const auto& status) { return status == kSbDrmKeyStatusUsable; });

  key_statuses_changed_callback_(this, context_, session_id.data(),
                                 session_id.size(),
                                 static_cast<int>(drm_key_ids.size()),
                                 drm_key_ids.data(), drm_key_statuses.data());
}

void DrmSystem::OnInsufficientOutputProtection() {
  // HDCP has lost, update the statuses of all keys in all known sessions to
  // be restricted.
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
  return is_key_provided_;
}

}  // namespace starboard::android::shared
