// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_sender.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/resource_request_client.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "url/gurl.h"

namespace blink {

namespace {

static constexpr char kTestPageUrl[] = "http://www.google.com/";

constexpr size_t kDataPipeCapacity = 4096;

std::string ReadOneChunk(mojo::ScopedDataPipeConsumerHandle* handle) {
  char buffer[kDataPipeCapacity];
  uint32_t read_bytes = kDataPipeCapacity;
  MojoResult result =
      (*handle)->ReadData(buffer, &read_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    return "";
  }
  return std::string(buffer, read_bytes);
}

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks TicksFromMicroseconds(int64_t micros) {
  return base::TimeTicks() + base::Microseconds(micros);
}

}  // namespace

// A mock ResourceRequestClient to receive messages from the
// ResourceRequestSender.
class MockRequestClient : public ResourceRequestClient {
 public:
  MockRequestClient() = default;

  // ResourceRequestClient overrides:
  void OnUploadProgress(uint64_t position, uint64_t size) override {}
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head,
                          std::vector<std::string>* removed_headers) override {
    last_load_timing_ = head->load_timing;
    return true;
  }
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head,
                          base::TimeTicks response_arrival) override {
    last_load_timing_ = head->load_timing;
    received_response_ = true;
  }
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    if (body) {
      data_ += ReadOneChunk(&body);
    }
  }
  void OnTransferSizeUpdated(int transfer_size_diff) override {}
  void OnReceivedCachedMetadata(mojo_base::BigBuffer data) override {}
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override {
    completion_status_ = status;
    complete_ = true;
  }

  std::string data() { return data_; }
  bool received_response() { return received_response_; }
  bool complete() { return complete_; }
  net::LoadTimingInfo last_load_timing() { return last_load_timing_; }
  network::URLLoaderCompletionStatus completion_status() {
    return completion_status_;
  }

 private:
  // Data received. If downloading to file, remains empty.
  std::string data_;

  bool received_response_ = false;
  bool complete_ = false;
  net::LoadTimingInfo last_load_timing_;
  network::URLLoaderCompletionStatus completion_status_;
};  // namespace blink

// Sets up the message sender override for the unit test.
class ResourceRequestSenderTest : public testing::Test,
                                  public network::mojom::URLLoaderFactory {
 public:
  explicit ResourceRequestSenderTest()
      : resource_request_sender_(new ResourceRequestSender()) {}

  ~ResourceRequestSenderTest() override {
    resource_request_sender_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& annotation) override {
    loader_and_clients_.emplace_back(std::move(receiver), std::move(client));
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED();
  }

  std::unique_ptr<network::ResourceRequest> CreateResourceRequest() {
    std::unique_ptr<network::ResourceRequest> request(
        new network::ResourceRequest());

    request->method = "GET";
    request->url = GURL(kTestPageUrl);
    request->site_for_cookies =
        net::SiteForCookies::FromUrl(GURL(kTestPageUrl));
    request->referrer_policy = ReferrerUtils::GetDefaultNetReferrerPolicy();
    request->resource_type =
        static_cast<int>(mojom::ResourceType::kSubResource);
    request->priority = net::LOW;
    request->mode = network::mojom::RequestMode::kNoCors;

    auto url_request_extra_data =
        base::MakeRefCounted<WebURLRequestExtraData>();
    url_request_extra_data->CopyToResourceRequest(request.get());

    return request;
  }

  ResourceRequestSender* sender() { return resource_request_sender_.get(); }

  void StartAsync(std::unique_ptr<network::ResourceRequest> request,
                  scoped_refptr<ResourceRequestClient> client) {
    sender()->SendAsync(
        std::move(request), scheduler::GetSingleThreadTaskRunnerForTesting(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false,
        /*cors_exempt_header_list=*/Vector<String>(), std::move(client),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(this),
        std::vector<std::unique_ptr<URLLoaderThrottle>>(),
        std::make_unique<ResourceLoadInfoNotifierWrapper>(
            /*resource_load_info_notifier=*/nullptr),
        /*back_forward_cache_loader_helper=*/nullptr);
  }

 protected:
  std::vector<std::pair<mojo::PendingReceiver<network::mojom::URLLoader>,
                        mojo::PendingRemote<network::mojom::URLLoaderClient>>>
      loader_and_clients_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  std::unique_ptr<ResourceRequestSender> resource_request_sender_;

  scoped_refptr<MockRequestClient> mock_client_;
};

// Tests the generation of unique request ids.
TEST_F(ResourceRequestSenderTest, MakeRequestID) {
  int first_id = GenerateRequestId();
  int second_id = GenerateRequestId();

  // Child process ids are unique (per process) and counting from 0 upwards:
  EXPECT_GT(second_id, first_id);
  EXPECT_GE(first_id, 0);
}

class TimeConversionTest : public ResourceRequestSenderTest {
 public:
  void PerformTest(network::mojom::URLResponseHeadPtr response_head) {
    std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
    StartAsync(std::move(request), mock_client_);

    ASSERT_EQ(1u, loader_and_clients_.size());
    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(loader_and_clients_[0].second));
    loader_and_clients_.clear();
    client->OnReceiveResponse(std::move(response_head),
                              mojo::ScopedDataPipeConsumerHandle(),
                              absl::nullopt);
  }

  const network::mojom::URLResponseHead& response_info() const {
    return *response_info_;
  }

 private:
  network::mojom::URLResponseHeadPtr response_info_ =
      network::mojom::URLResponseHead::New();
};

