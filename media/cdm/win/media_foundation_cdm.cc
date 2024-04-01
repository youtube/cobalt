// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm.h"

#include <mferror.h>

#include <stdlib.h>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_promise.h"
#include "media/base/win/media_foundation_cdm_proxy.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/media_foundation_cdm_module.h"
#include "media/cdm/win/media_foundation_cdm_session.h"

namespace media {

namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Exception = CdmPromise::Exception;

// GUID is little endian. The byte array in network order is big endian.
std::vector<uint8_t> ByteArrayFromGUID(REFGUID guid) {
  std::vector<uint8_t> byte_array(sizeof(GUID));
  GUID* reversed_guid = reinterpret_cast<GUID*>(byte_array.data());
  *reversed_guid = guid;
  reversed_guid->Data1 = _byteswap_ulong(guid.Data1);
  reversed_guid->Data2 = _byteswap_ushort(guid.Data2);
  reversed_guid->Data3 = _byteswap_ushort(guid.Data3);
  // Data4 is already a byte array so no need to byte swap.
  return byte_array;
}

HRESULT CreatePolicySetEvent(ComPtr<IMFMediaEvent>& policy_set_event) {
  base::win::ScopedPropVariant policy_set_prop;
  PROPVARIANT* var_to_set = policy_set_prop.Receive();
  var_to_set->vt = VT_UI4;
  var_to_set->ulVal = 0;
  RETURN_IF_FAILED(MFCreateMediaEvent(
      MEPolicySet, GUID_NULL, S_OK, policy_set_prop.ptr(), &policy_set_event));
  return S_OK;
}

// Notifies the Decryptor about the last key ID so the decryptor can prefetch
// the corresponding key to reduce start-to-play time when resuming playback.
// This is done by sending a MEContentProtectionMetadata event.
HRESULT RefreshDecryptor(IMFTransform* decryptor,
                         const GUID& protection_system_id,
                         const GUID& last_key_id) {
  // The MFT_MESSAGE_NOTIFY_START_OF_STREAM message is usually sent by the MF
  // pipeline when starting playback. Here we send it out-of-band as it is a
  // pre-requisite for getting the decryptor to process the
  // MEContentProtectionMetadata event.
  RETURN_IF_FAILED(
      decryptor->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

  // After receiving a MEContentProtectionMetadata event, the Decryptor
  // requires that it is notified of a MEPolicySet event to continue decryption.
  ComPtr<IMFMediaEvent> policy_set_event;
  RETURN_IF_FAILED(CreatePolicySetEvent(policy_set_event));
  RETURN_IF_FAILED(decryptor->ProcessMessage(
      MFT_MESSAGE_NOTIFY_EVENT,
      reinterpret_cast<ULONG_PTR>(policy_set_event.Get())));

  // Prepare the MEContentProtectionMetadata event.
  ComPtr<IMFMediaEvent> key_rotation_event;
  RETURN_IF_FAILED(MFCreateMediaEvent(MEContentProtectionMetadata, GUID_NULL,
                                      S_OK, nullptr, &key_rotation_event));

  // MF_EVENT_STREAM_METADATA_SYSTEMID expects the system ID (GUID) to be in
  // little endian order. So no need to call `ByteArrayFromGUID()`.
  RETURN_IF_FAILED(key_rotation_event->SetBlob(
      MF_EVENT_STREAM_METADATA_SYSTEMID,
      reinterpret_cast<const uint8_t*>(&protection_system_id), sizeof(GUID)));

  std::vector<uint8_t> last_key_id_byte_array = ByteArrayFromGUID(last_key_id);
  RETURN_IF_FAILED(key_rotation_event->SetBlob(
      MF_EVENT_STREAM_METADATA_CONTENT_KEYIDS, last_key_id_byte_array.data(),
      last_key_id_byte_array.size()));

  // The `dwInputStreamID` refers to a local stream ID of the Decryptor. Since
  // Decryptors typically only support a single stream, always pass 0 here.
  RETURN_IF_FAILED(
      decryptor->ProcessEvent(/*dwInputStreamID=*/0, key_rotation_event.Get()));
  return S_OK;
}

// The HDCP value follows the feature value in
// https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.protectioncapabilities.istypesupported?view=winrt-19041
// - 0 (off)
// - 1 (on without HDCP 2.2 Type 1 restriction)
// - 2 (on with HDCP 2.2 Type 1 restriction)
int GetHdcpValue(HdcpVersion hdcp_version) {
  switch (hdcp_version) {
    case HdcpVersion::kHdcpVersionNone:
      return 0;
    case HdcpVersion::kHdcpVersion1_0:
    case HdcpVersion::kHdcpVersion1_1:
    case HdcpVersion::kHdcpVersion1_2:
    case HdcpVersion::kHdcpVersion1_3:
    case HdcpVersion::kHdcpVersion1_4:
    case HdcpVersion::kHdcpVersion2_0:
    case HdcpVersion::kHdcpVersion2_1:
      return 1;
    case HdcpVersion::kHdcpVersion2_2:
    case HdcpVersion::kHdcpVersion2_3:
      return 2;
  }
}

class CdmProxyImpl : public MediaFoundationCdmProxy {
 public:
  CdmProxyImpl(ComPtr<IMFContentDecryptionModule> mf_cdm,
               base::RepeatingClosure hardware_context_reset_cb)
      : mf_cdm_(mf_cdm),
        hardware_context_reset_cb_(std::move(hardware_context_reset_cb)) {}

  // MediaFoundationCdmProxy implementation

  HRESULT GetPMPServer(REFIID riid, LPVOID* object_result) override {
    DVLOG_FUNC(1);
    ComPtr<IMFGetService> cdm_services;
    RETURN_IF_FAILED(mf_cdm_.As(&cdm_services));
    RETURN_IF_FAILED(cdm_services->GetService(
        MF_CONTENTDECRYPTIONMODULE_SERVICE, riid, object_result));
    return S_OK;
  }

  HRESULT GetInputTrustAuthority(uint32_t stream_id,
                                 uint32_t /*stream_count*/,
                                 const uint8_t* content_init_data,
                                 uint32_t content_init_data_size,
                                 REFIID riid,
                                 IUnknown** object_out) override {
    DVLOG_FUNC(1);

    if (input_trust_authorities_.count(stream_id)) {
      RETURN_IF_FAILED(input_trust_authorities_[stream_id].CopyTo(object_out));
      return S_OK;
    }

    if (!trusted_input_) {
      RETURN_IF_FAILED(mf_cdm_->CreateTrustedInput(
          content_init_data, content_init_data_size, &trusted_input_));
    }

    // GetInputTrustAuthority takes IUnknown* as the output. Using other COM
    // interface will have a v-table mismatch issue.
    ComPtr<IUnknown> unknown;
    RETURN_IF_FAILED(
        trusted_input_->GetInputTrustAuthority(stream_id, riid, &unknown));

    ComPtr<IMFInputTrustAuthority> input_trust_authority;
    RETURN_IF_FAILED(unknown.As(&input_trust_authority));
    RETURN_IF_FAILED(unknown.CopyTo(object_out));

    // Success! Store ITA in the map.
    input_trust_authorities_[stream_id] = input_trust_authority;

    return S_OK;
  }

  HRESULT SetLastKeyId(uint32_t stream_id, REFGUID key_id) override {
    DVLOG_FUNC(1);
    last_key_ids_[stream_id] = key_id;
    return S_OK;
  }

  HRESULT RefreshTrustedInput() override {
    DVLOG_FUNC(1);

    // Refresh all decryptors of the last key IDs.
    for (const auto& entry : input_trust_authorities_) {
      const auto& stream_id = entry.first;
      const auto& input_trust_authority = entry.second;

      const auto& last_key_id = last_key_ids_[stream_id];
      if (last_key_id == GUID_NULL)
        continue;

      ComPtr<IMFTransform> decryptor;
      RETURN_IF_FAILED(
          input_trust_authority->GetDecrypter(IID_PPV_ARGS(&decryptor)));
      GUID protection_system_id;
      RETURN_IF_FAILED(GetProtectionSystemId(&protection_system_id));
      RETURN_IF_FAILED(
          RefreshDecryptor(decryptor.Get(), protection_system_id, last_key_id));
    }

    input_trust_authorities_.clear();
    last_key_ids_.clear();
    return S_OK;
  }

  HRESULT
  ProcessContentEnabler(IUnknown* request, IMFAsyncResult* result) override {
    DVLOG_FUNC(1);
    ComPtr<IMFContentEnabler> content_enabler;
    RETURN_IF_FAILED(request->QueryInterface(IID_PPV_ARGS(&content_enabler)));
    return mf_cdm_->SetContentEnabler(content_enabler.Get(), result);
  }

  void OnHardwareContextReset() override {
    // Hardware context reset happens, all the crypto sessions are in invalid
    // states. So drop everything here.
    // TODO(xhwang): Keep the `last_key_ids_` here for faster resume.
    trusted_input_.Reset();
    input_trust_authorities_.clear();
    last_key_ids_.clear();

    // Must be the last call because `this` could be destructed when running
    // the callback. We are not certain because `this` is ref-counted.
    hardware_context_reset_cb_.Run();
  }

 private:
  ~CdmProxyImpl() override = default;

  HRESULT GetProtectionSystemId(GUID* protection_system_id) {
    // Typically the CDM should only return one protection system ID. So just
    // use the first one if available.
    base::win::ScopedCoMem<GUID> protection_system_ids;
    DWORD count = 0;
    RETURN_IF_FAILED(
        mf_cdm_->GetProtectionSystemIds(&protection_system_ids, &count));
    if (count == 0)
      return E_FAIL;

    *protection_system_id = *protection_system_ids;
    DVLOG(2) << __func__ << " protection_system_id="
             << base::win::WStringFromGUID(*protection_system_id);

    return S_OK;
  }

  ComPtr<IMFContentDecryptionModule> mf_cdm_;

  // A callback to notify hardware context reset.
  base::RepeatingClosure hardware_context_reset_cb_;

  // Store IMFTrustedInput to avoid potential performance cost.
  ComPtr<IMFTrustedInput> trusted_input_;

  // |stream_id| to IMFInputTrustAuthority (ITA) mapping. Serves two purposes:
  // 1. The same ITA should always be returned in GetInputTrustAuthority() for
  // the same |stream_id|.
  // 2. The ITA must keep alive for decryptors to work.
  std::map<uint32_t, ComPtr<IMFInputTrustAuthority>> input_trust_authorities_;

  // |stream_id| to last used key ID mapping.
  std::map<uint32_t, GUID> last_key_ids_;
};

}  // namespace

// static
bool MediaFoundationCdm::IsAvailable() {
  return base::win::GetVersion() >= base::win::Version::WIN10_20H1;
}

MediaFoundationCdm::MediaFoundationCdm(
    const CreateMFCdmCB& create_mf_cdm_cb,
    const IsTypeSupportedCB& is_type_supported_cb,
    const StoreClientTokenCB& store_client_token_cb,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : create_mf_cdm_cb_(create_mf_cdm_cb),
      is_type_supported_cb_(is_type_supported_cb),
      store_client_token_cb_(store_client_token_cb),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {
  DVLOG_FUNC(1);
  DCHECK(create_mf_cdm_cb_);
  DCHECK(is_type_supported_cb_);
  DCHECK(session_message_cb_);
  DCHECK(session_closed_cb_);
  DCHECK(session_keys_change_cb_);
  DCHECK(session_expiration_update_cb_);
}

MediaFoundationCdm::~MediaFoundationCdm() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationCdm::Initialize() {
  HRESULT hresult = E_FAIL;
  ComPtr<IMFContentDecryptionModule> mf_cdm;
  create_mf_cdm_cb_.Run(hresult, mf_cdm);
  if (!mf_cdm) {
    DCHECK(FAILED(hresult));
    return hresult;
  }

  mf_cdm_.Swap(mf_cdm);
  return S_OK;
}

void MediaFoundationCdm::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  if (!mf_cdm_) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "CDM Unavailable");
    return;
  }

  if (FAILED(mf_cdm_->SetServerCertificate(certificate.data(),
                                           certificate.size()))) {
    promise->reject(Exception::NOT_SUPPORTED_ERROR, 0, "Failed to set cert");
    return;
  }

  promise->resolve();
}

