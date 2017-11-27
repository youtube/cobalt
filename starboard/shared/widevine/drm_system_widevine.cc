// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <map>
#include <set>
#include <string>
#include <vector>

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/string.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace widevine {

class SbDrmSystemWidevine::BufferImpl : public cdm::Buffer {
 public:
  BufferImpl() : size_(0) {}

  // cdm::Buffer methods
  void Destroy() override {}
  int32_t Capacity() const override { return buffer_.capacity(); }
  uint8_t* Data() override { return buffer_.empty() ? NULL : &buffer_[0]; }
  void SetSize(int32_t size) override {
    SB_DCHECK(size >= 0);
    SB_DCHECK(size <= buffer_.capacity());
    size_ = size;
  }
  int32_t Size() const override { return size_; }

  void Reset(int32_t new_capacity) {
    if (new_capacity > buffer_.capacity()) {
      buffer_.resize(new_capacity + 1024);
    }
    size_ = 0;
  }

 private:
  std::vector<uint8_t> buffer_;
  int32_t size_;
};

class SbDrmSystemWidevine::DecryptedBlockImpl : public cdm::DecryptedBlock {
 public:
  DecryptedBlockImpl() : buffer_(NULL), timestamp_(0) {}
  ~DecryptedBlockImpl() {
    if (buffer_) {
      buffer_->Destroy();
    }
  }

  // cdm::DecryptedBlock methods
  void SetDecryptedBuffer(cdm::Buffer* buffer) override { buffer_ = buffer; }
  cdm::Buffer* DecryptedBuffer() override { return buffer_; }
  void SetTimestamp(int64_t timestamp) override { timestamp_ = timestamp; }
  int64_t Timestamp() const override { return timestamp_; }

 private:
  cdm::Buffer* buffer_;
  int64_t timestamp_;
};

namespace {

const char kWidevineKeySystem[] = "com.widevine.alpha";

}  // namespace

SbDrmSystemWidevine::SbDrmSystemWidevine(
    void* context,
    SbDrmSessionUpdateRequestFunc session_update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback
#if SB_HAS(DRM_KEY_STATUSES)
    ,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback
#endif  // SB_HAS(DRM_KEY_STATUSES)
    )
    : context_(context),
      session_update_request_callback_(session_update_request_callback),
      session_updated_callback_(session_updated_callback),
#if SB_HAS(DRM_KEY_STATUSES)
      key_statuses_changed_callback_(key_statuses_changed_callback),
#endif  // SB_HAS(DRM_KEY_STATUSES)
      ticket_(kSbDrmTicketInvalid),
      ticket_thread_id_(SbThreadGetId()),
      buffer_(new BufferImpl()),
      cdm_(static_cast<cdm::ContentDecryptionModule*>(
          CreateCdmInstance(cdm::kCdmInterfaceVersion,
                            kWidevineKeySystem,
                            SbStringGetLength(kWidevineKeySystem),
                            GetHostInterface,
                            this))),
      quitting_(false),
      timer_thread_(SbThreadCreate(0,
                                   kSbThreadNoPriority,
                                   kSbThreadNoAffinity,
                                   true,
                                   "Widevine CDM Timer",
                                   TimerThreadFunc,
                                   this)) {
  SB_DCHECK(cdm_);
  SB_DCHECK(timer_thread_ != kSbThreadInvalid);
}

SbDrmSystemWidevine::~SbDrmSystemWidevine() {
  quitting_ = true;
  timer_queue_.Wake();
  SbThreadJoin(timer_thread_, NULL);

  cdm_->Destroy();

  delete buffer_;
}

void SbDrmSystemWidevine::GenerateSessionUpdateRequest(
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  // We assume that CDM is called from one thread and that CDM calls host's
  // methods synchronously on the same thread, so it shouldn't be possible
  // to request multiple concurrent session updates.
  SB_DCHECK(GetTicket() == kSbDrmTicketInvalid)
      << "Another session update request is already pending.";
  SetTicket(ticket);

  cdm::Status status = cdm_->GenerateKeyRequest(
      type, SbStringGetLength(type),
      reinterpret_cast<const uint8_t*>(initialization_data),
      initialization_data_size);

  if (status != cdm::kSuccess) {
    // Reset ticket before invoking user-provided callback to indicate that
    // no session update request is pending.
    SetTicket(kSbDrmTicketInvalid);

    SB_DLOG(ERROR) << "GenerateKeyRequest status " << status;
    // Send an empty request to signal an error.
    session_update_request_callback_(this, context_,
                                     ticket,
                                     NULL, 0, NULL, 0, NULL);
  }
}

void SbDrmSystemWidevine::UpdateSession(
    int ticket,
    const void* key,
    int key_size,
    const void* session_id,
    int session_id_size) {
  cdm::Status status =
      cdm_->AddKey(reinterpret_cast<const char*>(session_id), session_id_size,
                   reinterpret_cast<const uint8_t*>(key), key_size, NULL, 0);
  bool succeeded = status == cdm::kSuccess;
  SB_DLOG_IF(ERROR, !succeeded) << "AddKey status " << status;
  session_updated_callback_(this, context_,
                            ticket,
                            session_id, session_id_size, succeeded);

#if SB_HAS(DRM_KEY_STATUSES)
#if ENABLE_KEY_STATUSES_CALLBACK
  std::set<std::string> key_ids =
      cdm_->GetKeys(reinterpret_cast<const char*>(session_id), session_id_size);

  std::vector<SbDrmKeyId> sb_key_ids;
  for (auto& key_id : key_ids) {
    SbDrmKeyId sb_key_id;
    SB_DCHECK(key_id.size() <= sizeof(sb_key_id.identifier));
    SbMemoryCopy(sb_key_id.identifier, key_id.c_str(), key_id.size());
    sb_key_id.identifier_size = key_id.size();
    sb_key_ids.push_back(sb_key_id);
  }
  std::vector<SbDrmKeyStatus> sb_key_statuses(key_ids.size(),
                                              kSbDrmKeyStatusUsable);

  key_statuses_changed_callback_(this, context_, session_id, session_id_size,
                                 sb_key_ids.size(), sb_key_ids.data(),
                                 sb_key_statuses.data());
#endif  // ENABLE_KEY_STATUSES_CALLBACK
#endif  // SB_HAS(DRM_KEY_STATUSES)
}

void SbDrmSystemWidevine::CloseSession(const void* session_id,
                                       int session_id_size) {
  cdm_->CloseSession(reinterpret_cast<const char*>(session_id),
                     session_id_size);
}

SbDrmSystemPrivate::DecryptStatus SbDrmSystemWidevine::Decrypt(
    InputBuffer* buffer) {
  const SbDrmSampleInfo* drm_info = buffer->drm_info();

  if (drm_info == NULL || drm_info->initialization_vector_size == 0) {
    return kSuccess;
  }

  // Adapt |buffer| and |drm_info| to a |cdm::InputBuffer|.
  cdm::InputBuffer input;
  input.data = buffer->data();
  input.data_size = buffer->size();
  input.data_offset = 0;
  input.key_id = drm_info->identifier;
  input.key_id_size = drm_info->identifier_size;
  input.iv = drm_info->initialization_vector;
  input.iv_size = drm_info->initialization_vector_size;

  std::vector<cdm::SubsampleEntry> subsample_entries;
  if (drm_info->subsample_count == 0) {
    // The WideVine decryptor requires at least one subsample.  So we create a
    // dummy subsample covering all media data.
    subsample_entries.push_back(cdm::SubsampleEntry(0, buffer->size()));
  } else {
    for (int i = 0; i < drm_info->subsample_count; ++i) {
      subsample_entries.push_back(cdm::SubsampleEntry(
          drm_info->subsample_mapping[i].clear_byte_count,
          drm_info->subsample_mapping[i].encrypted_byte_count));
    }
  }
  input.subsamples = &subsample_entries[0];
  input.num_subsamples = subsample_entries.size();
  // Convert |pts| in SbMediaTime into |timestamp| in SbTime.
  input.timestamp = buffer->pts() * kSbTimeSecond / kSbMediaTimeSecond;

  DecryptedBlockImpl output;
  cdm::Status status;

  // This check is a workaround for bug in WV 2.2 that will return kDecryptError
  // when the key is not ready.  It is not necessary for WV 3.x.  Remove this
  // once we finish upgrading to WV 3.x.
  if (cdm_->IsKeyValid(input.key_id, input.key_id_size)) {
    status = cdm_->Decrypt(input, &output);
  } else {
    status = cdm::kNoKey;
  }

  if (status == cdm::kSuccess) {
    // Copy the output from our recycled temporary buffer into the shell buffer
    // we got as input.
    BufferImpl* decrypted = static_cast<BufferImpl*>(output.DecryptedBuffer());
    buffer->SetDecryptedContent(decrypted->Data(), decrypted->Size());
    return kSuccess;
  } else if (status == cdm::kNoKey) {
    return kRetry;
  } else {
    SB_DLOG(ERROR) << "Decrypt status " << status;
    return kFailure;
  }
}

cdm::Buffer* SbDrmSystemWidevine::Allocate(int32_t capacity) {
  // We recycle/grow a single buffer for each Decrypt call as needed.
  buffer_->Reset(capacity);
  // Call SetSize explicitly as it is not properly handled in the current
  // version of wvcdm.
  buffer_->SetSize(capacity);
  return buffer_;
}

// Set time from now (in milliseconds) when the timer should fire.
// This is not a recurring request, and multiple outstanding timers may exist.
void SbDrmSystemWidevine::SetTimer(int64_t delay_in_milliseconds,
                                   void* context) {
  timer_queue_.Put(Timer(delay_in_milliseconds, context));
}

double SbDrmSystemWidevine::GetCurrentWallTimeInSeconds() {
  return SbTimeGetMonotonicNow() / static_cast<double>(kSbTimeSecond);
}

void SbDrmSystemWidevine::SendKeyMessage(const char* web_session_id,
                                         int32_t web_session_id_length,
                                         const char* message,
                                         int32_t message_length,
                                         const char* default_url,
                                         int32_t default_url_length) {
  SB_DCHECK(SbStringGetLength(default_url) == default_url_length);

  int ticket = GetTicket();
  if (SbDrmTicketIsValid(ticket)) {
    // Reset ticket before invoking user-provided callback to indicate that
    // no session update request is pending.
    SetTicket(kSbDrmTicketInvalid);
  }

  session_update_request_callback_(this, context_,
                                   ticket,
                                   web_session_id, web_session_id_length,
                                   message, message_length, default_url);
}

void SbDrmSystemWidevine::SendKeyError(const char* web_session_id,
                                       int32_t web_session_id_length,
                                       cdm::MediaKeyError error_code,
                                       uint32_t system_code) {
#if SB_HAS(DRM_KEY_STATUSES)
#if ENABLE_KEY_STATUSES_CALLBACK
  // CDM may call this method in response to |GenerateKeyRequest| and |AddKey|,
  // as well as spontaneously.
  //
  std::set<std::string> key_ids =
      cdm_->GetKeys(web_session_id, web_session_id_length);

  std::vector<SbDrmKeyId> sb_key_ids;
  for (auto& key_id : key_ids) {
    SbDrmKeyId sb_key_id;
    SB_DCHECK(key_id.size() < sizeof(sb_key_id.identifier));
    SbMemoryCopy(sb_key_id.identifier, key_id.c_str(), key_id.size());
    sb_key_id.identifier_size = key_id.size();
    sb_key_ids.push_back(sb_key_id);
  }
  std::vector<SbDrmKeyStatus> sb_key_statuses(key_ids.size(),
                                              kSbDrmKeyStatusExpired);

  key_statuses_changed_callback_(this, context_, web_session_id,
                                 web_session_id_length, sb_key_ids.size(),
                                 sb_key_ids.data(), sb_key_statuses.data());
#endif  // ENABLE_KEY_STATUSES_CALLBACK
#endif  // SB_HAS(DRM_KEY_STATUSES)
}

namespace {

// The x509 certificate required to enable privacy mode.
// Privacy mode encrypts the client identification in a license request.
const char kServiceCertificate[] = {
    0x08, 0x03, 0x12, 0x10, 0x17, 0x05, 0xB9, 0x17, 0xCC, 0x12, 0x04, 0x86,
    0x8B, 0x06, 0x33, 0x3A, 0x2F, 0x77, 0x2A, 0x8C, 0x18, 0x82, 0xB4, 0x82,
    0x92, 0x05, 0x22, 0x8E, 0x02, 0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01,
    0x01, 0x00, 0x99, 0xED, 0x5B, 0x3B, 0x32, 0x7D, 0xAB, 0x5E, 0x24, 0xEF,
    0xC3, 0xB6, 0x2A, 0x95, 0xB5, 0x98, 0x52, 0x0A, 0xD5, 0xBC, 0xCB, 0x37,
    0x50, 0x3E, 0x06, 0x45, 0xB8, 0x14, 0xD8, 0x76, 0xB8, 0xDF, 0x40, 0x51,
    0x04, 0x41, 0xAD, 0x8C, 0xE3, 0xAD, 0xB1, 0x1B, 0xB8, 0x8C, 0x4E, 0x72,
    0x5A, 0x5E, 0x4A, 0x9E, 0x07, 0x95, 0x29, 0x1D, 0x58, 0x58, 0x40, 0x23,
    0xA7, 0xE1, 0xAF, 0x0E, 0x38, 0xA9, 0x12, 0x79, 0x39, 0x30, 0x08, 0x61,
    0x0B, 0x6F, 0x15, 0x8C, 0x87, 0x8C, 0x7E, 0x21, 0xBF, 0xFB, 0xFE, 0xEA,
    0x77, 0xE1, 0x01, 0x9E, 0x1E, 0x57, 0x81, 0xE8, 0xA4, 0x5F, 0x46, 0x26,
    0x3D, 0x14, 0xE6, 0x0E, 0x80, 0x58, 0xA8, 0x60, 0x7A, 0xDC, 0xE0, 0x4F,
    0xAC, 0x84, 0x57, 0xB1, 0x37, 0xA8, 0xD6, 0x7C, 0xCD, 0xEB, 0x33, 0x70,
    0x5D, 0x98, 0x3A, 0x21, 0xFB, 0x4E, 0xEC, 0xBD, 0x4A, 0x10, 0xCA, 0x47,
    0x49, 0x0C, 0xA4, 0x7E, 0xAA, 0x5D, 0x43, 0x82, 0x18, 0xDD, 0xBA, 0xF1,
    0xCA, 0xDE, 0x33, 0x92, 0xF1, 0x3D, 0x6F, 0xFB, 0x64, 0x42, 0xFD, 0x31,
    0xE1, 0xBF, 0x40, 0xB0, 0xC6, 0x04, 0xD1, 0xC4, 0xBA, 0x4C, 0x95, 0x20,
    0xA4, 0xBF, 0x97, 0xEE, 0xBD, 0x60, 0x92, 0x9A, 0xFC, 0xEE, 0xF5, 0x5B,
    0xBA, 0xF5, 0x64, 0xE2, 0xD0, 0xE7, 0x6C, 0xD7, 0xC5, 0x5C, 0x73, 0xA0,
    0x82, 0xB9, 0x96, 0x12, 0x0B, 0x83, 0x59, 0xED, 0xCE, 0x24, 0x70, 0x70,
    0x82, 0x68, 0x0D, 0x6F, 0x67, 0xC6, 0xD8, 0x2C, 0x4A, 0xC5, 0xF3, 0x13,
    0x44, 0x90, 0xA7, 0x4E, 0xEC, 0x37, 0xAF, 0x4B, 0x2F, 0x01, 0x0C, 0x59,
    0xE8, 0x28, 0x43, 0xE2, 0x58, 0x2F, 0x0B, 0x6B, 0x9F, 0x5D, 0xB0, 0xFC,
    0x5E, 0x6E, 0xDF, 0x64, 0xFB, 0xD3, 0x08, 0xB4, 0x71, 0x1B, 0xCF, 0x12,
    0x50, 0x01, 0x9C, 0x9F, 0x5A, 0x09, 0x02, 0x03, 0x01, 0x00, 0x01, 0x3A,
    0x14, 0x6C, 0x69, 0x63, 0x65, 0x6E, 0x73, 0x65, 0x2E, 0x77, 0x69, 0x64,
    0x65, 0x76, 0x69, 0x6E, 0x65, 0x2E, 0x63, 0x6F, 0x6D,
};

}  // namespace

void SbDrmSystemWidevine::GetPlatformString(const std::string& name,
                                            std::string* value) {
  if (name == "PrivacyOn") {
    value->assign("True");
  } else if (name == "ServiceCertificate") {
    value->assign(kServiceCertificate, sizeof(kServiceCertificate));
  } else {
    SB_DLOG(WARNING) << "GetPlatformString(" << name << ")";
    value->clear();
  }
}

void SbDrmSystemWidevine::SetPlatformString(const std::string& name,
                                            const std::string& value) {
  SB_NOTIMPLEMENTED();
}

// static
void* SbDrmSystemWidevine::GetHostInterface(int host_interface_version,
                                            void* user_data) {
  if (host_interface_version != cdm::kHostInterfaceVersion)
    return NULL;
  return static_cast<cdm::Host*>(static_cast<SbDrmSystemWidevine*>(user_data));
}

void SbDrmSystemWidevine::TimerThread() {
  std::map<SbTimeMonotonic, void*> timers;
  for (;;) {
    while (!timers.empty() && timers.begin()->first < SbTimeGetMonotonicNow()) {
      cdm_->TimerExpired(timers.begin()->second);
      timers.erase(timers.begin());
    }

    Timer timer;
    if (timers.empty()) {
      timer = timer_queue_.Get();
    } else {
      SbTimeMonotonic delay = timers.begin()->first - SbTimeGetMonotonicNow();
      if (delay > 0) {
        timer = timer_queue_.GetTimed(delay);
      }
    }
    if (quitting_) {
      break;
    }
    if (timer.time_to_fire != 0) {
      while (timers.find(timer.time_to_fire) != timers.end()) {
        ++timer.time_to_fire;
      }
      timers[timer.time_to_fire] = timer.context;
    }
  }
}

// static
void* SbDrmSystemWidevine::TimerThreadFunc(void* context) {
  SbDrmSystemWidevine* drm_system = static_cast<SbDrmSystemWidevine*>(context);
  SB_DCHECK(drm_system);
  drm_system->TimerThread();
  return NULL;
}

void SbDrmSystemWidevine::SetTicket(int ticket) {
  SB_DCHECK(SbThreadGetId() == ticket_thread_id_)
      << "Ticket should only be set from the constructor thread.";
  ticket_ = ticket;
}

int SbDrmSystemWidevine::GetTicket() const {
  // Returning no ticket is a valid way to indicate that a host's method was
  // called spontaneously by CDM, potentially from the timer thread.
  return SbThreadGetId() == ticket_thread_id_ ? ticket_ : kSbDrmTicketInvalid;
}

}  // namespace widevine
}  // namespace shared
}  // namespace starboard
