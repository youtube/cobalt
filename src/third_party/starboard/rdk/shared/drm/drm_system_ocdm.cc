//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#if defined(HAS_OCDM)
#include "third_party/starboard/rdk/shared/drm/drm_system_ocdm.h"

#include <dlfcn.h>
#include <mutex>
#include <cstring>
#include <gst/gst.h>

#include "starboard/shared/starboard/thread_checker.h"

#include <websocket/URL.h>
#include <opencdm/open_cdm.h>
#include <opencdm/open_cdm_adapter.h>

#include "third_party/starboard/rdk/shared/log_override.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace drm {
namespace {

static pthread_mutex_t g_session_dtor_mutex_ = PTHREAD_MUTEX_INITIALIZER;

struct OcdmSessionDeleter {
  void operator()(OpenCDMSession* session) {
    if (session) {
      pthread_mutex_lock(&g_session_dtor_mutex_);
      opencdm_destruct_session(session);
      pthread_mutex_unlock(&g_session_dtor_mutex_);
    }
  }
};

using ScopedOcdmSession = std::unique_ptr<OpenCDMSession, OcdmSessionDeleter>;

using OcdmGstSessionDecryptExFn =
  OpenCDMError(*)(struct OpenCDMSession*, GstBuffer*, GstBuffer*, const uint32_t, GstBuffer*, GstBuffer*, uint32_t, GstCaps*);

using OcdmGstSessionDecryptBufferFn =
  OpenCDMError(*)(struct OpenCDMSession* session, GstBuffer* buffer, GstCaps* caps);

using OcdmGetMetricSystemDataFn =
  OpenCDMError(*)(struct OpenCDMSystem* system, uint32_t* bufferLength, uint8_t* buffer);

static OcdmGstSessionDecryptExFn g_ocdmGstSessionDecryptEx { nullptr };
static OcdmGstSessionDecryptBufferFn g_ocdmGstSessionDecryptBuffer { nullptr };
static OcdmGetMetricSystemDataFn g_ocdmGetMetricSystemData { nullptr };

}  // namespace

namespace session {

SbDrmKeyStatus KeyStatus2DrmKeyStatus(KeyStatus status) {
  switch (status) {
    case Usable:
      return kSbDrmKeyStatusUsable;
    case Expired:
      return kSbDrmKeyStatusExpired;
    case Released:
      return kSbDrmKeyStatusReleased;
    case OutputRestricted:
      return kSbDrmKeyStatusRestricted;
    case OutputDownscaled:
      return kSbDrmKeyStatusDownscaled;
    case StatusPending:
      return kSbDrmKeyStatusPending;
    default:
    case InternalError:
      return kSbDrmKeyStatusError;
  }
}

class Session {
 public:
  Session(Session&) = delete;
  Session& operator=(Session&) = delete;

  Session(
      DrmSystemOcdm* drm_system,
      OpenCDMSystem* ocdm_system,
      void* context,
      const SbDrmSessionUpdateRequestFunc& session_update_request_callback,
      const SbDrmSessionUpdatedFunc& session_updated_callback,
      const SbDrmSessionKeyStatusesChangedFunc& key_statuses_changed_callback,
      const SbDrmSessionClosedFunc session_closed_callback);
  ~Session();
  void Close();
  void GenerateChallenge(const std::string& type,
                         const void* initialization_data,
                         int initialization_data_size,
                         int ticket);
  void Update(const void* key, int key_size, int ticket);
  std::string Id() const { return id_; }

  int Decrypt( _GstBuffer* buffer,
    _GstBuffer* sub_sample, uint32_t sub_sample_count,
    _GstBuffer* iv, _GstBuffer* key, _GstCaps* caps);