void MediaFoundationCdm::GetStatusForPolicy(
    HdcpVersion min_hdcp_version,
    std::unique_ptr<KeyStatusCdmPromise> promise) {
  if (!mf_cdm_) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "CDM Unavailable");
    return;
  }

  // Keys should be always usable when there is no HDCP requirement.
  if (min_hdcp_version == HdcpVersion::kHdcpVersionNone) {
    promise->resolve(CdmKeyInformation::KeyStatus::USABLE);
    return;
  }

  // HDCP is independent to the codec. So query H.264, which is always supported
  // by MFCDM.
  const std::string content_type =
      base::StringPrintf("video/mp4;codecs=\"avc1\";features=\"hdcp=%d\"",
                         GetHdcpValue(min_hdcp_version));

  is_type_supported_cb_.Run(
      content_type,
      base::BindOnce(&MediaFoundationCdm::OnIsTypeSupportedResult,
                     weak_factory_.GetWeakPtr(), std::move(promise)));
}

void MediaFoundationCdm::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG_FUNC(1);

  if (!mf_cdm_) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "CDM Unavailable");
    return;
  }

  // TODO(xhwang): Implement session expiration update.
  auto session = std::make_unique<MediaFoundationCdmSession>(
      session_message_cb_, session_keys_change_cb_,
      session_expiration_update_cb_);

  if (FAILED(session->Initialize(mf_cdm_.Get(), session_type))) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0,
                    "Failed to create session");
    return;
  }

  int session_token = next_session_token_++;

  // Keep a raw pointer since the |promise| will be moved to the callback.
  // Use base::Unretained() is safe because |session| is owned by |this|.
  auto* raw_promise = promise.get();
  auto session_id_cb =
      base::BindOnce(&MediaFoundationCdm::OnSessionId, base::Unretained(this),
                     session_token, std::move(promise));

  if (FAILED(session->GenerateRequest(init_data_type, init_data,
                                      std::move(session_id_cb)))) {
    raw_promise->reject(Exception::INVALID_STATE_ERROR, 0, "Init failure");
    return;
  }

  pending_sessions_.emplace(session_token, std::move(session));
}

