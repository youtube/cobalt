// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_sender.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {
const char kExampleURL[] = "https://a.com/";

constexpr char kReportSenderStatusHistogramName[] =
    "PrivacySandbox.AggregationService.ReportSender.Status";
constexpr char kReportSenderHttpResponseOrNetErrorCodeHistogramName[] =
    "PrivacySandbox.AggregationService.ReportSender.HttpResponseOrNetErrorCode";

base::Value GetExampleContents() {
  base::Value::Dict contents;
  contents.Set("id", "1234");
  return base::Value(std::move(contents));
}

}  // namespace

class AggregatableReportSenderTest : public testing::Test {
 public:
  AggregatableReportSenderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        sender_(AggregatableReportSender::CreateForTesting(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_))) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<AggregatableReportSender> sender_;
};

TEST_F(AggregatableReportSenderTest, ReportSent_RequestAttributesSet) {
  sender_->SendReport(GURL(kExampleURL), GetExampleContents(),
                      base::DoNothing());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(
      test_url_loader_factory_.IsPending(kExampleURL, &pending_request));

  EXPECT_EQ(pending_request->url, GURL(kExampleURL));
  EXPECT_EQ(pending_request->method, net::HttpRequestHeaders::kPostMethod);
  EXPECT_EQ(pending_request->credentials_mode,
            network::mojom::CredentialsMode::kOmit);

  int load_flags = pending_request->load_flags;
  EXPECT_TRUE(load_flags & net::LOAD_BYPASS_CACHE);
  EXPECT_TRUE(load_flags & net::LOAD_DISABLE_CACHE);
}

TEST_F(AggregatableReportSenderTest, ReportSent_IsolationKeyDifferent) {
  sender_->SendReport(GURL(kExampleURL), GetExampleContents(),
                      base::DoNothing());
  sender_->SendReport(GURL(kExampleURL), GetExampleContents(),
                      base::DoNothing());

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);

  const network::ResourceRequest& request1 =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  const network::ResourceRequest& request2 =
      test_url_loader_factory_.GetPendingRequest(1)->request;

  EXPECT_EQ(request1.trusted_params->isolation_info.request_type(),
            net::IsolationInfo::RequestType::kOther);
  EXPECT_EQ(request2.trusted_params->isolation_info.request_type(),
            net::IsolationInfo::RequestType::kOther);

  EXPECT_TRUE(request1.trusted_params->isolation_info.network_isolation_key()
                  .IsTransient());
  EXPECT_TRUE(request2.trusted_params->isolation_info.network_isolation_key()
                  .IsTransient());

  EXPECT_NE(request1.trusted_params->isolation_info.network_isolation_key(),
            request2.trusted_params->isolation_info.network_isolation_key());
}

TEST_F(AggregatableReportSenderTest, ReportSent_UploadDataCorrect) {
  sender_->SendReport(GURL(kExampleURL), GetExampleContents(),
                      base::DoNothing());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(
      test_url_loader_factory_.IsPending(kExampleURL, &pending_request));
  EXPECT_EQ(network::GetUploadData(*pending_request), R"({"id":"1234"})");
}

TEST_F(AggregatableReportSenderTest, ReportSent_StatusOk) {
  base::HistogramTester histograms;

  sender_->SendReport(
      GURL(kExampleURL), GetExampleContents(),
      base::BindLambdaForTesting(
          [&](AggregatableReportSender::RequestStatus status) {
            EXPECT_EQ(status, AggregatableReportSender::RequestStatus::kOk);
          }));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleURL, ""));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  histograms.ExpectUniqueSample(kReportSenderStatusHistogramName,
                                AggregatableReportSender::RequestStatus::kOk,
                                1);
  histograms.ExpectUniqueSample(
      kReportSenderHttpResponseOrNetErrorCodeHistogramName, net::HTTP_OK, 1);
}

TEST_F(AggregatableReportSenderTest, SenderDeletedDuringRequest_NoCrash) {
  sender_->SendReport(GURL(kExampleURL), GetExampleContents(),
                      base::DoNothing());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  sender_.reset();
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleURL, ""));
}

TEST_F(AggregatableReportSenderTest, ReportRequestHangs_Timeout) {
  base::HistogramTester histograms;

  sender_->SendReport(
      GURL(kExampleURL), GetExampleContents(),
      base::BindLambdaForTesting(
          [&](AggregatableReportSender::RequestStatus status) {
            EXPECT_EQ(status,
                      AggregatableReportSender::RequestStatus::kNetworkError);
          }));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  // The request should time out after 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(30));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  histograms.ExpectUniqueSample(
      kReportSenderStatusHistogramName,
      AggregatableReportSender::RequestStatus::kNetworkError, 1);
  histograms.ExpectUniqueSample(
      kReportSenderHttpResponseOrNetErrorCodeHistogramName, net::ERR_TIMED_OUT,
      1);
}