  void DispatchPendingKeyUpdates();
 private:
  static void OnProcessChallenge(OpenCDMSession* session,
                                 void* user_data,
                                 const char url[],
                                 const uint8_t challenge[],
                                 const uint16_t challenge_length);
  static void ProcessChallenge(Session* session,
                               int ticket,
                               std::string&& id,
                               std::string&& url,
                               std::string&& challenge);
  static void OnKeyUpdated(struct OpenCDMSession* session,
                           void* user_data,
                           const uint8_t key_id[],
                           const uint8_t length);
  static void OnAllKeysUpdated(const struct OpenCDMSession* session,
                               void* user_data);
  static void OnError(struct OpenCDMSession* session,
                      void* user_data,
                      const char message[]);

  OpenCDMSessionCallbacks session_callbacks_ = {
      &Session::OnProcessChallenge, &Session::OnKeyUpdated, &Session::OnError,
      &Session::OnAllKeysUpdated};

  enum class Operation {
    kNone,
    kGenrateChallenge,
    kUpdate,
  };

  ::starboard::ThreadChecker thread_checker_;
  Operation operation_{Operation::kNone};
  int ticket_{0};
  DrmSystemOcdm* drm_system_;
  OpenCDMSystem* ocdm_system_;
  ScopedOcdmSession session_;
  void* context_;
  const SbDrmSessionUpdateRequestFunc session_update_request_callback_;
  const SbDrmSessionUpdatedFunc session_updated_callback_;
  const SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback_;
  const SbDrmSessionClosedFunc session_closed_callback_;
  std::mutex mutex_;
  std::mutex close_mutex_;
  std::string last_challenge_;
  std::string last_challenge_url_;
  std::string id_;

  std::vector<SbDrmKeyId> pending_key_updates_;
  bool all_keys_updated_ { false };
};

Session::Session(
    DrmSystemOcdm* drm_system,
    OpenCDMSystem* ocdm_system,
    void* context,
    const SbDrmSessionUpdateRequestFunc& session_update_request_callback,
    const SbDrmSessionUpdatedFunc& session_updated_callback,
    const SbDrmSessionKeyStatusesChangedFunc& key_statuses_changed_callback,
    const SbDrmSessionClosedFunc session_closed_callback)
    : drm_system_(drm_system),
      ocdm_system_(ocdm_system),
      context_(context),
      session_update_request_callback_(session_update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      session_closed_callback_(session_closed_callback) {}

Session::~Session() {
  Close();
}

void Session::Close() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (session_) {
    std::lock_guard lock(close_mutex_);
    ScopedOcdmSession tmp;
    {
      std::lock_guard lock(mutex_);
      tmp = std::move (session_);
      SB_CHECK( session_ == nullptr );
    }
    opencdm_session_close(tmp.get());
  }

  auto id = Id();
  if (!id.empty())
    session_closed_callback_(drm_system_, context_, id.c_str(), id.size());
  else
    SB_LOG(WARNING) << "Closing ivalid session ?";

  {
    std::lock_guard lock(mutex_);
    ticket_ = kSbDrmTicketInvalid;
    operation_ = Operation::kNone;
    id_.clear();
  }
}

void Session::GenerateChallenge(const std::string& type,
                                const void* initialization_data,
                                int initialization_data_size,
                                int ticket) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_LOG(INFO) << "Generating challenge";
  {
    std::lock_guard lock(mutex_);
    ticket_ = ticket;
    operation_ = Operation::kGenrateChallenge;
    all_keys_updated_ = false;
  }
  OpenCDMSession* session = nullptr;
  if (opencdm_construct_session(
          ocdm_system_, Temporary, type.c_str(),
          reinterpret_cast<const uint8_t*>(initialization_data),
          initialization_data_size, nullptr, 0, &session_callbacks_, this,
          &session) != ERROR_NONE ||
      !session) {
    session_update_request_callback_(drm_system_, context_, ticket,
                                     kSbDrmStatusUnknownError,
                                     kSbDrmSessionRequestTypeLicenseRequest,
                                     nullptr, nullptr, 0, nullptr, 0, nullptr);
    return;
  }

  session_.reset(session);
  std::string challenge;
  std::string url;
  std::string id;
  {
    std::lock_guard lock(mutex_);
    id_ = opencdm_session_id(session_.get());
    id = id_;
    challenge.swap(last_challenge_);
    url.swap(last_challenge_url_);
  }

