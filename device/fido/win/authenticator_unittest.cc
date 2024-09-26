// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using MakeCredentialCallbackReceiver = test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    absl::optional<AuthenticatorMakeCredentialResponse>>;

using GetAssertionCallbackReceiver = test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    std::vector<AuthenticatorGetAssertionResponse>>;

using GetCredentialCallbackReceiver =
    test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>,
                               FidoRequestHandlerBase::RecognizedCredential>;

using EnumeratePlatformCredentialsCallbackReceiver =
    test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>>;

const std::vector<uint8_t> kCredentialId = {1, 2, 3, 4};
const std::vector<uint8_t> kCredentialId2 = {9, 0, 1, 2};
constexpr char kRpId[] = "project-altdeus.example.com";
const std::vector<uint8_t> kUserId = {5, 6, 7, 8};
constexpr char kUserName[] = "unit-aarc-noa";
constexpr char kUserDisplayName[] = "Noa";
const std::vector<uint8_t> kLargeBlob = {'b', 'l', 'o', 'b'};

class WinAuthenticatorTest : public testing::Test {
 public:
  void SetUp() override {
    fake_webauthn_api_ = std::make_unique<FakeWinWebAuthnApi>();
    fake_webauthn_api_->set_supports_silent_discovery(true);
    authenticator_ = std::make_unique<WinWebAuthnApiAuthenticator>(
        /*current_window=*/nullptr, fake_webauthn_api_.get());
  }

  void SetVersion(int version) {
    fake_webauthn_api_->set_version(version);
    // `WinWebAuthnApiAuthenticator` does not expect the webauthn.dll version to
    // change during its lifetime, thus needs to be recreated for each version
    // change.
    authenticator_ = std::make_unique<WinWebAuthnApiAuthenticator>(
        /*current_window=*/nullptr, fake_webauthn_api_.get());
  }

 protected:
  std::unique_ptr<FidoAuthenticator> authenticator_;
  std::unique_ptr<FakeWinWebAuthnApi> fake_webauthn_api_;
  base::test::TaskEnvironment task_environment;
};

// Tests getting credential information for an empty allow-list request that has
// valid credentials on a Windows version that supports silent discovery.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_HasCredentials) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), callback.callback());
  callback.WaitForCallback();

  DiscoverableCredentialMetadata expected = DiscoverableCredentialMetadata(
      AuthenticatorType::kWinNative, kRpId, kCredentialId, user);
  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{expected});
  EXPECT_EQ(
      std::get<1>(*callback.result()),
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
  EXPECT_FALSE(fake_webauthn_api_->last_get_credentials_options()
                   ->bBrowserInPrivateMode);
}

// Tests a request with the off the record flag on passes the
// bBrowserInPrivateMode option to the Windows API.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_Incognito) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  CtapGetAssertionOptions options;
  options.is_off_the_record_context = true;
  GetCredentialCallbackReceiver callback;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), std::move(options), callback.callback());
  callback.WaitForCallback();
  EXPECT_TRUE(fake_webauthn_api_->last_get_credentials_options()
                  ->bBrowserInPrivateMode);
}

// Tests getting credential information for an empty allow-list request that
// does not have valid credentials on a Windows version that supports silent
// discovery.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_NoCredentials) {
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(
      std::get<1>(*callback.result()),
      FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
}

// Tests the authenticator handling of an unexpected error from the Windows API.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_UnknownError) {
  fake_webauthn_api_->set_hresult(ERROR_NOT_SUPPORTED);
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(std::get<1>(*callback.result()),
            FidoRequestHandlerBase::RecognizedCredential::kUnknown);
}

// Tests the authenticator handling of attempting to get credential information
// for a version of the Windows API that does not support silent discovery.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_Unsupported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(false);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), callback.callback());
  callback.WaitForCallback();

  DiscoverableCredentialMetadata expected = DiscoverableCredentialMetadata(
      AuthenticatorType::kWinNative, kRpId, kCredentialId, user);
  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(std::get<1>(*callback.result()),
            FidoRequestHandlerBase::RecognizedCredential::kUnknown);
}

// Tests that for non empty allow-list requests with a matching discoverable
// credential, the authenticator returns an empty credential list but reports
// the credential availability.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList_Found) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  request.allow_list.emplace_back(CredentialType::kPublicKey, kCredentialId);
  GetCredentialCallbackReceiver callback;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(
      std::get<1>(*callback.result()),
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
}

