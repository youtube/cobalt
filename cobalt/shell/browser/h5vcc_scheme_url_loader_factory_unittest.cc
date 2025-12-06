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
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

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
    factory_.SetResourceMapForTesting(&test_resource_map_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  H5vccSchemeURLLoaderFactory factory_;
  GeneratedResourceMap test_resource_map_;
};

TEST_F(H5vccSchemeURLLoaderFactoryTest, LoadExistingResource) {
  network::ResourceRequest request;
  request.url = GURL("h5vcc-embedded://test.html");

  mojo::PendingRemote<network::mojom::URLLoader> loader;
  auto client = std::make_unique<TestURLLoaderClient>();

  factory_.CreateLoaderAndStart(
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

  factory_.CreateLoaderAndStart(
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

}  // namespace content
