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

#include "starboard/shared/widevine/drm_system_widevine.h"

#include <vector>

#include "starboard/character.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/shared/widevine/widevine_storage.h"
#include "starboard/shared/widevine/widevine_timer.h"
#include "starboard/string.h"
#include "starboard/time.h"
#include "third_party/ce_cdm/core/include/log.h"  // for wvcdm::InitLogging();
#include "third_party/ce_cdm/core/include/string_conversions.h"

using wv3cdm = ::widevine::Cdm;

namespace starboard {
namespace shared {
namespace widevine {
namespace {

const int kInitializationVectorSize = 16;
const char* kWidevineKeySystem[] = {"com.widevine", "com.widevine.alpha"};
const char kWidevineStorageFileName[] = "wvcdm.dat";

class WidevineClock : public wv3cdm::IClock {
 public:
  int64_t now() override {
    return SbTimeToPosix(SbTimeGetNow()) / kSbTimeMillisecond;
  }
};

std::string GetWidevineStoragePath() {
  char path[SB_FILE_MAX_PATH + 1] = {0};
  auto path_size = SB_ARRAY_SIZE_INT(path);
  SB_CHECK(SbSystemGetPath(kSbSystemPathCacheDirectory, path, path_size) &&
           SbStringConcat(path, SB_FILE_SEP_STRING, path_size) &&
           SbStringConcat(path, kWidevineStorageFileName, path_size));
  return path;
}

// Converts |::widevine::Cdm::KeyStatus| to starboard's |SbDrmKeyStatus|
// Note: there is no mapping from any Widevine's Cdm::KeyStatus to
// starboard's kSbDrmKeyStatusDownscaled.
SbDrmKeyStatus CdmKeyStatusToSbDrmKeyStatus(
    const wv3cdm::KeyStatus key_status) {
  switch (key_status) {
    case wv3cdm::kUsable:
      return kSbDrmKeyStatusUsable;
    case wv3cdm::kExpired:
      return kSbDrmKeyStatusExpired;
    case wv3cdm::kOutputRestricted:
      return kSbDrmKeyStatusRestricted;
    case wv3cdm::kStatusPending:
      return kSbDrmKeyStatusPending;
    case wv3cdm::kInternalError:
      return kSbDrmKeyStatusError;
    case wv3cdm::kReleased:
      return kSbDrmKeyStatusReleased;
    default:
      SB_NOTREACHED();
  }
  return kSbDrmKeyStatusError;
}

// Converts |::widevine::Cdm::Status| to starboard's |SbDrmStatus|
// Note: there is no mapping for few Widevine's Cdm::Status-es that
// just converted here to starboard's kSbDrmStatusUnknownError.
SbDrmStatus CdmStatusToSbDrmStatus(const wv3cdm::Status status) {
  switch (status) {
    case wv3cdm::kSuccess:
      return kSbDrmStatusSuccess;
    case wv3cdm::kTypeError:
      return kSbDrmStatusTypeError;
    case wv3cdm::kNotSupported:
      return kSbDrmStatusNotSupportedError;
    case wv3cdm::kInvalidState:
      return kSbDrmStatusInvalidStateError;
    case wv3cdm::kQuotaExceeded:
      return kSbDrmStatusQuotaExceededError;
    case wv3cdm::kNeedsDeviceCertificate:
    case wv3cdm::kSessionNotFound:
    case wv3cdm::kDecryptError:
    case wv3cdm::kNoKey:
    case wv3cdm::kKeyUsageBlockedByPolicy:
    case wv3cdm::kRangeError:
    case wv3cdm::kDeferred:
    case wv3cdm::kUnexpectedError:
      return kSbDrmStatusUnknownError;
    default:
      SB_NOTREACHED();
  }
  return kSbDrmStatusUnknownError;
}

SB_ONCE_INITIALIZE_FUNCTION(Mutex, GetInitializationMutex);

void EnsureWidevineCdmIsInitialized(const std::string& company_name,
                                    const std::string& model_name) {
  static WidevineClock s_clock;
  static WidevineStorage s_storage(GetWidevineStoragePath());
  static WidevineTimer s_timer;
  static bool s_initialized = false;

  ScopedLock scoped_lock(*GetInitializationMutex());

  if (s_initialized) {
    return;
  }

  wvcdm::InitLogging();

  wv3cdm::ClientInfo client_info;

  client_info.product_name = "Cobalt";
  client_info.company_name = company_name;
  client_info.device_name = "";
  client_info.model_name = model_name;
  client_info.arch_name = "";
  client_info.build_info = wv3cdm::version();

  SB_LOG(INFO) << "Initialize wvcdm using product_name: \""
               << client_info.product_name << "\", company_name: \""
               << client_info.company_name << "\", and model_name: \""
               << client_info.model_name << "\".";

  auto log_level = wv3cdm::kInfo;
#if COBALT_BUILD_TYPE_GOLD
  log_level = wv3cdm::kSilent;
#endif  // COBALT_BUILD_TYPE_GOLD
  wv3cdm::Status status =
      wv3cdm::initialize(wv3cdm::kNoSecureOutput, client_info, &s_storage,
                         &s_clock, &s_timer, log_level);
  SB_DCHECK(status == wv3cdm::kSuccess);
  s_initialized = true;
}

}  // namespace

// static
const char DrmSystemWidevine::kFirstSbDrmSessionId[] = "initialdrmsessionid";

DrmSystemWidevine::DrmSystemWidevine(
    void* context,
    SbDrmSessionUpdateRequestFunc session_update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback
#if SB_HAS(DRM_KEY_STATUSES)
    ,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback
#endif  // SB_HAS(DRM_KEY_STATUSES)
#if SB_API_VERSION >= 10
    ,
    SbDrmServerCertificateUpdatedFunc server_certificate_updated_callback
#endif  // SB_API_VERSION >= 10
#if SB_HAS(DRM_SESSION_CLOSED)
    ,
    SbDrmSessionClosedFunc session_closed_callback
#endif  // SB_HAS(DRM_SESSION_CLOSED)
    ,
    const std::string& company_name,
    const std::string& model_name)
    : context_(context),
      session_update_request_callback_(session_update_request_callback),
      session_updated_callback_(session_updated_callback),
#if SB_HAS(DRM_KEY_STATUSES)
      key_statuses_changed_callback_(key_statuses_changed_callback),
#endif  // SB_HAS(DRM_KEY_STATUSES)
#if SB_API_VERSION >= 10
      server_certificate_updated_callback_(server_certificate_updated_callback),
#endif  // SB_API_VERSION >= 10
#if SB_HAS(DRM_SESSION_CLOSED)
      session_closed_callback_(session_closed_callback),
#endif  // SB_HAS(DRM_SESSION_CLOSED)
      ticket_thread_id_(SbThreadGetId()) {
  SB_DCHECK(!company_name.empty());
  SB_DCHECK(!model_name.empty());
  EnsureWidevineCdmIsInitialized(company_name, model_name);
#if SB_API_VERSION >= 10
  const bool kEnablePrivacyMode = true;
#else   // SB_API_VERSION >= 10
  const bool kEnablePrivacyMode = false;
#endif  // SB_API_VERSION >= 10
  cdm_.reset(wv3cdm::create(this, NULL, kEnablePrivacyMode));
  SB_DCHECK(cdm_);
}

DrmSystemWidevine::~DrmSystemWidevine() {}

// static
bool DrmSystemWidevine::IsKeySystemSupported(const char* key_system) {
  for (auto wv_key_system : kWidevineKeySystem) {
    if (SbStringCompareAll(key_system, wv_key_system) == 0) {
      return true;
    }
  }
  return false;
}

void DrmSystemWidevine::GenerateSessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  const std::string init_str(static_cast<const char*>(initialization_data),
                             initialization_data_size);
  wv3cdm::InitDataType init_type = wv3cdm::kWebM;
  if (SbStringCompareAll("cenc", type) == 0) {
    init_type = wv3cdm::kCenc;
  } else if (SbStringCompareAll("webm", type) == 0) {
    init_type = wv3cdm::kWebM;
  } else {
    SB_NOTREACHED();
  }