  if (!challenge.empty()) {
    Session::ProcessChallenge(this, ticket, std::move(id), std::move(url),
                              std::move(challenge));
  }
}

void Session::DispatchPendingKeyUpdates() {
  bool all_keys_updated = false;
  std::vector<SbDrmKeyId> pending_key_updates;
  {
    std::lock_guard lock(mutex_);
    pending_key_updates.swap(pending_key_updates_);
    all_keys_updated = all_keys_updated_;
  }

  if (!pending_key_updates.empty()) {
    for (const auto& drm_key_id : pending_key_updates) {
      Session::OnKeyUpdated(
         session_.get(), this,
         drm_key_id.identifier,
         drm_key_id.identifier_size);
    }
    if (all_keys_updated) {
      Session::OnAllKeysUpdated(session_.get(), this);
    }
  }
}

void Session::Update(const void* key, int key_size, int ticket) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  auto id = Id();
  SB_DCHECK(!id.empty());
  {
    std::lock_guard lock(mutex_);
    SB_LOG(INFO) << "Updating session " << id;
    ticket_ = ticket;
    operation_ = Operation::kUpdate;
  }
  if (opencdm_session_update(session_.get(), static_cast<const uint8_t*>(key),
                             key_size) != ERROR_NONE) {
    session_updated_callback_(drm_system_, context_, ticket,
                              kSbDrmStatusUnknownError, nullptr, id.c_str(),
                              id.size());
  }
}

int Session::Decrypt(
  _GstBuffer* buffer,
  _GstBuffer* sub_sample,
  uint32_t sub_sample_count,
  _GstBuffer* iv,
  _GstBuffer* key,
  _GstCaps* caps) {

  std::lock_guard lock(close_mutex_);
  OpenCDMSession* session = session_.get();
  if (!session)
    return ERROR_INVALID_SESSION;

  if (g_ocdmGstSessionDecryptEx != nullptr) {
    return g_ocdmGstSessionDecryptEx(session, buffer,
                                     sub_sample, sub_sample_count, iv,
                                     key, 0, caps);
  }

  if (g_ocdmGstSessionDecryptBuffer != nullptr) {
    return g_ocdmGstSessionDecryptBuffer(session, buffer, caps);
  }

  return opencdm_gstreamer_session_decrypt(session, buffer,
                                           sub_sample, sub_sample_count, iv,
                                           key, 0);
}

// static
void Session::OnProcessChallenge(OpenCDMSession* ocdm_session,
                                 void* user_data,
                                 const char url[],
                                 const uint8_t challenge[],
                                 const uint16_t challenge_length) {
  if (challenge_length == 0) {
    Session::OnError(ocdm_session, user_data, "Empty challenge");
    return;
  }

  Session* session = static_cast<Session*>(user_data);
  std::string id;
  int ticket;
  {
    std::lock_guard lock(session->mutex_);
    id = session->Id();
    if (id.empty()) {
      session->last_challenge_url_ = url;
      session->last_challenge_ = {reinterpret_cast<char const*>(challenge),
                                  challenge_length};
      return;
    }

    session->operation_ = Operation::kNone;
    ticket = session->ticket_;
    session->ticket_ = kSbDrmTicketInvalid;
  }

  std::string challenge_str = {reinterpret_cast<const char*>(challenge),
                               challenge_length};
  Session::ProcessChallenge(session, ticket, std::move(id), {url},
                            std::move(challenge_str));
}

// static
void Session::ProcessChallenge(Session* session,
                               int ticket,
                               std::string&& id,
                               std::string&& url,
                               std::string&& challenge) {
  SB_DCHECK(!id.empty() && !challenge.empty());

  size_t type_position = challenge.find(":Type:");
  std::string request_type = {
      challenge.c_str(),
      type_position != std::string::npos ? type_position : 0};

  unsigned offset = 0u;
  if (!request_type.empty() && request_type.length() != challenge.length())
    offset = type_position + 6;

  SbDrmSessionRequestType message_type = kSbDrmSessionRequestTypeLicenseRequest;
  if (request_type.length() == 1)
    message_type =
        static_cast<SbDrmSessionRequestType>(std::stoi(request_type));

  challenge = {challenge.c_str() + offset, challenge.size() - offset};

  SB_LOG(INFO) << "Process challenge for " << id << " type " << request_type;
  session->session_update_request_callback_(
      session->drm_system_, session->context_, ticket, kSbDrmStatusSuccess,
      message_type, "", id.c_str(), id.size(), challenge.c_str(),
      challenge.size(), url.c_str());
}

