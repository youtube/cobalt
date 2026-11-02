// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cobalt/browser/features.h"
#include "cobalt/shell/embedded_resources/embedded_resources.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::HasSubstr;
using ::testing::Not;

class TestURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  TestURLLoaderClient() = default;
  ~TestURLLoaderClient() override = default;

  mojo::PendingRemote<network::mojom::URLLoaderClient> CreateRemote() {
    mojo::PendingRemote<network::mojom::URLLoaderClient> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    response_head_ = std::move(head);
    body_ = std::move(body);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    completion_status_ = status;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void WaitForResponse() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void WaitForCompletion() {
    if (completion_status_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  std::string GetResponseBody() {
    std::string response;
    if (!body_.is_valid()) {
      return "";
    }
    EXPECT_TRUE(mojo::BlockingCopyToString(std::move(body_), &response));
    return response;
  }

  const network::mojom::URLResponseHeadPtr& response_head() const {
    return response_head_;
  }
  const std::optional<network::URLLoaderCompletionStatus>& completion_status()
      const {
    return completion_status_;
  }

 private:
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
  network::mojom::URLResponseHeadPtr response_head_;
  mojo::ScopedDataPipeConsumerHandle body_;
  std::optional<network::URLLoaderCompletionStatus> completion_status_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

const char kTestHtmlContent[] = "<html><body><h1>Test</h1></body></html>";
// Stand-ins for the splash screen's page, image and WebAssembly policy.
const char kTestSplashHtmlContent[] = "<html><body>Splash</body></html>";
const char kTestSplashPngContent[] = "png";
const char kTestSplashPolicyContent[] = "wasm";

constexpr char kOutcomeHistogram[] = "Cobalt.SplashScreen.WasmPolicy.Outcome";
constexpr char kDecisionTimeHistogram[] =
    "Cobalt.SplashScreen.WasmPolicy.DecisionTime";
constexpr char kScriptSrc[] = "script-src 'self' 'unsafe-inline'; ";
constexpr char kScriptSrcWithWasm[] =
    "script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval'; ";

FileContents ContentsOf(const char* content, size_t size_with_nul) {
  return {reinterpret_cast<const unsigned char*>(content),
          static_cast<int>(size_with_nul - 1)};
}

}  // namespace

class H5vccSchemeURLLoaderFactoryTest : public testing::Test {
 public:
  H5vccSchemeURLLoaderFactoryTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    testing::Test::SetUp();
    mojo::core::Init();

    FileContents test_content = {
        reinterpret_cast<const unsigned char*>(kTestHtmlContent),
        sizeof(kTestHtmlContent) - 1};
    test_resource_map_["test.html"] = test_content;
    test_resource_map_["splash.html"] =
        ContentsOf(kTestSplashHtmlContent, sizeof(kTestSplashHtmlContent));
    test_resource_map_["splash.png"] =
        ContentsOf(kTestSplashPngContent, sizeof(kTestSplashPngContent));
    test_resource_map_["splash_policy.wasm"] =
        ContentsOf(kTestSplashPolicyContent, sizeof(kTestSplashPolicyContent));
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(&test_resource_map_);
    factory_ = std::make_unique<H5vccSchemeURLLoaderFactory>(nullptr);
  }

  void TearDown() override {
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(nullptr);
    testing::Test::TearDown();
  }

  struct Response {
    int status = 0;
    std::string mime_type;
    std::string content_security_policy;
  };

  // Loads `url`, and returns its response.
  Response Load(const std::string& url) {
    SCOPED_TRACE(url);
    network::ResourceRequest request;
    request.url = GURL(url);
    mojo::PendingRemote<network::mojom::URLLoader> loader;
    TestURLLoaderClient client;
    factory_->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
        client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    client.WaitForResponse();
    if (!client.response_head() || !client.response_head()->headers) {
      ADD_FAILURE() << "No response";
      return {};
    }
    const net::HttpResponseHeaders& headers = *client.response_head()->headers;
    return {
        headers.response_code(), client.response_head()->mime_type,
        headers.GetNormalizedHeader("Content-Security-Policy").value_or("")};
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<H5vccSchemeURLLoaderFactory> factory_;
  GeneratedResourceMap test_resource_map_;
};

TEST_F(H5vccSchemeURLLoaderFactoryTest, LoadExistingResource) {
  network::ResourceRequest request;
  request.url = GURL("h5vcc-embedded://test.html");

  mojo::PendingRemote<network::mojom::URLLoader> loader;
  auto client = std::make_unique<TestURLLoaderClient>();

  factory_->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client->WaitForResponse();

  ASSERT_TRUE(client->response_head());
  EXPECT_EQ(net::HTTP_OK, client->response_head()->headers->response_code());
  EXPECT_EQ("text/html", client->response_head()->mime_type);

  std::optional<std::string> csp =
      client->response_head()->headers->GetNormalizedHeader(
          "Content-Security-Policy");
  EXPECT_TRUE(csp.has_value());
  EXPECT_FALSE(csp.value().empty());

  std::optional<std::string> accept_ranges =
      client->response_head()->headers->GetNormalizedHeader("Accept-Ranges");
  EXPECT_TRUE(accept_ranges.has_value());
  EXPECT_EQ("none", accept_ranges.value());

  client->WaitForCompletion();
  ASSERT_TRUE(client->completion_status());
  EXPECT_EQ(net::OK, client->completion_status()->error_code);

  std::string response_body = client->GetResponseBody();
  EXPECT_EQ(kTestHtmlContent, response_body);
}

TEST_F(H5vccSchemeURLLoaderFactoryTest, LoadNonExistingResource) {
  network::ResourceRequest request;
  request.url = GURL("h5vcc-embedded://nonexistent.resource");

  mojo::PendingRemote<network::mojom::URLLoader> loader;
  auto client = std::make_unique<TestURLLoaderClient>();

  factory_->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client->WaitForResponse();

  ASSERT_TRUE(client->response_head());
  EXPECT_EQ(net::HTTP_NOT_FOUND,
            client->response_head()->headers->response_code());
  EXPECT_EQ("text/plain", client->response_head()->mime_type);

  client->WaitForCompletion();
  ASSERT_TRUE(client->completion_status());
  EXPECT_EQ(net::OK, client->completion_status()->error_code);
  EXPECT_EQ(client->GetResponseBody(), "Resource not found");
}

// No page can compile WebAssembly without SplashWasmPolicy.
TEST_F(H5vccSchemeURLLoaderFactoryTest, ContentSecurityPolicyForbidsWasm) {
  for (const char* url : {"h5vcc-embedded://splash.html?force_image=true",
                          "h5vcc-embedded://test.html"}) {
    SCOPED_TRACE(url);
    const Response response = Load(url);

    EXPECT_EQ(net::HTTP_OK, response.status);
    EXPECT_THAT(response.content_security_policy, HasSubstr(kScriptSrc));
    EXPECT_THAT(response.content_security_policy, Not(HasSubstr("wasm")));
  }
}

// With it, splash.html can, for its WebAssembly policy, and no other page.
TEST_F(H5vccSchemeURLLoaderFactoryTest,
       ContentSecurityPolicyAllowsWasmForSplashHtmlWithSplashWasmPolicy) {
  base::test::ScopedFeatureList feature_list(
      cobalt::features::kSplashWasmPolicy);

  const Response splash_html =
      Load("h5vcc-embedded://splash.html?force_image=true&wasm_policy=true");
  const Response test_html = Load("h5vcc-embedded://test.html");

  EXPECT_EQ(net::HTTP_OK, splash_html.status);
  EXPECT_THAT(splash_html.content_security_policy,
              HasSubstr(kScriptSrcWithWasm));
  EXPECT_EQ(net::HTTP_OK, test_html.status);
  EXPECT_THAT(test_html.content_security_policy, HasSubstr(kScriptSrc));
  EXPECT_THAT(test_html.content_security_policy, Not(HasSubstr("wasm")));
}

TEST_F(H5vccSchemeURLLoaderFactoryTest, ServesWasmAsApplicationWasm) {
  const Response response = Load("h5vcc-embedded://splash_policy.wasm");

  EXPECT_EQ(net::HTTP_OK, response.status);
  EXPECT_EQ("application/wasm", response.mime_type);
}

TEST_F(H5vccSchemeURLLoaderFactoryTest,
       NoSplashAssetListWithoutSplashWasmPolicy) {
  EXPECT_EQ(net::HTTP_NOT_FOUND,
            Load("h5vcc-embedded://splash_assets.json?cache=default").status);
}

// The assets can't be listed without a BrowserContext, as there's no cache.
TEST_F(H5vccSchemeURLLoaderFactoryTest,
       SplashAssetListIsUnavailableWithoutBrowserContext) {
  base::test::ScopedFeatureList feature_list(
      cobalt::features::kSplashWasmPolicy);

  EXPECT_EQ(net::HTTP_SERVICE_UNAVAILABLE,
            Load("h5vcc-embedded://splash_assets.json?cache=default").status);
}

// splash.html reports its policy's outcome, and how long it waited for the
// policy's choice, in its request for the asset that it shows.
TEST_F(H5vccSchemeURLLoaderFactoryTest, RecordsSplashWasmPolicyMetrics) {
  base::test::ScopedFeatureList feature_list(
      cobalt::features::kSplashWasmPolicy);
  base::HistogramTester histogram_tester;

  EXPECT_EQ(net::HTTP_OK, Load("h5vcc-embedded://"
                               "splash.png?source=builtin&policy=builtin&"
                               "policy_ms=12")
                              .status);

  histogram_tester.ExpectUniqueSample(
      kOutcomeHistogram, SplashWasmPolicyOutcome::kChoseBuiltIn, 1);
  histogram_tester.ExpectUniqueTimeSample(kDecisionTimeHistogram,
                                          base::Milliseconds(12), 1);
}

TEST_F(H5vccSchemeURLLoaderFactoryTest,
       RecordsNoSplashWasmPolicyMetricsWithoutSplashWasmPolicy) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(net::HTTP_OK, Load("h5vcc-embedded://"
                               "splash.png?source=builtin&policy=builtin&"
                               "policy_ms=12")
                              .status);

  histogram_tester.ExpectTotalCount(kOutcomeHistogram, 0);
  histogram_tester.ExpectTotalCount(kDecisionTimeHistogram, 0);
}

TEST_F(H5vccSchemeURLLoaderFactoryTest,
       RecordsNoSplashWasmPolicyMetricsForUnknownOutcome) {
  base::test::ScopedFeatureList feature_list(
      cobalt::features::kSplashWasmPolicy);
  base::HistogramTester histogram_tester;

  EXPECT_EQ(
      net::HTTP_OK,
      Load("h5vcc-embedded://splash.png?policy=chosen&policy_ms=12").status);

  histogram_tester.ExpectTotalCount(kOutcomeHistogram, 0);
  histogram_tester.ExpectTotalCount(kDecisionTimeHistogram, 0);
}

TEST_F(H5vccSchemeURLLoaderFactoryTest, RecordsNoInvalidDecisionTime) {
  base::test::ScopedFeatureList feature_list(
      cobalt::features::kSplashWasmPolicy);
  base::HistogramTester histogram_tester;

  for (const char* policy_ms : {"-1", "abc"}) {
    EXPECT_EQ(net::HTTP_OK,
              Load(base::StrCat({"h5vcc-embedded://splash.png?source=builtin&"
                                 "policy=builtin&policy_ms=",
                                 policy_ms}))
                  .status);
  }

  histogram_tester.ExpectUniqueSample(
      kOutcomeHistogram, SplashWasmPolicyOutcome::kChoseBuiltIn, 2);
  histogram_tester.ExpectTotalCount(kDecisionTimeHistogram, 0);
}

// The page reports no decision time if the policy made no choice.
TEST_F(H5vccSchemeURLLoaderFactoryTest,
       RecordsNoDecisionTimeWithoutPolicyChoice) {
  base::test::ScopedFeatureList feature_list(
      cobalt::features::kSplashWasmPolicy);
  base::HistogramTester histogram_tester;

  for (const char* outcome : {"failed", "list_unavailable"}) {
    EXPECT_EQ(net::HTTP_OK,
              Load(base::StrCat({"h5vcc-embedded://splash.png?cache=default&"
                                 "fallback=splash.png&policy=",
                                 outcome, "&policy_ms=12"}))
                  .status);
  }

  EXPECT_THAT(histogram_tester.GetAllSamples(kOutcomeHistogram),
              base::BucketsAre(
                  base::Bucket(SplashWasmPolicyOutcome::kListUnavailable, 1),
                  base::Bucket(SplashWasmPolicyOutcome::kFailed, 1)));
  histogram_tester.ExpectTotalCount(kDecisionTimeHistogram, 0);
}

TEST_F(H5vccSchemeURLLoaderFactoryTest, GetsBuiltInResourceSize) {
  EXPECT_EQ(sizeof(kTestHtmlContent) - 1,
            H5vccSchemeURLLoaderFactory::GetBuiltInResourceSize("test.html"));
  EXPECT_EQ(0u,
            H5vccSchemeURLLoaderFactory::GetBuiltInResourceSize("unknown.png"));
  // From the test map, which has no video.
  EXPECT_EQ(0u,
            H5vccSchemeURLLoaderFactory::GetBuiltInResourceSize("splash.webm"));

  // From the embedded resources, without the test map.
  H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(nullptr);
  EXPECT_GT(H5vccSchemeURLLoaderFactory::GetBuiltInResourceSize("splash.webm"),
            0u);
  EXPECT_EQ(0u,
            H5vccSchemeURLLoaderFactory::GetBuiltInResourceSize("unknown.png"));
}

}  // namespace content