  if (!is_server_certificate_set_) {
    // When privacy mode is on and server certificate hasn't been set yet for
    // the current playback, save the requests and send a server certificate
    // request instead.
    bool first_request = pending_generate_session_update_requests_.empty();
    GenerateSessionUpdateRequestData request_data = {
        first_request ? kSbDrmTicketInvalid : ticket, init_type, init_str};
    pending_generate_session_update_requests_.push_back(request_data);
    if (first_request) {
      SendServerCertificateRequest(ticket);
    }
    return;
  }

  GenerateSessionUpdateRequestInternal(ticket, init_type, init_str, false);
}

void DrmSystemWidevine::UpdateSession(int ticket,
                                      const void* key,
                                      int key_size,
                                      const void* sb_drm_session_id,
                                      int sb_drm_session_id_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  const std::string str_key(static_cast<const char*>(key), key_size);

  wv3cdm::Status status;
  if (!pending_generate_session_update_requests_.empty()) {
    status = ProcessServerCertificateResponse(str_key);
  } else {
    const std::string wvcdm_session_id = SbDrmSessionIdToWvdmSessionId(
        sb_drm_session_id, sb_drm_session_id_size);
    status = cdm_->update(wvcdm_session_id, str_key);
  }
  SB_DLOG(INFO) << "Update keys status " << status;
#if SB_API_VERSION >= 10
  session_updated_callback_(this, context_, ticket,
                            CdmStatusToSbDrmStatus(status), "",
                            sb_drm_session_id, sb_drm_session_id_size);
#else   // SB_API_VERSION >= 10
  session_updated_callback_(this, context_, ticket, sb_drm_session_id,
                            sb_drm_session_id_size, status == wv3cdm::kSuccess);
#endif  // SB_API_VERSION >= 10

  // It is possible that |key| actually contains a server certificate, in such
  // case try to process the pending GenerateSessionUpdateRequest() calls.
  TrySendPendingGenerateSessionUpdateRequests();
}

void DrmSystemWidevine::CloseSession(const void* sb_drm_session_id,
                                     int sb_drm_session_id_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  const std::string wvcdm_session_id =
      SbDrmSessionIdToWvdmSessionId(sb_drm_session_id, sb_drm_session_id_size);
  cdm_->close(wvcdm_session_id);
}

#if SB_API_VERSION >= 10
void DrmSystemWidevine::UpdateServerCertificate(int ticket,
                                                const void* certificate,
                                                int certificate_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  const std::string str_certificate(static_cast<const char*>(certificate),
                                    certificate_size);
  wv3cdm::Status status = cdm_->setServiceCertificate(str_certificate);

  is_server_certificate_set_ = (status == wv3cdm::kSuccess);

  server_certificate_updated_callback_(this, context_, ticket,
                                       CdmStatusToSbDrmStatus(status), "");
}
#endif  // SB_API_VERSION >= 10

void IncrementIv(uint8_t* iv, size_t block_count) {
  if (0 == block_count)
    return;
  uint8_t carry = 0;
  uint8_t n = static_cast<uint8_t>(kInitializationVectorSize - 1);

  while (n >= 8) {
    uint32_t temp = block_count & 0xff;
    temp += iv[n];
    temp += carry;
    iv[n] = temp & 0xff;
    carry = (temp & 0x100) ? 1 : 0;
    block_count = block_count >> 8;
    n--;
    if (0 == block_count && !carry) {
      break;
    }
  }
}

SbDrmSystemPrivate::DecryptStatus DrmSystemWidevine::Decrypt(
    InputBuffer* buffer) {
  const SbDrmSampleInfo* drm_info = buffer->drm_info();

  if (drm_info == NULL || drm_info->initialization_vector_size == 0) {
    return kSuccess;
  }

  // Adapt |buffer| and |drm_info| to a |cdm::InputBuffer|.
  SB_DCHECK(drm_info->initialization_vector_size == kInitializationVectorSize);
  std::vector<uint8_t> initialization_vector(
      drm_info->initialization_vector,
      drm_info->initialization_vector + drm_info->initialization_vector_size);

  wv3cdm::InputBuffer input;
  input.data = buffer->data();
  input.data_length = buffer->size();
  input.block_offset = 0;
  input.key_id = drm_info->identifier;
  input.key_id_length = drm_info->identifier_size;
  input.iv = initialization_vector.data();
  input.iv_length = static_cast<uint32_t>(initialization_vector.size());
  input.is_video = (buffer->sample_type() == kSbMediaTypeVideo);

  std::vector<uint8_t> output_data(buffer->size());
  wv3cdm::OutputBuffer output;
  output.data = output_data.data();
  output.data_length = output_data.size();

  size_t block_counter = 0;
  size_t encrypted_offset = 0;

  for (size_t i = 0; i < buffer->drm_info()->subsample_count; i++) {
    const SbDrmSubSampleMapping& subsample =
        buffer->drm_info()->subsample_mapping[i];
    if (subsample.clear_byte_count) {
      input.last_subsample = i + 1 == buffer->drm_info()->subsample_count &&
                             subsample.encrypted_byte_count == 0;
      input.encryption_scheme = wv3cdm::EncryptionScheme::kClear;
      input.data_length = subsample.clear_byte_count;

      wv3cdm::Status status = cdm_->decrypt(input, output);
      if (status != wv3cdm::kSuccess) {
        if (status == wv3cdm::kNoKey) {
          return kRetry;
        }
        SB_DLOG(ERROR) << "Decrypt status " << status;
        SB_DLOG(ERROR) << "Key ID "
                       << wvcdm::a2bs_hex(
                              std::string(reinterpret_cast<const char*>(
                                              &drm_info->identifier[0]),
                                          drm_info->identifier_size));
        return kFailure;
      }

      input.data += subsample.clear_byte_count;
      output.data += subsample.clear_byte_count;
      output.data_length -= subsample.clear_byte_count;
      input.first_subsample = false;
    }

    if (subsample.encrypted_byte_count) {
      input.last_subsample = i + 1 == buffer->drm_info()->subsample_count;
      input.encryption_scheme = wv3cdm::EncryptionScheme::kAesCtr;
      input.data_length = subsample.encrypted_byte_count;

      wv3cdm::Status status = cdm_->decrypt(input, output);
      if (status != wv3cdm::kSuccess) {
        if (status == wv3cdm::kNoKey) {
          SB_DLOG(ERROR) << "Decrypt status: kNoKey";
          return kRetry;
        }
        SB_DLOG(ERROR) << "Decrypt status " << status;
        SB_DLOG(ERROR) << "Key ID "
                       << wvcdm::a2bs_hex(
                              std::string(reinterpret_cast<const char*>(
                                              &drm_info->identifier[0]),
                                          drm_info->identifier_size));
        return kFailure;
      }

      input.data += subsample.encrypted_byte_count;
      output.data += subsample.encrypted_byte_count;
      output.data_length -= subsample.encrypted_byte_count;

      input.block_offset += subsample.encrypted_byte_count;
      input.block_offset %= 16;

      encrypted_offset += subsample.encrypted_byte_count;

      // Increase initialization vector for CTR mode.
      if (input.encryption_scheme == wv3cdm::kAesCtr) {
        size_t new_block_counter = encrypted_offset / 16;
        IncrementIv(initialization_vector.data(),
                    new_block_counter - block_counter);
        block_counter = new_block_counter;
      }
      input.first_subsample = false;
    }
  }

  buffer->SetDecryptedContent(output_data.data(), output_data.size());
  return kSuccess;
}

void DrmSystemWidevine::GenerateSessionUpdateRequestInternal(
    int ticket,
    wv3cdm::InitDataType init_data_type,
    const std::string& initialization_data,
    bool is_first_session) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  wv3cdm::SessionType session_type = wv3cdm::kTemporary;

