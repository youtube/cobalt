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

#include <mfidl.h>
#include <windows.media.protection.playready.h>
#include <wrl.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/optional.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/drm_system_playready.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/wrm_header.h"

namespace starboard {
namespace xb1 {
namespace shared {
namespace {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::win32::DrmSystemPlayready;
using ::starboard::shared::win32::WrmHeader;

class PlayreadyLicense : public DrmSystemPlayready::License {
 public:
  PlayreadyLicense(const void* initialization_data,
                   int initialization_data_size);
  ~PlayreadyLicense() override;

  GUID key_id() const override { return key_id_; }
  bool usable() const override { return usable_; }
  std::string license_challenge() const override { return license_challenge_; }
  Microsoft::WRL::ComPtr<IMFTransform> decryptor() override {
    return decryptor_;
  }

  void UpdateLicense(const void* license, int license_size) override;
  bool IsHDCPRequired() override;

 private:
  bool CreateTrustedInputAndTrustedInputAuthority(
      Microsoft::WRL::ComPtr<IStream> content_header);
  void GenerateChallenge();
  void UpdateLicenseInternal();

  WrmHeader wrm_header_;
  GUID key_id_;
  Microsoft::WRL::ComPtr<IMFTrustedInput> trusted_input_;
  Microsoft::WRL::ComPtr<IMFInputTrustAuthority> trusted_input_authority_;
  Microsoft::WRL::ComPtr<IMFTransform> decryptor_;

  Windows::Media::Protection::PlayReady::IPlayReadyServiceRequest ^
      service_request_;
  std::string license_challenge_;
  bool usable_;
  optional<bool> hdcp_required_;