// TODO(simonjam): Enable this when 10829031 lands.
TEST_F(TimeConversionTest, DISABLED_ProperlyInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->request_start = TicksFromMicroseconds(5);
  response_head->response_start = TicksFromMicroseconds(15);
  response_head->load_timing.request_start_time = base::Time::Now();
  response_head->load_timing.request_start = TicksFromMicroseconds(10);
  response_head->load_timing.connect_timing.connect_start =
      TicksFromMicroseconds(13);

  auto request_start = response_head->load_timing.request_start;
  PerformTest(std::move(response_head));

  EXPECT_LT(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.domain_lookup_start);
  EXPECT_LE(request_start,
            response_info().load_timing.connect_timing.connect_start);
}

TEST_F(TimeConversionTest, PartiallyInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->request_start = TicksFromMicroseconds(5);
  response_head->response_start = TicksFromMicroseconds(15);

  PerformTest(std::move(response_head));

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.domain_lookup_start);
}

TEST_F(TimeConversionTest, NotInitialized) {
  auto response_head = network::mojom::URLResponseHead::New();

  PerformTest(std::move(response_head));

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.domain_lookup_start);
}

class CompletionTimeConversionTest : public ResourceRequestSenderTest {
 public:
  void PerformTest(base::TimeTicks remote_request_start,
                   base::TimeTicks completion_time,
                   base::TimeDelta delay) {
    std::unique_ptr<network::ResourceRequest> request(CreateResourceRequest());
    mock_client_ = base::MakeRefCounted<MockRequestClient>();
    StartAsync(std::move(request), mock_client_);

    ASSERT_EQ(1u, loader_and_clients_.size());
    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(loader_and_clients_[0].second));
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->request_start = remote_request_start;
    response_head->load_timing.request_start = remote_request_start;
    response_head->load_timing.receive_headers_end = remote_request_start;
    // We need to put something non-null time, otherwise no values will be
    // copied.
    response_head->load_timing.request_start_time =
        base::Time() + base::Seconds(99);

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
              MOJO_RESULT_OK);

    client->OnReceiveResponse(std::move(response_head),
                              std::move(consumer_handle), absl::nullopt);
    producer_handle.reset();  // The response is empty.

    network::URLLoaderCompletionStatus status;
    status.completion_time = completion_time;

    client->OnComplete(status);

    const base::TimeTicks until = base::TimeTicks::Now() + delay;
    while (base::TimeTicks::Now() < until) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
    base::RunLoop().RunUntilIdle();
    loader_and_clients_.clear();
  }

  base::TimeTicks request_start() const {
    EXPECT_TRUE(mock_client_->received_response());
    return mock_client_->last_load_timing().request_start;
  }
  base::TimeTicks completion_time() const {
    EXPECT_TRUE(mock_client_->complete());
    return mock_client_->completion_status().completion_time;
  }
};

TEST_F(CompletionTimeConversionTest, NullCompletionTimestamp) {
  const auto remote_request_start = base::TimeTicks() + base::Milliseconds(4);

  PerformTest(remote_request_start, base::TimeTicks(), base::TimeDelta());

  EXPECT_EQ(base::TimeTicks(), completion_time());
}

TEST_F(CompletionTimeConversionTest, RemoteRequestStartIsUnavailable) {
  base::TimeTicks begin = base::TimeTicks::Now();

  const auto remote_completion_time = base::TimeTicks() + base::Milliseconds(8);

  PerformTest(base::TimeTicks(), remote_completion_time, base::TimeDelta());

  base::TimeTicks end = base::TimeTicks::Now();
  EXPECT_LE(begin, completion_time());
  EXPECT_LE(completion_time(), end);
}

TEST_F(CompletionTimeConversionTest, Convert) {
  const auto remote_request_start = base::TimeTicks() + base::Milliseconds(4);

  const auto remote_completion_time =
      remote_request_start + base::Milliseconds(3);

  PerformTest(remote_request_start, remote_completion_time,
              base::Milliseconds(15));

  EXPECT_EQ(completion_time(), request_start() + base::Milliseconds(3));
}

}  // namespace blink