// static
void Session::OnKeyUpdated(struct OpenCDMSession* /*ocdm_session*/,
                           void* user_data,
                           const uint8_t key_id[],
                           const uint8_t length) {
  Session* session = static_cast<Session*>(user_data);
  std::string id;
  {
    std::lock_guard lock(session->mutex_);
    id = session->Id();
  }

  SbDrmKeyId drm_key_id;
  drm_key_id.identifier_size = std::min(static_cast<int>(length), SB_ARRAY_SIZE_INT(drm_key_id.identifier));
  memcpy(drm_key_id.identifier, key_id, drm_key_id.identifier_size);

  if (id.empty()) {
    std::lock_guard lock(session->mutex_);
    if (SbDrmTicketIsValid(session->ticket_)) {
      session->pending_key_updates_.push_back(std::move(drm_key_id));
    } else {
      SB_LOG(WARNING) << "Updating closed session ?";
    }
    return;
  }

  auto status = opencdm_session_status(session->session_.get(), key_id, length);

  gchar *md5sum = g_compute_checksum_for_data(G_CHECKSUM_MD5, key_id, length);
  SB_LOG(INFO) << "OnKeyUpdated SessionId: '" << id << "', key_id: '" << md5sum << "' status: " << status;
  g_free(md5sum);

  session->drm_system_->OnKeyUpdated(id, std::move(drm_key_id),
                                     KeyStatus2DrmKeyStatus(status));
}

// static
void Session::OnAllKeysUpdated(const struct OpenCDMSession* /*ocdm_session*/,
                               void* user_data) {
  Session* session = static_cast<Session*>(user_data);
  int ticket;
  std::string id;
  {
    std::lock_guard lock(session->mutex_);
    id = session->Id();
    session->all_keys_updated_ = true;
    if (id.empty()) {
      if (SbDrmTicketIsValid(session->ticket_) &&
          session->operation_ == Operation::kGenrateChallenge) {
        return;
      }
    }
    session->operation_ = Operation::kNone;
    ticket = session->ticket_;
    session->ticket_ = kSbDrmTicketInvalid;
  }
  if (id.empty()) {
    SB_LOG(WARNING) << "Updating closed session ?";
    return;
  }

  session->session_updated_callback_(session->drm_system_, session->context_,
                                     ticket, kSbDrmStatusSuccess, nullptr,
                                     id.c_str(), id.size());
  session->drm_system_->OnAllKeysUpdated();

  auto session_keys = session->drm_system_->GetSessionKeys(id);
  std::vector<SbDrmKeyId> keys;
  std::vector<SbDrmKeyStatus> statuses;
  for (auto& session_key : session_keys) {
    keys.push_back(session_key.key);
    statuses.push_back(session_key.status);
  }
  session->key_statuses_changed_callback_(
      session->drm_system_, session->context_, id.c_str(), id.size(),
      session_keys.size(), keys.data(), statuses.data());

  SB_LOG(INFO) << "Session update ended " << id;
}