  std::string wvcdm_session_id;
  // createSession() may return |kDeferred| if individualization is pending.
  wv3cdm::Status status = cdm_->createSession(session_type, &wvcdm_session_id);

  if (status == wv3cdm::kSuccess) {
    // Ensure that the session id generated by the cdm is never the same as the
    // fake id (kFirstSbDrmSessionId).
    SB_DCHECK(wvcdm_session_id != kFirstSbDrmSessionId);
    if (is_first_session) {
      first_wvcdm_session_id_ = wvcdm_session_id;
    }
    SetTicket(WvdmSessionIdToSbDrmSessionId(wvcdm_session_id), ticket);
    SB_DLOG(INFO) << "Calling generateRequest()";
    status = cdm_->generateRequest(wvcdm_session_id, init_data_type,
                                   initialization_data);
    SB_DLOG(INFO) << "generateRequest() returns " << status;
  } else {
    // createSession() shouldn't return |kDeferred|, and if it does, the
    // following if statement will incorrectly assume that there is a follow-up
    // license request automatically generated after the individualization is
    // finished, which won't happen.
    SB_DCHECK(status != wv3cdm::kDeferred);
  }

  if (status != wv3cdm::kSuccess && status != wv3cdm::kDeferred) {
    // Reset ticket before invoking user-provided callback to indicate that
    // no session update request is pending.
    SetTicket(WvdmSessionIdToSbDrmSessionId(wvcdm_session_id),
              kSbDrmTicketInvalid);

    SB_DLOG(ERROR) << "GenerateKeyRequest status " << status;
// Send an empty request to signal an error.
#if SB_API_VERSION >= 10
    session_update_request_callback_(
        this, context_, ticket, CdmStatusToSbDrmStatus(status),
        kSbDrmSessionRequestTypeLicenseRequest, "", NULL, 0, NULL, 0, NULL);
#else   // SB_API_VERSION >= 10
    session_update_request_callback_(this, context_, ticket, NULL, 0, NULL, 0,
                                     NULL);
#endif  // SB_API_VERSION >= 10
  }