void MediaFoundationCdm::LoadSession(
    CdmSessionType session_type,
    const std::string& session_id,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG_FUNC(1);

  if (!mf_cdm_) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "CDM Unavailable");
    return;
  }

  NOTIMPLEMENTED();
  promise->reject(Exception::NOT_SUPPORTED_ERROR, 0, "Load not supported");
}

void MediaFoundationCdm::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  if (!mf_cdm_) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "CDM Unavailable");
    return;
  }

  auto* session = GetSession(session_id);
  if (!session) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Session not found");
    return;
  }

  if (FAILED(session->Update(response))) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Update failed");
    return;
  }

  // Failure to store the client token will not prevent the CDM from correctly
  // functioning.
  StoreClientTokenIfNeeded();

  promise->resolve();
}

void MediaFoundationCdm::CloseSession(
    const std::string& session_id,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  CloseSessionInternal(session_id, CdmSessionClosedReason::kClose,
                       std::move(promise));
}

void MediaFoundationCdm::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  if (!mf_cdm_) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "CDM Unavailable");
    return;
  }

  auto* session = GetSession(session_id);
  if (!session) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Session not found");
    return;
  }

  if (FAILED(session->Remove())) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Remove failed");
    return;
  }

  promise->resolve();
}

CdmContext* MediaFoundationCdm::GetCdmContext() {
  return this;
}