// static
void Session::OnError(struct OpenCDMSession* /*ocdm_session*/,
                      void* user_data,
                      const char message[]) {
  Session* session = static_cast<Session*>(user_data);
  int ticket;
  std::string id;
  Operation operation;
  {
    std::lock_guard lock(session->mutex_);
    operation = session->operation_;
    session->operation_ = Operation::kNone;
    ticket = session->ticket_;
    session->ticket_ = kSbDrmTicketInvalid;
    id = session->Id();
  }
  SB_LOG(ERROR) << "DRM error: " << message << ", session " << id;
  switch (operation) {
    case Operation::kGenrateChallenge:
      session->session_update_request_callback_(
          session->drm_system_, session->context_, ticket,
          kSbDrmStatusUnknownError, kSbDrmSessionRequestTypeLicenseRequest,
          nullptr, nullptr, 0, nullptr, 0, nullptr);
      break;
    case Operation::kUpdate:
      session->session_updated_callback_(
          session->drm_system_, session->context_, ticket,
          kSbDrmStatusUnknownError, nullptr, id.c_str(), id.size());
      break;
    case Operation::kNone:
      break;
    default:
      SB_NOTREACHED();
      break;
  }
}

}  // namespace session

using session::Session;

DrmSystemOcdm::DrmSystemOcdm(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc session_update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback,
    SbDrmSessionClosedFunc session_closed_callback)
    : key_system_(key_system),
      context_(context),
      session_update_request_callback_(session_update_request_callback),
      session_updated_callback_(session_updated_callback),
      key_statuses_changed_callback_(key_statuses_changed_callback),
      server_certificate_updated_callback_(server_certificate_updated_callback),
      session_closed_callback_(session_closed_callback) {
  SB_LOG(INFO) << "Create DRM system ";
  ocdm_system_ = opencdm_create_system(key_system_.c_str());

  static std::once_flag flag;
  std::call_once(flag, [](){
    g_ocdmGstSessionDecryptBuffer = (OcdmGstSessionDecryptBufferFn)dlsym(RTLD_DEFAULT, "opencdm_gstreamer_session_decrypt_buffer");
    if (g_ocdmGstSessionDecryptBuffer) {
      SB_LOG(INFO) << "Has opencdm_gstreamer_session_decrypt_buffer";
    } else {
      SB_LOG(INFO) << "No opencdm_gstreamer_session_decrypt_buffer. Will try opencdm_gstreamer_session_decrypt_ex.";
      g_ocdmGstSessionDecryptEx = (OcdmGstSessionDecryptExFn)dlsym(RTLD_DEFAULT, "opencdm_gstreamer_session_decrypt_ex");
      if (g_ocdmGstSessionDecryptEx) {
        SB_LOG(INFO) << "Has opencdm_gstreamer_session_decrypt_ex";
      } else {
        SB_LOG(INFO) << "No opencdm_gstreamer_session_decrypt_ex. Fallback to opencdm_gstreamer_session_decrypt.";
      }
    }

    g_ocdmGetMetricSystemData = (OcdmGetMetricSystemDataFn)dlsym(RTLD_DEFAULT, "opencdm_get_metric_system_data");
    if (g_ocdmGetMetricSystemData) {
        SB_LOG(INFO) << "Has opencdm_get_metric_system_data";
    } else {
      SB_LOG(INFO) << "No opencdm_get_metric_system_data.";
    }
  });
}

DrmSystemOcdm::~DrmSystemOcdm() {
  SB_CHECK(ocdm_system_ == nullptr);
}

void DrmSystemOcdm::Invalidate() {
  OpenCDMSystem* ocdm_system = nullptr;
  {
    std::lock_guard lock(mutex_);
    if (event_id_ != kSbEventIdInvalid)
      SbEventCancel(event_id_);
    ocdm_system = std::exchange(ocdm_system_, nullptr);
  }
  if (ocdm_system)
    opencdm_destruct_system(ocdm_system);
}

// static
bool DrmSystemOcdm::IsKeySystemSupported(const char* key_system,
                                         const char* mime_type) {
  return opencdm_is_type_supported(key_system, mime_type) == ERROR_NONE;
}

void DrmSystemOcdm::GenerateSessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  SB_CHECK(ocdm_system_ != nullptr);
  SB_LOG(INFO) << "Generate challenge type: " << type;
  std::unique_ptr<Session> session(
      new Session(this, ocdm_system_, context_,
                  session_update_request_callback_, session_updated_callback_,
                  key_statuses_changed_callback_, session_closed_callback_));
  session->GenerateChallenge(type, initialization_data,
                             initialization_data_size, ticket);
  Session *session_ptr = session.get();
  std::unique_lock lock(mutex_);
  sessions_.push_back(std::move(session));
  lock.unlock();
  session_ptr->DispatchPendingKeyUpdates();
}