TEST_F(AggregatableReportSenderTest,
       ReportRequestFailsDueToNetworkChange_Retries) {
  // Retry fails
  {
    base::HistogramTester histograms;

    sender_->SendReport(
        GURL(kExampleURL), GetExampleContents(),
        base::BindLambdaForTesting(
            [&](AggregatableReportSender::RequestStatus status) {
              EXPECT_EQ(status,
                        AggregatableReportSender::RequestStatus::kNetworkError);
            }));
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleURL),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // The sender should automatically retry.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleURL),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // We should not retry again.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

    histograms.ExpectUniqueSample(
        kReportSenderStatusHistogramName,
        AggregatableReportSender::RequestStatus::kNetworkError, 1);
    histograms.ExpectUniqueSample(
        kReportSenderHttpResponseOrNetErrorCodeHistogramName,
        net::ERR_NETWORK_CHANGED, 1);
  }

  // Retry succeeds
  {
    base::HistogramTester histograms;

    sender_->SendReport(
        GURL(kExampleURL), GetExampleContents(),
        base::BindLambdaForTesting(
            [&](AggregatableReportSender::RequestStatus status) {
              EXPECT_EQ(status, AggregatableReportSender::RequestStatus::kOk);
            }));
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleURL),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), std::string());

    // The sender should automatically retry.
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

    // Simulate a second request with respoonse.
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kExampleURL, ""));

    histograms.ExpectUniqueSample(kReportSenderStatusHistogramName,
                                  AggregatableReportSender::RequestStatus::kOk,
                                  1);
    histograms.ExpectUniqueSample(
        kReportSenderHttpResponseOrNetErrorCodeHistogramName, net::HTTP_OK, 1);
  }
}

TEST_F(AggregatableReportSenderTest, HttpError_CallbackRuns) {
  base::HistogramTester histograms;

  bool callback_run = false;
  sender_->SendReport(
      GURL(kExampleURL), GetExampleContents(),
      base::BindLambdaForTesting(
          [&](AggregatableReportSender::RequestStatus status) {
            EXPECT_EQ(status,
                      AggregatableReportSender::RequestStatus::kServerError);
            callback_run = true;
          }));

  // We should run the callback even if there is an http error.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kExampleURL, "", net::HTTP_BAD_REQUEST));

  EXPECT_TRUE(callback_run);

  histograms.ExpectUniqueSample(
      kReportSenderStatusHistogramName,
      AggregatableReportSender::RequestStatus::kServerError, 1);
  histograms.ExpectUniqueSample(
      kReportSenderHttpResponseOrNetErrorCodeHistogramName,
      net::HTTP_BAD_REQUEST, 1);
}

TEST_F(AggregatableReportSenderTest, ManyReports_AllSentSuccessfully) {
  GURL url = GURL(kExampleURL);
  int num_callbacks_run = 0;
  for (int i = 0; i < 10; i++) {
    sender_->SendReport(
        url, GetExampleContents(),
        base::BindLambdaForTesting(
            [&](AggregatableReportSender::RequestStatus status) {
              EXPECT_EQ(status, AggregatableReportSender::RequestStatus::kOk);
              ++num_callbacks_run;
            }));
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 10);

  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kExampleURL, ""));
  }

  EXPECT_EQ(num_callbacks_run, 10);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(AggregatableReportSenderTest, StatusHistoram_Expected) {
  // All OK.
  {
    base::HistogramTester histograms;
    sender_->SendReport(GURL(kExampleURL), GetExampleContents(),
                        base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kExampleURL, ""));
    histograms.ExpectUniqueSample(kReportSenderStatusHistogramName,
                                  AggregatableReportSender::RequestStatus::kOk,
                                  1);
    histograms.ExpectUniqueSample(
        kReportSenderHttpResponseOrNetErrorCodeHistogramName, net::HTTP_OK, 1);
  }

  // Network error.
  {
    base::HistogramTester histograms;
    sender_->SendReport(GURL(kExampleURL), GetExampleContents(),
                        base::DoNothing());
    network::URLLoaderCompletionStatus completion_status(net::ERR_FAILED);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kExampleURL), completion_status,
        network::mojom::URLResponseHead::New(), std::string()));
    histograms.ExpectUniqueSample(
        kReportSenderStatusHistogramName,
        AggregatableReportSender::RequestStatus::kNetworkError, 1);
    histograms.ExpectUniqueSample(
        kReportSenderHttpResponseOrNetErrorCodeHistogramName, net::ERR_FAILED,
        1);
  }

  // Server error.
  {
    base::HistogramTester histograms;
    sender_->SendReport(GURL(kExampleURL), GetExampleContents(),
                        base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kExampleURL, std::string(), net::HTTP_UNAUTHORIZED));
    histograms.ExpectUniqueSample(
        kReportSenderStatusHistogramName,
        AggregatableReportSender::RequestStatus::kServerError, 1);
    histograms.ExpectUniqueSample(
        kReportSenderHttpResponseOrNetErrorCodeHistogramName,
        net::HTTP_UNAUTHORIZED, 1);
  }
}

}  // namespace content