  // When |status| is |kDeferred|, it indicates that the cdm requires
  // individualization.  In such case an individualization request may be sent
  // if this is the first GenerateSessionUpdateRequest().  We won't send a
  // generated key request now. Once the individualization response is received
  // by the cdm, it will call |onMessage| for all pending sessions on the same
  // thread.
}

void DrmSystemWidevine::onMessage(const std::string& wvcdm_session_id,
                                  wv3cdm::MessageType message_type,
                                  const std::string& message) {
  const std::string sb_drm_session_id =
      WvdmSessionIdToSbDrmSessionId(wvcdm_session_id);
  switch (message_type) {
    case wv3cdm::kLicenseRequest:
      SendSessionUpdateRequest(kSbDrmSessionRequestTypeLicenseRequest,
                               sb_drm_session_id, message);
      break;
    case wv3cdm::kLicenseRenewal:
      SendSessionUpdateRequest(kSbDrmSessionRequestTypeLicenseRenewal,
                               sb_drm_session_id, message);
      break;
    case wv3cdm::kLicenseRelease:
      SendSessionUpdateRequest(kSbDrmSessionRequestTypeLicenseRelease,
                               sb_drm_session_id, message);
      break;
    case wv3cdm::kIndividualizationRequest:
      // Not used, onDirectIndividualizationRequest() will be called instead.
      SB_NOTREACHED();
      break;
    case wv3cdm::kLicenseSub:
      // For loading sub licenses from embedded key data, not used.
      SB_NOTREACHED();
      break;
  }
}

void DrmSystemWidevine::onKeyStatusesChange(
    const std::string& wvcdm_session_id) {
#if SB_HAS(DRM_KEY_STATUSES)
  wv3cdm::KeyStatusMap key_statuses;
  wv3cdm::Status status = cdm_->getKeyStatuses(wvcdm_session_id, &key_statuses);

  if (status != wv3cdm::kSuccess) {
    return;
  }

  std::vector<SbDrmKeyId> sb_key_ids;
  std::vector<SbDrmKeyStatus> sb_key_statuses;

  for (auto& key_status : key_statuses) {
    SbDrmKeyId sb_key_id;
    SB_DCHECK(key_status.first.size() <= sizeof(sb_key_id.identifier));
    SbMemoryCopy(sb_key_id.identifier, key_status.first.c_str(),
                 key_status.first.size());
    sb_key_id.identifier_size = static_cast<int>(key_status.first.size());
    sb_key_ids.push_back(sb_key_id);
    sb_key_statuses.push_back(CdmKeyStatusToSbDrmKeyStatus(key_status.second));
  }

  const std::string sb_drm_session_id =
      WvdmSessionIdToSbDrmSessionId(wvcdm_session_id);
  key_statuses_changed_callback_(this, context_, sb_drm_session_id.c_str(),
                                 sb_drm_session_id.size(), sb_key_ids.size(),
                                 sb_key_ids.data(), sb_key_statuses.data());
#else   // SB_HAS(DRM_KEY_STATUSES)
  SB_UNREFERENCED_PARAMETER(wvcdm_session_id);
#endif  // SB_HAS(DRM_KEY_STATUSES)
}

void DrmSystemWidevine::onRemoveComplete(const std::string& wvcdm_session_id) {
  SB_UNREFERENCED_PARAMETER(wvcdm_session_id);
  SB_NOTIMPLEMENTED();
}

void DrmSystemWidevine::onDeferredComplete(const std::string& wvcdm_session_id,
                                           wv3cdm::Status result) {
  SB_UNREFERENCED_PARAMETER(wvcdm_session_id);
  SB_UNREFERENCED_PARAMETER(result);
  SB_NOTIMPLEMENTED();
}

void DrmSystemWidevine::onDirectIndividualizationRequest(
    const std::string& wvcdm_session_id,
    const std::string& request) {
  SendSessionUpdateRequest(kSbDrmSessionRequestTypeIndividualizationRequest,
                           WvdmSessionIdToSbDrmSessionId(wvcdm_session_id),
                           request);
}

void DrmSystemWidevine::SetTicket(const std::string& sb_drm_session_id,
                                  int ticket) {
  SB_DCHECK(SbThreadGetId() == ticket_thread_id_)
      << "Ticket should only be set from the constructor thread.";
  sb_drm_session_id_to_ticket_map_[sb_drm_session_id] = ticket;
}

int DrmSystemWidevine::GetAndResetTicket(const std::string& sb_drm_session_id) {
  // Returning no ticket is a valid way to indicate that a host's method was
  // called spontaneously by CDM, potentially from the timer thread.
  if (SbThreadGetId() != ticket_thread_id_) {
    return kSbDrmTicketInvalid;
  }
  auto iter = sb_drm_session_id_to_ticket_map_.find(sb_drm_session_id);
  if (iter == sb_drm_session_id_to_ticket_map_.end()) {
    return kSbDrmTicketInvalid;
  }
  auto ticket = iter->second;
  sb_drm_session_id_to_ticket_map_.erase(iter);
  return ticket;
}

std::string DrmSystemWidevine::WvdmSessionIdToSbDrmSessionId(
    const std::string& wvcdm_session_id) {
#if SB_API_VERSION >= 10
  SB_DCHECK(wvcdm_session_id != kFirstSbDrmSessionId);
  if (wvcdm_session_id == first_wvcdm_session_id_) {
    return kFirstSbDrmSessionId;
  }
#endif  // SB_API_VERSION >= 10
  return wvcdm_session_id;
}

std::string DrmSystemWidevine::SbDrmSessionIdToWvdmSessionId(
    const void* sb_drm_session_id,
    int sb_drm_session_id_size) {
  const std::string str_sb_drm_session_id(
      static_cast<const char*>(sb_drm_session_id), sb_drm_session_id_size);
#if SB_API_VERSION >= 10
  if (str_sb_drm_session_id == kFirstSbDrmSessionId) {
    SB_DCHECK(!first_wvcdm_session_id_.empty());
    return first_wvcdm_session_id_;
  }
#endif  // SB_API_VERSION >= 10
  return str_sb_drm_session_id;
}

void DrmSystemWidevine::SendServerCertificateRequest(int ticket) {
  std::string message;
  auto status = cdm_->getServiceCertificateRequest(&message);
  if (status == wv3cdm::kSuccess) {
    SetTicket(kFirstSbDrmSessionId, ticket);
    // Note that calling createSession() without a server certificate may fail.
    // So use |kFirstSbDrmSessionId| as the session id.  Once we have a real
    // session id, we will map it from/to the fake session id.
    SendSessionUpdateRequest(kSbDrmSessionRequestTypeLicenseRequest,
                             kFirstSbDrmSessionId, message);
  } else {
// Signals failure by sending NULL as the session id.
#if SB_API_VERSION >= 10
    session_update_request_callback_(
        this, context_, ticket, CdmStatusToSbDrmStatus(status),
        kSbDrmSessionRequestTypeLicenseRequest, "", NULL, 0, NULL, 0, NULL);
#else   // SB_API_VERSION >= 10
    session_update_request_callback_(this, context_, ticket, NULL, 0, NULL, 0,
                                     NULL);
#endif  // SB_API_VERSION >= 10
  }
}

wv3cdm::Status DrmSystemWidevine::ProcessServerCertificateResponse(
    const std::string& response) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  is_server_certificate_set_ = false;
  std::string certificate;
  auto status = cdm_->parseServiceCertificateResponse(response, &certificate);
  if (status != wv3cdm::kSuccess) {
    return status;
  }
  status = cdm_->setServiceCertificate(certificate);
  is_server_certificate_set_ = (status == wv3cdm::kSuccess);
  return status;
}