void DrmSystemOcdm::UpdateSession(int ticket,
                                  const void* key,
                                  int key_size,
                                  const void* session_id,
                                  int session_id_size) {
  std::string id = {static_cast<const char*>(session_id), static_cast<std::string::size_type>(session_id_size)};
  SB_LOG(INFO) << "Update: " << id;
  auto* session = GetSessionById(id);
  if (session)
    session->Update(key, key_size, ticket);
}

void DrmSystemOcdm::CloseSession(const void* session_id, int session_id_size) {
  std::string id = {static_cast<const char*>(session_id), static_cast<std::string::size_type>(session_id_size)};
  SB_LOG(INFO) << "Close: " << id;
  auto* session = GetSessionById(id);
  if (session)
    session->Close();
}

void DrmSystemOcdm::UpdateServerCertificate(int ticket,
                                            const void* certificate,
                                            int certificate_size) {
  SB_CHECK(ocdm_system_ != nullptr);
  auto status = opencdm_system_set_server_certificate(
      ocdm_system_, static_cast<const uint8_t*>(certificate), certificate_size);

  server_certificate_updated_callback_(
      this, context_, ticket,
      status == ERROR_NONE ? kSbDrmStatusSuccess : kSbDrmStatusUnknownError,
      "Error");
}

SbDrmSystemPrivate::DecryptStatus DrmSystemOcdm::Decrypt(
    ::starboard::InputBuffer* buffer) {
  SB_NOTREACHED();
  return kFailure;
}

Session* DrmSystemOcdm::GetSessionById(const std::string& id) {
  std::lock_guard lock(mutex_);
  auto iter = std::find_if(
      sessions_.begin(), sessions_.end(),
      [&id](const std::unique_ptr<Session>& s) { return id == s->Id(); });

  if (iter != sessions_.end())
    return iter->get();

  return nullptr;
}

void DrmSystemOcdm::AddObserver(DrmSystemOcdm::Observer* obs) {
  std::lock_guard lock(mutex_);
  observers_.push_back(obs);
}

void DrmSystemOcdm::RemoveObserver(DrmSystemOcdm::Observer* obs) {
  std::lock_guard lock(mutex_);
  auto found = std::find(observers_.begin(), observers_.end(), obs);
  SB_DCHECK(found != observers_.end());
  observers_.erase(found);
}

void DrmSystemOcdm::OnKeyUpdated(const std::string& session_id,
                                 SbDrmKeyId&& key_id,
                                 SbDrmKeyStatus status) {
  std::lock_guard lock(mutex_);
  auto session_key = session_keys_.find(session_id);
  KeyWithStatus key_with_status;
  key_with_status.key = key_id;
  key_with_status.status = status;
  if (session_key == session_keys_.end()) {
    session_keys_[session_id].emplace_back(std::move(key_with_status));
  } else {
    auto key_entry = std::find_if(
        session_key->second.begin(), session_key->second.end(),
        [&key_id](const KeyWithStatus& key_with_status) {
          return memcmp(key_id.identifier, key_with_status.key.identifier,
                        std::min(key_with_status.key.identifier_size,
                                 key_id.identifier_size)) == 0;
        });
    if (key_entry != session_key->second.end()) {
      key_entry->status = status;
    } else {
      session_key->second.emplace_back(std::move(key_with_status));
    }
  }
}

void DrmSystemOcdm::OnAllKeysUpdated() {
  std::lock_guard lock(mutex_);
  cached_ready_keys_.clear();
  if (event_id_ != kSbEventIdInvalid)
    SbEventCancel(event_id_);
  event_id_ = SbEventSchedule(
      [](void* data) {
        DrmSystemOcdm* self = reinterpret_cast<DrmSystemOcdm*>(data);
        self->AnnounceKeys();
      },
      this, 0);
}

