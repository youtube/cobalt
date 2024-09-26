// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/endpoint_fetcher/endpoint_fetcher.h"

#include <string>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using MockEndpointFetcherCallback = base::MockCallback<EndpointFetcherCallback>;

namespace {
const char kContentType[] = "mock_content_type";
const char kEmail[] = "mock_email@gmail.com";
const char kEndpoint[] = "https://my-endpoint.com";
const char kEmptyResponse[] = "";
const char kExpectedResponse[] = "{}";
const char kExpectedAuthError[] = "There was an authentication error";
const char kExpectedResponseError[] = "There was a response error";
const char kExpectedPrimaryAccountError[] = "No primary accounts found";
const char kExpectedSanitizationError[] = "There was a sanitization error";
const char kHttpPostMethod[] = "POST";
const char kMalformedResponse[] = "asdf";
const char kMockPostData[] = "mock_post_data";
int64_t kMockTimeoutMs = 1000000;
const char kOAuthConsumerName[] = "mock_oauth_consumer_name";
const char kScope[] = "mock_scope";
const char kApiKey[] = "api_key";
}  // namespace

using ::testing::Field;
using ::testing::Pointee;

class EndpointFetcherTest : public testing::Test {
 protected:
  EndpointFetcherTest() = default;

  EndpointFetcherTest(const EndpointFetcherTest& endpoint_fetcher_test) =
      delete;
  EndpointFetcherTest& operator=(
      const EndpointFetcherTest& endpoint_fetcher_test) = delete;

  ~EndpointFetcherTest() override = default;

  void SetUp() override {
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    endpoint_fetcher_ = std::make_unique<EndpointFetcher>(
        kOAuthConsumerName, GURL(kEndpoint), kHttpPostMethod, kContentType,
        std::vector<std::string>{kScope}, kMockTimeoutMs, kMockPostData,
        TRAFFIC_ANNOTATION_FOR_TESTS, test_url_loader_factory,
        identity_test_env_.identity_manager());
    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();
  }

  void SignIn() {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail,
                                                   signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  MockEndpointFetcherCallback& endpoint_fetcher_callback() {
    return mock_callback_;
  }

  EndpointFetcher* endpoint_fetcher() { return endpoint_fetcher_.get(); }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  void SetMockResponse(const GURL& request_url,
                       const std::string& response_data,
                       net::HttpStatusCode response_code,
                       net::Error error) {
    auto head = network::mojom::URLResponseHead::New();
    std::string headers(base::StringPrintf(
        "HTTP/1.1 %d %s\nContent-type: application/json\n\n",
        static_cast<int>(response_code), GetHttpReasonPhrase(response_code)));
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    head->mime_type = "application/json";
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = response_data.size();
    test_url_loader_factory_.AddResponse(request_url, std::move(head),
                                         response_data, status);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  MockEndpointFetcherCallback mock_callback_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<EndpointFetcher> endpoint_fetcher_;
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
};

TEST_F(EndpointFetcherTest, FetchResponse) {
  SignIn();
  SetMockResponse(GURL(kEndpoint), kExpectedResponse, net::HTTP_OK, net::OK);
  EXPECT_CALL(endpoint_fetcher_callback(),
              Run(Pointee(AllOf(
                  Field(&EndpointResponse::response, kExpectedResponse),
                  Field(&EndpointResponse::http_status_code, net::HTTP_OK),
                  Field(&EndpointResponse::error_type, absl::nullopt)))));
  endpoint_fetcher()->Fetch(endpoint_fetcher_callback().Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(EndpointFetcherTest, FetchMalformedResponse) {
  SignIn();
  SetMockResponse(GURL(kEndpoint), kMalformedResponse, net::HTTP_OK, net::OK);
  EXPECT_CALL(endpoint_fetcher_callback(),
              Run(Pointee(AllOf(
                  Field(&EndpointResponse::response,
                        testing::StartsWith(kExpectedSanitizationError)),
                  Field(&EndpointResponse::http_status_code, net::HTTP_OK),
                  Field(&EndpointResponse::error_type,
                        FetchErrorType::kResultParseError)))));
  endpoint_fetcher()->Fetch(endpoint_fetcher_callback().Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(EndpointFetcherTest, FetchEndpointResponseError) {
  SignIn();
  SetMockResponse(GURL(kEndpoint), kExpectedResponse, net::HTTP_BAD_REQUEST,
                  net::ERR_FAILED);
  EXPECT_CALL(
      endpoint_fetcher_callback(),
      Run(Pointee(AllOf(
          Field(&EndpointResponse::response, kExpectedResponseError),
          Field(&EndpointResponse::http_status_code, -1),
          Field(&EndpointResponse::error_type, FetchErrorType::kNetError)))));
  endpoint_fetcher()->Fetch(endpoint_fetcher_callback().Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(EndpointFetcherTest, FetchRedirectionResponse) {
  SignIn();
  SetMockResponse(GURL(kEndpoint), kExpectedResponse, net::HTTP_FOUND, net::OK);
  EXPECT_CALL(endpoint_fetcher_callback(),
              Run(Pointee(AllOf(
                  Field(&EndpointResponse::response, kExpectedResponse),
                  Field(&EndpointResponse::http_status_code, net::HTTP_FOUND),
                  Field(&EndpointResponse::error_type, absl::nullopt)))));
  endpoint_fetcher()->Fetch(endpoint_fetcher_callback().Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(EndpointFetcherTest, FetchOAuthError) {
  SignIn();
  identity_test_env().SetAutomaticIssueOfAccessTokens(false);
  EXPECT_CALL(
      endpoint_fetcher_callback(),
      Run(Pointee(AllOf(
          Field(&EndpointResponse::response, kExpectedAuthError),
          Field(&EndpointResponse::http_status_code, -1),
          Field(&EndpointResponse::error_type, FetchErrorType::kAuthError)))));
  endpoint_fetcher()->Fetch(endpoint_fetcher_callback().Get());
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  base::RunLoop().RunUntilIdle();
}

TEST_F(EndpointFetcherTest, FetchOAuthNoPrimaryAccount) {
  EXPECT_CALL(
      endpoint_fetcher_callback(),
      Run(Pointee(AllOf(
          Field(&EndpointResponse::response, kExpectedPrimaryAccountError),
          Field(&EndpointResponse::http_status_code, -1),
          Field(&EndpointResponse::error_type, FetchErrorType::kAuthError)))));
  endpoint_fetcher()->Fetch(endpoint_fetcher_callback().Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(EndpointFetcherTest, PerformRequestAuthError) {
  SetMockResponse(GURL(kEndpoint), kEmptyResponse, net::HTTP_UNAUTHORIZED,
                  net::OK);
  EXPECT_CALL(
      endpoint_fetcher_callback(),
      Run(Pointee(AllOf(
          Field(&EndpointResponse::response, kEmptyResponse),
          Field(&EndpointResponse::http_status_code, net::HTTP_UNAUTHORIZED),
          Field(&EndpointResponse::error_type, FetchErrorType::kAuthError)))));

  endpoint_fetcher()->PerformRequest(endpoint_fetcher_callback().Get(),
                                     kApiKey);
  base::RunLoop().RunUntilIdle();
}