void DrmSystemWidevine::TrySendPendingGenerateSessionUpdateRequests() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (!is_server_certificate_set_) {
    return;
  }
  decltype(pending_generate_session_update_requests_) pending_requests;
  pending_requests.swap(pending_generate_session_update_requests_);
  for (auto iter = pending_requests.begin(); iter != pending_requests.end();
       ++iter) {
    GenerateSessionUpdateRequestInternal(iter->ticket, iter->init_data_type,
                                         iter->initialization_data,
                                         iter == pending_requests.begin());
  }
}

void DrmSystemWidevine::SendSessionUpdateRequest(
    SbDrmSessionRequestType type,
    const std::string& sb_drm_session_id,
    const std::string& message) {
  int ticket = GetAndResetTicket(sb_drm_session_id);

#if SB_API_VERSION >= 10
  session_update_request_callback_(
      this, context_, ticket, kSbDrmStatusSuccess, type, "",
      sb_drm_session_id.c_str(), static_cast<int>(sb_drm_session_id.size()),
      message.c_str(), static_cast<int>(message.size()), NULL);
#else   // SB_API_VERSION >= 10
  session_update_request_callback_(
      this, context_, ticket, sb_drm_session_id.c_str(),
      static_cast<int>(sb_drm_session_id.size()), message.c_str(),
      static_cast<int>(message.size()), NULL);
#endif  // SB_API_VERSION >= 10
}

}  // namespace widevine
}  // namespace shared
}  // namespace starboard