bool MediaFoundationCdm::RequiresMediaFoundationRenderer() {
  return true;
}

bool MediaFoundationCdm::GetMediaFoundationCdmProxy(
    GetMediaFoundationCdmProxyCB get_mf_cdm_proxy_cb) {
  DVLOG_FUNC(1);

  if (!mf_cdm_) {
    DLOG(ERROR) << __func__ << ": Invalid state with null `mf_cdm_`";
    return false;
  }

  if (!cdm_proxy_) {
    cdm_proxy_ = base::MakeRefCounted<CdmProxyImpl>(
        mf_cdm_,
        base::BindRepeating(&MediaFoundationCdm::OnHardwareContextReset,
                            weak_factory_.GetWeakPtr()));
  }

  BindToCurrentLoop(std::move(get_mf_cdm_proxy_cb)).Run(cdm_proxy_);
  return true;
}

bool MediaFoundationCdm::OnSessionId(
    int session_token,
    std::unique_ptr<NewSessionCdmPromise> promise,
    const std::string& session_id) {
  DVLOG_FUNC(1) << "session_token=" << session_token
                << ", session_id=" << session_id;

  auto itr = pending_sessions_.find(session_token);
  DCHECK(itr != pending_sessions_.end());
  auto session = std::move(itr->second);
  DCHECK(session);
  pending_sessions_.erase(itr);

  if (session_id.empty() || sessions_.count(session_id)) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0,
                    "Empty or duplicate session ID");
    return false;
  }

  sessions_.emplace(session_id, std::move(session));
  promise->resolve(session_id);
  return true;
}

