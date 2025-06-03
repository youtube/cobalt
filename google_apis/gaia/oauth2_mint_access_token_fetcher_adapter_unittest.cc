// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_access_token_fetcher_adapter.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Ge;
using testing::IsEmpty;
using testing::Le;
using testing::Matcher;
using testing::Property;

const char kTestClientId[] = "test_client_id";
const char kTestClientSecret[] = "test_client_secret";
const char kTestScope[] = "test_scope";
const char kTestRefreshToken[] = "test_refresh_token";
const char kTestAccessToken[] = "test_access_token";
const char kTestDeviceId[] = "test_device_id";
const char kTestVersion[] = "test_version";
const char kTestChannel[] = "test_channel";

class MockOAuth2AccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockOAuth2AccessTokenConsumer() = default;
  ~MockOAuth2AccessTokenConsumer() override = default;

  MOCK_METHOD(void,
              OnGetTokenSuccess,
              (const OAuth2AccessTokenConsumer::TokenResponse&),
              (override));
  MOCK_METHOD(void,
              OnGetTokenFailure,
              (const GoogleServiceAuthError& error),
              (override));

  std::string GetConsumerName() const override {
    return "oauth2_mint_access_token_fetcher_adapter_unittest";
  }
};

class MockOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  MockOAuth2MintTokenFlow(Delegate* delegate, Parameters params)
      : OAuth2MintTokenFlow(delegate, params.Clone()),
        delegate_(delegate),
        params_(std::move(params)) {}

  MOCK_METHOD(
      void,
      Start,
      (scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       const std::string& access_token),
      (override));

  void SimulateMintTokenSuccess(const std::string& access_token,
                                const std::set<std::string>& granted_scopes,
                                int time_to_live) {
    delegate_->OnMintTokenSuccess(access_token, granted_scopes, time_to_live);
  }
  void SimulateMintTokenFailure(const GoogleServiceAuthError& error) {
    delegate_->OnMintTokenFailure(error);
  }
  void SimulateRemoteConsentSuccess(
      const RemoteConsentResolutionData& resolution_data) {
    delegate_->OnRemoteConsentSuccess(resolution_data);
  }

  const Parameters& params() { return params_; }

  base::WeakPtr<MockOAuth2MintTokenFlow> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const raw_ptr<Delegate> delegate_ = nullptr;
  const Parameters params_;
  base::WeakPtrFactory<MockOAuth2MintTokenFlow> weak_ptr_factory_{this};
};

Matcher<const OAuth2AccessTokenConsumer::TokenResponse&> HasAccessTokenWithTtl(
    const std::string& access_token,
    base::TimeDelta time_to_live) {
  using TokenResponse = OAuth2AccessTokenConsumer::TokenResponse;
  return AllOf(
      Field("access_token", &TokenResponse::access_token, access_token),
      Field("refresh_token", &TokenResponse::refresh_token, IsEmpty()),
      Field("id_token", &TokenResponse::id_token, IsEmpty()),
      Field(
          "expiration_time", &TokenResponse::expiration_time,
          AllOf(Ge(base::Time::Now()), Le(base::Time::Now() + time_to_live))));
}

Matcher<const OAuth2MintTokenFlow::Parameters&> ParamsEq(
    const OAuth2MintTokenFlow::Parameters& expected) {
  using Parameters = OAuth2MintTokenFlow::Parameters;
  return AllOf(
      Field("extension_id", &Parameters::extension_id, expected.extension_id),
      Field("client_id", &Parameters::client_id, expected.client_id),
      Field("scopes", &Parameters::scopes, expected.scopes),
      Field("enable_granular_permissions",
            &Parameters::enable_granular_permissions,
            expected.enable_granular_permissions),
      Field("device_id", &Parameters::device_id, expected.device_id),
      Field("selected_user_id", &Parameters::selected_user_id,
            expected.selected_user_id),
      Field("consent_result", &Parameters::consent_result,
            expected.consent_result),
      Field("version", &Parameters::version, expected.version),
      Field("channel", &Parameters::channel, expected.channel),
      Field("mode", &Parameters::mode, expected.mode));
}

}  // namespace