std::set<std::string> DrmSystemOcdm::GetReadyKeysUnlocked() const {
  if (cached_ready_keys_.empty()) {
    for (auto& session_key : session_keys_) {
      for (auto& key_with_status : session_key.second) {
        cached_ready_keys_.emplace(std::string{
            reinterpret_cast<const char*>(key_with_status.key.identifier),
            static_cast<std::string::size_type>(key_with_status.key.identifier_size)});
      }
    }
  }

  return cached_ready_keys_;
}

std::set<std::string> DrmSystemOcdm::GetReadyKeys() {
  std::lock_guard lock(mutex_);
  return GetReadyKeysUnlocked();
}

DrmSystemOcdm::KeysWithStatus DrmSystemOcdm::GetSessionKeys(
    const std::string& session_id) const {
  auto session_key = session_keys_.find(session_id);
  return session_key != session_keys_.end() ? session_key->second
                                            : DrmSystemOcdm::KeysWithStatus{};
}

void DrmSystemOcdm::AnnounceKeys() {
  std::lock_guard lock(mutex_);
  auto ready_keys = GetReadyKeysUnlocked();
  for (auto* observer : observers_) {
    for (auto& key : ready_keys) {
      observer->OnKeyReady(reinterpret_cast<const uint8_t*>(key.c_str()),
                           key.size());
    }
  }
  event_id_ = kSbEventIdInvalid;
}

std::string DrmSystemOcdm::SessionIdByKeyId(const uint8_t* key,
                                            uint8_t key_len) {
  SB_CHECK(ocdm_system_ != nullptr);
  pthread_mutex_lock(&g_session_dtor_mutex_);
  ScopedOcdmSession session{
      opencdm_get_system_session(ocdm_system_, key, key_len, 0)};
  pthread_mutex_unlock(&g_session_dtor_mutex_);
  return session ? opencdm_session_id(session.get()) : std::string{};
}

int DrmSystemOcdm::Decrypt(const std::string& id,
                            _GstBuffer* buffer,
                            _GstBuffer* sub_sample,
                            uint32_t sub_sample_count,
                            _GstBuffer* iv,
                            _GstBuffer* key,
                            _GstCaps* caps) {
  session::Session* session = GetSessionById(id);
  if (!session)
    return ERROR_INVALID_SESSION;
  return session->Decrypt(buffer, sub_sample, sub_sample_count, iv, key, caps);
}

const void* DrmSystemOcdm::GetMetrics(int* size) {
  if ( !g_ocdmGetMetricSystemData )
    return nullptr;

  SB_CHECK(ocdm_system_ != nullptr);

  const int kMaxRetry = 5;
  for (int i = 0; i < kMaxRetry; ++i) {
    uint32_t buffer_length =  ( 1 << i ) * 4 * 1024;

    std::vector<uint8_t> tmp;
    tmp.resize(buffer_length);

    const uint32_t kBufferTooSmallErrorCode = 4; // ERROR_BUFFER_TOO_SMALL

    auto rc = g_ocdmGetMetricSystemData(ocdm_system_, &buffer_length, tmp.data());
    if ( rc == ERROR_NONE ) {
      uint16_t out_length = (((buffer_length * 8) / 6) + 4) * sizeof(TCHAR);
      metrics_.resize(out_length, '\0');
      out_length = WPEFramework::Core::URL::Base64Encode(tmp.data(), buffer_length, reinterpret_cast<char*>(metrics_.data()), out_length, false);
      metrics_.resize(out_length);
    } else if ( rc == kBufferTooSmallErrorCode && i < (kMaxRetry - 1) ) {
      SB_LOG(INFO) << "GetMetrics: buffer is too small, rc = " << rc << ", i = " << i;
      continue;
    } else {
      metrics_.resize(0);
      SB_LOG(ERROR) << "GetMetrics: failed, rc = " << rc;
    }

    break;
  }

  *size = static_cast<int>(metrics_.size());
  return metrics_.data();
}

}  // namespace drm
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

#endif  // defined(HAS_OCDM)