  // The following variables are not really instance variables.  They are saved
  // in the class to simplify the passing of data to functions run on newly
  // created MTA threads.
  Platform::Array<unsigned char> ^ license_response_;
};

typedef void (PlayreadyLicense::*PlayreadyLicenseMemberFunction)();

struct ThreadParam {
  PlayreadyLicenseMemberFunction function;
  PlayreadyLicense* license_object;
};

DWORD WINAPI MTAThreadFunc(void* param) {
  SB_DCHECK(param);

  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ThreadParam* thread_param = static_cast<ThreadParam*>(param);
  ((*thread_param->license_object).*(thread_param->function))();
  CoUninitialize();

  return 0;
}

// Playready licensing functions are required to run in MTA.  This function
// makes it easy to run a particular member function in MTA synchronously.
void RunFunctionOnMTA(PlayreadyLicenseMemberFunction function,
                      PlayreadyLicense* license_object) {
  DWORD thread_id;
  ThreadParam param = {function, license_object};
  // Use plain vanilla Windows thread to avoid any side effect.
  HANDLE thread = CreateThread(NULL, 0, MTAThreadFunc, &param, 0, &thread_id);
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
}

PlayreadyLicense::PlayreadyLicense(const void* initialization_data,
                                   int initialization_data_size)
    : wrm_header_(initialization_data, initialization_data_size),
      key_id_(WrmHeader::kInvalidKeyId),
      service_request_(nullptr),
      license_response_(nullptr),
      usable_(false) {
  SB_DCHECK(wrm_header_.is_valid());
  if (!wrm_header_.is_valid()) {
    return;
  }
  if (CreateTrustedInputAndTrustedInputAuthority(
          wrm_header_.content_header())) {
    RunFunctionOnMTA(&PlayreadyLicense::GenerateChallenge, this);
    key_id_ = wrm_header_.key_id();
  }
}

PlayreadyLicense::~PlayreadyLicense() {
  decryptor_.Reset();
  if (trusted_input_authority_) {
    trusted_input_authority_->Reset();
  }
}

bool PlayreadyLicense::CreateTrustedInputAndTrustedInputAuthority(
    ComPtr<IStream> content_header) {
  SB_DCHECK(content_header);

  ComPtr<IInspectable> inspectable;
  ComPtr<IPersistStream> persist_stream;

  HRESULT hr = ::Windows::Foundation::ActivateInstance(
      ::Microsoft::WRL::Wrappers::HStringReference(
          L"Windows.Media.Protection.PlayReady."
          L"PlayReadyWinRTTrustedInput")
          .Get(),
      &inspectable);
  CheckResult(hr);
  if (FAILED(hr)) {
    return false;
  }

  hr = inspectable.As(&persist_stream);
  CheckResult(hr);
  if (FAILED(hr)) {
    return false;
  }

  hr = persist_stream->Load(content_header.Get());
  CheckResult(hr);
  if (FAILED(hr)) {
    return false;
  }

  hr = inspectable.As(&trusted_input_);
  CheckResult(hr);
  if (FAILED(hr)) {
    return false;
  }

  const int kDefaultStreamId = 0;
  ComPtr<IUnknown> unknown;
  hr = trusted_input_->GetInputTrustAuthority(kDefaultStreamId,
                                              IID_IMFInputTrustAuthority,
                                              unknown.ReleaseAndGetAddressOf());
  CheckResult(hr);
  if (FAILED(hr)) {
    return false;
  }

  hr = unknown.As(&trusted_input_authority_);
  CheckResult(hr);

  hr = trusted_input_authority_->GetDecrypter(IID_IMFTransform, &decryptor_);
  CheckResult(hr);

  return SUCCEEDED(hr);
}

void PlayreadyLicense::GenerateChallenge() {
  SB_DCHECK(trusted_input_);
  SB_DCHECK(trusted_input_authority_);
  SB_DCHECK(!service_request_);
  SB_DCHECK(license_challenge_.empty());

  using Windows::Media::Protection::PlayReady::IPlayReadyServiceRequest;
  using Windows::Media::Protection::PlayReady::PlayReadyContentHeader;
  using Windows::Media::Protection::PlayReady::
      PlayReadyLicenseAcquisitionServiceRequest;
  using Windows::Media::Protection::PlayReady::PlayReadySoapMessage;

  ComPtr<IMFActivate> enabler_activate;

  HRESULT hr = trusted_input_authority_->RequestAccess(
      PEACTION_PLAY, enabler_activate.GetAddressOf());

  if (SUCCEEDED(hr)) {
    // The license is usable but we still want to generate a challenge to
    // ensure that the implementation conforms to the EME spec.
    PlayReadyLicenseAcquisitionServiceRequest ^ fake_service_request =
        ref new PlayReadyLicenseAcquisitionServiceRequest;

    std::vector<uint8_t> copy_of_header(wrm_header_.wrm_header());
    PlayReadyContentHeader ^ content_header =
        ref new PlayReadyContentHeader(ref new Platform::Array<unsigned char>(
            copy_of_header.data(), static_cast<DWORD>(copy_of_header.size())));
    fake_service_request->ContentHeader = content_header;

    PlayReadySoapMessage ^ soap_message =
        fake_service_request->GenerateManualEnablingChallenge();

    if (soap_message != nullptr) {
      Platform::Array<unsigned char> ^ challenge =
          soap_message->GetMessageBody();

      license_challenge_.assign(challenge->begin(), challenge->end());
      SB_DCHECK(!license_challenge_.empty());
      SB_LOG(INFO) << "Playready license is already usable, force challenge "
                   << "generating using license acquisition service request";
    }

    usable_ = true;
    return;
  }

  if (!enabler_activate) {
    SB_NOTREACHED() << "enabler_activate is NULL but the license isn't usable.";
    return;
  }

  hr = enabler_activate->ActivateObject(
      __uuidof(IPlayReadyServiceRequest ^),
      reinterpret_cast<LPVOID*>(&service_request_));
  CheckResult(hr);
  if (FAILED(hr)) {
    return;
  }

  PlayReadySoapMessage ^ soap_message =
      service_request_->GenerateManualEnablingChallenge();

  if (soap_message != nullptr) {
    Platform::Array<unsigned char> ^ challenge = soap_message->GetMessageBody();

    license_challenge_.assign(challenge->begin(), challenge->end());
    SB_DCHECK(!license_challenge_.empty());
  }
}

void PlayreadyLicense::UpdateLicense(const void* license, int license_size) {
  if (!service_request_) {
    SB_DCHECK(usable_);
    return;
  }

  SB_DCHECK(!license_response_);

  // Note that store the response in the object is not an ideal way to exchange
  // data between the function and the MTA thread but it allows us to easily
  // unify the RunFunctionOnMTA() interface.
  unsigned char* license_in_unsigned_char =
      static_cast<unsigned char*>(const_cast<void*>(license));
  license_response_ = ref new Platform::Array<unsigned char>(
      license_in_unsigned_char, license_size);
  RunFunctionOnMTA(&PlayreadyLicense::UpdateLicenseInternal, this);

  if (!usable_) {
    SB_LOG(INFO)
        << "License is not usable, try to re-create the ITA and challenge";
    if (CreateTrustedInputAndTrustedInputAuthority(
            wrm_header_.content_header())) {
      RunFunctionOnMTA(&PlayreadyLicense::GenerateChallenge, this);
      key_id_ = wrm_header_.key_id();
    }
    SB_LOG_IF(INFO, usable_) << "License is usable after retry";
  }
}

void PlayreadyLicense::UpdateLicenseInternal() {
  Windows::Foundation::HResult hresult =
      service_request_->ProcessManualEnablingResponse(license_response_);
  SB_LOG_IF(ERROR, FAILED(hresult.Value))
      << "ProcessManualEnablingResponse() failed with 0x" << std::hex
      << hresult.Value;
  SB_DCHECK(SUCCEEDED(hresult.Value));

  ComPtr<IMFActivate> enabler_activate;
  HRESULT hr = trusted_input_authority_->RequestAccess(
      PEACTION_PLAY, enabler_activate.GetAddressOf());
  SB_LOG_IF(ERROR, FAILED(hr))
      << "RequestAccess() failed after license update with 0x" << std::hex
      << hr;

  // TODO: It is possible that the above license is invalid and the player will
  // retry after error status is propagated.  However, consider to generate a
  // new license message directly using the |enabler_activate| above.
  usable_ = SUCCEEDED(hr);
  license_response_ = nullptr;
  service_request_ = nullptr;
}

bool PlayreadyLicense::IsHDCPRequired() {
  if (hdcp_required_.has_engaged()) {
    return hdcp_required_.value();
  }

  ComPtr<IMFOutputPolicy> output_policy;
  HRESULT hr = trusted_input_authority_->GetPolicy(
      PEACTION_PLAY, output_policy.GetAddressOf());
  CheckResult(hr);

  ComPtr<IMFCollection> required_schemas;
  DWORD attributes = MFOUTPUTATTRIBUTE_DIGITAL | MFOUTPUTATTRIBUTE_VIDEO;
  GUID protection_schema = MFPROTECTION_HDCP;

  hr = output_policy->GenerateRequiredSchemas(attributes, MFCONNECTOR_HDMI,
                                              &protection_schema, 1,
                                              required_schemas.GetAddressOf());
  CheckResult(hr);

  DWORD element_count;
  hr = required_schemas->GetElementCount(&element_count);
  CheckResult(hr);
  hdcp_required_ = false;
  if (element_count != 1) {
    SB_DCHECK(element_count == 0);
    return false;
  }

  ComPtr<IUnknown> unknown;
  ComPtr<IMFOutputSchema> output_schema;
  hr = required_schemas->GetElement(0, unknown.GetAddressOf());
  CheckResult(hr);
  hr = unknown.As(&output_schema);
  CheckResult(hr);
  GUID schema_id;
  hr = output_schema->GetSchemaType(&schema_id);
  CheckResult(hr);
  SB_DCHECK(schema_id == MFPROTECTION_HDCP);
  hdcp_required_ = true;
  return true;
}

}  // namespace
}  // namespace shared
}  // namespace xb1
}  // namespace starboard

namespace starboard {
namespace shared {
namespace win32 {

scoped_refptr<DrmSystemPlayready::License> DrmSystemPlayready::License::Create(
    const void* initialization_data,
    int initialization_data_size) {
  return new ::starboard::xb1::shared::PlayreadyLicense(
      initialization_data, initialization_data_size);
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