MediaFoundationCdmSession* MediaFoundationCdm::GetSession(
    const std::string& session_id) {
  auto itr = sessions_.find(session_id);
  if (itr == sessions_.end())
    return nullptr;

  return itr->second.get();
}

void MediaFoundationCdm::CloseSessionInternal(
    const std::string& session_id,
    CdmSessionClosedReason reason,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  if (!mf_cdm_) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "CDM Unavailable");
    return;
  }

  // Validate that this is a reference to an open session. close() shouldn't
  // be called if the session is already closed. However, the operation is
  // asynchronous, so there is a window where close() was called a second time
  // just before the closed event arrives. As a result it is possible that the
  // session is already closed, so assume that the session is closed if it
  // doesn't exist. https://github.com/w3c/encrypted-media/issues/365.
  //
  // close() is called from a MediaKeySession object, so it is unlikely that
  // this method will be called with a previously unseen |session_id|.
  auto* session = GetSession(session_id);
  if (!session) {
    promise->resolve();
    return;
  }

  if (FAILED(session->Close())) {
    sessions_.erase(session_id);
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Close failed");
    return;
  }

  // EME requires running session closed algorithm before resolving the promise.
  sessions_.erase(session_id);
  session_closed_cb_.Run(session_id, reason);
  promise->resolve();
}

// When hardware context is reset, all sessions are in a bad state. Close all
// the sessions and hopefully the player will create new sessions to resume.
void MediaFoundationCdm::OnHardwareContextReset() {
  DVLOG_FUNC(1);

  // Collect all the session IDs to avoid iterating the map while we delete
  // entries in the map (in `CloseSession()`).
  std::vector<std::string> session_ids;
  for (const auto& s : sessions_)
    session_ids.push_back(s.first);

  for (const auto& session_id : session_ids) {
    CloseSessionInternal(session_id,
                         CdmSessionClosedReason::kHardwareContextReset,
                         std::make_unique<DoNothingCdmPromise<>>());
  }

  cdm_proxy_.reset();

  // Reset IMFContentDecryptionModule which also holds the old ITA.
  mf_cdm_.Reset();

  // Recreates IMFContentDecryptionModule so we can create new sessions.
  if (FAILED(Initialize())) {
    DLOG(ERROR) << __func__ << ": Re-initialization failed";
    DCHECK(!mf_cdm_);
  }
}

void MediaFoundationCdm::OnIsTypeSupportedResult(
    std::unique_ptr<KeyStatusCdmPromise> promise,
    bool is_supported) {
  if (is_supported) {
    promise->resolve(CdmKeyInformation::KeyStatus::USABLE);
  } else {
    promise->resolve(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED);
  }
}

void MediaFoundationCdm::StoreClientTokenIfNeeded() {
  DVLOG_FUNC(1);

  ComPtr<IMFAttributes> attributes;
  if (FAILED(mf_cdm_.As(&attributes))) {
    DLOG(ERROR) << "Failed to access the CDM's IMFAttribute store";
    return;
  }

  base::win::ScopedCoMem<uint8_t> client_token;
  uint32_t client_token_size;

  HRESULT hr = attributes->GetAllocatedBlob(
      EME_CONTENTDECRYPTIONMODULE_CLIENT_TOKEN.fmtid, &client_token,
      &client_token_size);
  if (FAILED(hr)) {
    if (hr != MF_E_ATTRIBUTENOTFOUND)
      DLOG(ERROR) << "Failed to get the client token blob. hr=" << hr;
    return;
  }

  DVLOG(2) << "Got client token of size " << client_token_size;

  std::vector<uint8_t> client_token_vector;
  client_token_vector.assign(client_token.get(),
                             client_token.get() + client_token_size);

  // The store operation is cross-process so only run it if we have a new
  // client token.
  if (client_token_vector == cached_client_token_)
    return;

  cached_client_token_ = client_token_vector;
  store_client_token_cb_.Run(cached_client_token_);
}

}  // namespace media