class OAuth2MintAccessTokenFetcherAdapterTest : public testing::Test {
 public:
  OAuth2MintAccessTokenFetcherAdapterTest() = default;
  ~OAuth2MintAccessTokenFetcherAdapterTest() override = default;

  std::unique_ptr<OAuth2MintAccessTokenFetcherAdapter> CreateFetcher() {
    auto fetcher = std::make_unique<OAuth2MintAccessTokenFetcherAdapter>(
        &mock_consumer_, url_loader_factory_.GetSafeWeakWrapper(),
        kTestRefreshToken, kTestDeviceId, kTestVersion, kTestChannel);
    fetcher->SetOAuth2MintTokenFlowFactoryForTesting(base::BindRepeating(
        &OAuth2MintAccessTokenFetcherAdapterTest::CreateMockFlow,
        base::Unretained(this)));
    return fetcher;
  }

  std::unique_ptr<OAuth2MintTokenFlow> CreateMockFlow(
      OAuth2MintTokenFlow::Delegate* delegate,
      OAuth2MintTokenFlow::Parameters params) {
    CHECK(!mock_flow_);
    auto mock_flow =
        std::make_unique<MockOAuth2MintTokenFlow>(delegate, std::move(params));
    mock_flow_ = mock_flow->GetWeakPtr();
    EXPECT_CALL(*mock_flow_, Start(_, kTestRefreshToken));
    return mock_flow;
  }

  MockOAuth2AccessTokenConsumer* mock_consumer() { return &mock_consumer_; }

  base::WeakPtr<MockOAuth2MintTokenFlow> mock_flow() { return mock_flow_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory url_loader_factory_;
  MockOAuth2AccessTokenConsumer mock_consumer_;
  base::WeakPtr<MockOAuth2MintTokenFlow> mock_flow_ = nullptr;
};

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, Params) {
  auto fetcher = CreateFetcher();
  // Need to start a fetcher to create a mock flow.
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  EXPECT_TRUE(mock_flow());
  OAuth2MintTokenFlow::Parameters expected_params;
  expected_params.client_id = kTestClientId;
  expected_params.version = kTestVersion;
  expected_params.channel = kTestChannel;
  expected_params.device_id = kTestDeviceId;
  expected_params.enable_granular_permissions = false;
  expected_params.mode = OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE;
  expected_params.scopes = {kTestScope};
  EXPECT_THAT(mock_flow()->params(), ParamsEq(expected_params));
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, Success) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  base::TimeDelta kTimeToLive = base::Hours(4);
  EXPECT_CALL(*mock_consumer(), OnGetTokenSuccess(HasAccessTokenWithTtl(
                                    kTestAccessToken, kTimeToLive)));
  mock_flow()->SimulateMintTokenSuccess(kTestAccessToken, {kTestScope},
                                        kTimeToLive.InSeconds());
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, Failure) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  EXPECT_CALL(*mock_consumer(), OnGetTokenFailure(error));
  mock_flow()->SimulateMintTokenFailure(error);
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, UnexpectedConsentResult) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  EXPECT_CALL(*mock_consumer(),
              OnGetTokenFailure(Property(
                  "state", &GoogleServiceAuthError::state,
                  GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE)));
  mock_flow()->SimulateRemoteConsentSuccess(RemoteConsentResolutionData());
}

TEST_F(OAuth2MintAccessTokenFetcherAdapterTest, CancelRequest) {
  auto fetcher = CreateFetcher();
  fetcher->Start(kTestClientId, kTestClientSecret, {kTestScope});
  fetcher->CancelRequest();
  EXPECT_FALSE(mock_flow());
}