// Tests that for non empty allow-list requests without a matching discoverable
// credential, the authenticator returns an empty credential list and reports no
// credential availability.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList_NotMatching) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  request.allow_list.emplace_back(CredentialType::kPublicKey, kCredentialId2);
  GetCredentialCallbackReceiver callback;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(
      std::get<1>(*callback.result()),
      FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
}

// Tests that for non empty allow-list requests without an only internal
// transport credential, the authenticator returns an empty credential list and
// reports no credential availability, even if silent discovery is not
// supported.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList_NoInternal) {
  fake_webauthn_api_->set_supports_silent_discovery(false);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");

  PublicKeyCredentialDescriptor credential(CredentialType::kPublicKey,
                                           kCredentialId2);
  credential.transports = {FidoTransportProtocol::kInternal,
                           FidoTransportProtocol::kHybrid};
  request.allow_list.emplace_back(std::move(credential));

  GetCredentialCallbackReceiver callback;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(
      std::get<1>(*callback.result()),
      FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
}

TEST_F(WinAuthenticatorTest, EnumeratePlatformCredentials_NotSupported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(false);

  test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>>
      callback;
  WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      fake_webauthn_api_.get(), callback.callback());

  while (!callback.was_called()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(std::get<0>(*callback.result()).empty());
}

TEST_F(WinAuthenticatorTest, EnumeratePlatformCredentials_Supported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(true);

  test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>>
      callback;
  WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      fake_webauthn_api_.get(), callback.callback());

  while (!callback.was_called()) {
    base::RunLoop().RunUntilIdle();
  }

  std::vector<DiscoverableCredentialMetadata> creds =
      std::move(std::get<0>(callback.TakeResult()));
  ASSERT_EQ(creds.size(), 1u);
  const DiscoverableCredentialMetadata& cred = creds[0];
  EXPECT_EQ(cred.source, AuthenticatorType::kWinNative);
  EXPECT_EQ(cred.rp_id, kRpId);
  EXPECT_EQ(cred.cred_id, kCredentialId);
  EXPECT_EQ(cred.user.name, kUserName);
  EXPECT_EQ(cred.user.display_name, kUserDisplayName);
}

TEST_F(WinAuthenticatorTest, IsConditionalMediationAvailable) {
  for (bool silent_discovery : {false, true}) {
    SCOPED_TRACE(silent_discovery);
    fake_webauthn_api_->set_supports_silent_discovery(silent_discovery);
    test::TestCallbackReceiver<bool> callback;
    base::RunLoop run_loop;
    WinWebAuthnApiAuthenticator::IsConditionalMediationAvailable(
        fake_webauthn_api_.get(),
        base::BindLambdaForTesting([&](bool is_available) {
          EXPECT_EQ(is_available, silent_discovery);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(WinAuthenticatorTest, MakeCredentialLargeBlob) {
  enum Availability : bool {
    kNotAvailable = false,
    kAvailable = true,
  };

  enum Result : bool {
    kDoesNotHaveLargeBlob = false,
    kHasLargeBlob = true,
  };

  struct LargeBlobTestCase {
    LargeBlobSupport requirement;
    Availability availability;
    Result result;
  };

  std::array<LargeBlobTestCase, 5> test_cases = {{
      {LargeBlobSupport::kNotRequested, kAvailable, kDoesNotHaveLargeBlob},
      {LargeBlobSupport::kNotRequested, kNotAvailable, kDoesNotHaveLargeBlob},
      {LargeBlobSupport::kPreferred, kAvailable, kHasLargeBlob},
      {LargeBlobSupport::kPreferred, kNotAvailable, kDoesNotHaveLargeBlob},
      {LargeBlobSupport::kRequired, kAvailable, kHasLargeBlob},
      // Calling the Windows API with large blob = required is not allowed if
      // it's not supported by the API version.
  }};

  for (const LargeBlobTestCase& test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "requirement=" << static_cast<bool>(test_case.requirement)
                 << ", availability="
                 << static_cast<bool>(test_case.availability));

    SetVersion(test_case.availability ? WEBAUTHN_API_VERSION_3
                                      : WEBAUTHN_API_VERSION_2);
    EXPECT_EQ(authenticator_->Options().large_blob_type.has_value(),
              test_case.availability);
    PublicKeyCredentialRpEntity rp("adrestian-empire.com");
    PublicKeyCredentialUserEntity user(std::vector<uint8_t>{1, 2, 3, 4},
                                       "el@adrestian-empire.com", "Edelgard");
    CtapMakeCredentialRequest request(
        test_data::kClientDataJson, rp, user,
        PublicKeyCredentialParams({{CredentialType::kPublicKey, -257}}));
    MakeCredentialOptions options;
    options.large_blob_support = test_case.requirement;
    MakeCredentialCallbackReceiver callback;
    authenticator_->MakeCredential(std::move(request), options,
                                   callback.callback());
    callback.WaitForCallback();
    ASSERT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_EQ(callback.value()->large_blob_type.has_value(), test_case.result);
  }
}

// Tests that making a credential with attachment=undefined forces the
// attachment to cross-platform if large blob is required.
// This is because largeBlob=required is ignored by the Windows platform
// authenticator at the time of writing (Feb 2023).
TEST_F(WinAuthenticatorTest, MakeCredentialLargeBlobAttachmentUndefined) {
  SetVersion(WEBAUTHN_API_VERSION_3);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  CtapMakeCredentialRequest request(
      test_data::kClientDataJson, rp, user,
      PublicKeyCredentialParams({{CredentialType::kPublicKey, -257}}));
  request.authenticator_attachment = AuthenticatorAttachment::kAny;
  fake_webauthn_api_->set_preferred_attachment(
      WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM);
  MakeCredentialOptions options;
  options.large_blob_support = LargeBlobSupport::kRequired;
  MakeCredentialCallbackReceiver callback;
  authenticator_->MakeCredential(std::move(request), options,
                                 callback.callback());
  callback.WaitForCallback();
  ASSERT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
  EXPECT_TRUE(callback.value()->large_blob_type.has_value());
  EXPECT_NE(*callback.value()->transport_used,
            FidoTransportProtocol::kInternal);
}

TEST_F(WinAuthenticatorTest, GetAssertionLargeBlobNotSupported) {
  SetVersion(WEBAUTHN_API_VERSION_2);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, std::move(rp),
                                                   std::move(user));
  {
    // Read large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    GetAssertionCallbackReceiver callback;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_FALSE(callback.value().at(0).large_blob.has_value());
  }
  {
    // Write large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_write = kLargeBlob;
    GetAssertionCallbackReceiver callback;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_FALSE(callback.value().at(0).large_blob_written);
  }
}

TEST_F(WinAuthenticatorTest, GetAssertionLargeBlobError) {
  SetVersion(WEBAUTHN_API_VERSION_3);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, std::move(rp),
                                                   std::move(user));
  fake_webauthn_api_->set_large_blob_result(
      WEBAUTHN_CRED_LARGE_BLOB_STATUS_NOT_SUPPORTED);
  {
    // Read large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    GetAssertionCallbackReceiver callback;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_FALSE(callback.value().at(0).large_blob.has_value());
  }
  {
    // Write large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_write = kLargeBlob;
    GetAssertionCallbackReceiver callback;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_FALSE(callback.value().at(0).large_blob_written);
  }
}

TEST_F(WinAuthenticatorTest, GetAssertionLargeBlobSuccess) {
  SetVersion(WEBAUTHN_API_VERSION_3);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, std::move(rp),
                                                   std::move(user));
  {
    // Read large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    GetAssertionCallbackReceiver callback;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_FALSE(callback.value().at(0).large_blob.has_value());
    EXPECT_FALSE(callback.value().at(0).large_blob_written);
  }
  {
    // Write large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_write = kLargeBlob;
    GetAssertionCallbackReceiver callback;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_FALSE(callback.value().at(0).large_blob.has_value());
    EXPECT_TRUE(callback.value().at(0).large_blob_written);
  }
  {
    // Read the large blob that was just written.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    GetAssertionCallbackReceiver callback;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_TRUE(callback.value().at(0).large_blob.has_value());
    EXPECT_EQ(*callback.value().at(0).large_blob, kLargeBlob);
    EXPECT_FALSE(callback.value().at(0).large_blob_written);
  }
}

}  // namespace
}  // namespace device
