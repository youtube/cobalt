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

#include "cobalt/browser/metrics/cobalt_metrics_log_uploader.h"

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom.h"
#include "components/metrics/metrics_log_uploader.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/cobalt_uma_event.pb.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace cobalt {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::SaveArg;
using ::testing::StrictMock;

class MockMetricsListener : public h5vcc_metrics::mojom::MetricsListener {
 public:
  MOCK_METHOD(void,
              OnMetrics,
              (h5vcc_metrics::mojom::H5vccMetricType type,
               const std::string& serialized_proto_data),
              (override));
};

class CobaltMetricsLogUploaderTest : public ::testing::Test {
 public:
  void OnUploadComplete(int response_code,
                        int error_code,
                        bool was_https,
                        bool force_discard,
                        std::string_view app_locale) {
    upload_complete_called_ = true;
    response_code_ = response_code;
    error_code_ = error_code;
    was_https_ = was_https;
    force_discard_ = force_discard;
    app_locale_ = app_locale;
  }

 protected:
  CobaltMetricsLogUploaderTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        uploader_(std::make_unique<CobaltMetricsLogUploader>(
            ::metrics::MetricsLogUploader::MetricServiceType::UMA)) {
    mojo::core::Init();
  }

  void SetUp() override {
    mojo::PendingRemote<h5vcc_metrics::mojom::MetricsListener> listener_remote;
    receiver_ =
        std::make_unique<mojo::Receiver<h5vcc_metrics::mojom::MetricsListener>>(
            &mock_listener_, listener_remote.InitWithNewPipeAndPassReceiver());
    uploader_->SetMetricsListener(std::move(listener_remote));

    ResetUploadCompleteFlags();
    uploader_->setOnUploadComplete(
        base::BindRepeating(&CobaltMetricsLogUploaderTest::OnUploadComplete,
                            base::Unretained(this)));
  }

  void TearDown() override {
    uploader_.reset();
    receiver_.reset();
    task_environment_.RunUntilIdle();  // Flush any pending Mojo tasks.
  }

  void ResetUploadCompleteFlags() {
    upload_complete_called_ = false;
    response_code_ = -1;
    error_code_ = -1;
    was_https_ = false;
    force_discard_ = true;  // Default to an unexpected value.
    app_locale_ = "UNSET";
  }

  // Helper to create compressed log data from a ChromeUserMetricsExtension
  // proto.
  std::string CreateCompressedLogData(
      const ::metrics::ChromeUserMetricsExtension& uma_proto) {
    std::string serialized_proto;
    uma_proto.SerializeToString(&serialized_proto);
    std::string compressed_log_data;
    // The uploader uses metrics::DecodeLogData, which expects gzipped data.
    EXPECT_TRUE(
        compression::GzipCompress(serialized_proto, &compressed_log_data));
    return compressed_log_data;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<CobaltMetricsLogUploader> uploader_;
  StrictMock<MockMetricsListener>
      mock_listener_;  // StrictMock helps catch unexpected calls.
  std::unique_ptr<mojo::Receiver<h5vcc_metrics::mojom::MetricsListener>>
      receiver_;

  // Flags and captured values for the OnUploadComplete callback.
  bool upload_complete_called_;
  int response_code_;
  int error_code_;
  bool was_https_;
  bool force_discard_;
  std::string app_locale_;
};

TEST_F(CobaltMetricsLogUploaderTest, UploadLogNoListenerBound) {
  // Create a new uploader instance that never gets a listener set.
  uploader_ = std::make_unique<CobaltMetricsLogUploader>(
      ::metrics::MetricsLogUploader::MetricServiceType::UMA);
  ResetUploadCompleteFlags();  // Reset flags for this specific uploader.
  uploader_->setOnUploadComplete(base::BindRepeating(
      &CobaltMetricsLogUploaderTest::OnUploadComplete, base::Unretained(this)));

  ::metrics::ReportingInfo reporting_info;
  // No listener, so OnMetrics shouldn't be called.
  EXPECT_CALL(mock_listener_, OnMetrics(_, _)).Times(0);
  uploader_->UploadLog("test_data", metrics::LogMetadata(), "test_hash",
                       "test_signature", reporting_info);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(upload_complete_called_);
  EXPECT_EQ(response_code_, 200);
  EXPECT_EQ(error_code_, 0);
  EXPECT_TRUE(was_https_);
  EXPECT_FALSE(force_discard_);
  EXPECT_EQ(app_locale_, "");
}

TEST_F(CobaltMetricsLogUploaderTest, UploadLogListenerConnectionClosed) {
  // The listener is set up in SetUp(). Now, close the connection.
  ASSERT_TRUE(receiver_ && receiver_->is_bound());
  receiver_
      .reset();  // This will trigger the disconnect handler on the uploader.
  task_environment_.RunUntilIdle();  // Allow disconnect handler to run.

  ::metrics::ReportingInfo reporting_info;
  // StrictMock ensures OnMetrics is not called after disconnection.

  uploader_->UploadLog("test_data", metrics::LogMetadata(), "test_hash",
                       "test_signature", reporting_info);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(upload_complete_called_);
  EXPECT_EQ(response_code_, 200);
  EXPECT_EQ(error_code_, 0);
  EXPECT_TRUE(was_https_);
}

TEST_F(CobaltMetricsLogUploaderTest, UploadLogNonUmaServiceType) {
  // Recreate uploader_ with a non-UMA service type.
  uploader_ = std::make_unique<CobaltMetricsLogUploader>(
      ::metrics::MetricsLogUploader::MetricServiceType::UKM);

  // Bind the mock listener to this new uploader instance.
  mojo::PendingRemote<h5vcc_metrics::mojom::MetricsListener> listener_remote;
  receiver_ =
      std::make_unique<mojo::Receiver<h5vcc_metrics::mojom::MetricsListener>>(
          &mock_listener_, listener_remote.InitWithNewPipeAndPassReceiver());
  uploader_->SetMetricsListener(std::move(listener_remote));

  ResetUploadCompleteFlags();
  uploader_->setOnUploadComplete(base::BindRepeating(
      &CobaltMetricsLogUploaderTest::OnUploadComplete, base::Unretained(this)));

  ::metrics::ReportingInfo reporting_info;
  // StrictMock ensures OnMetrics is not called for non-UMA types.

  uploader_->UploadLog("test_data_for_ukm", metrics::LogMetadata(), "ukm_hash",
                       "ukm_signature", reporting_info);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(upload_complete_called_);
  EXPECT_EQ(response_code_, 200);
  EXPECT_EQ(error_code_, 0);
  EXPECT_TRUE(was_https_);
}

TEST_F(CobaltMetricsLogUploaderTest, UploadLogUmaServiceTypeSuccessfulUpload) {
  // uploader_ is already UMA type and listener is set from SetUp().

  ::metrics::ChromeUserMetricsExtension uma_proto;
  uma_proto.set_client_id(123456);
  uma_proto.set_session_id(789012);
  // Add a histogram event to check population.
  auto* histogram_event = uma_proto.add_histogram_event();
  histogram_event->set_name_hash(UINT64_C(0xabcdef1234567890));
  histogram_event->set_sum(150);
  histogram_event->add_bucket()->set_min(10);
  histogram_event->add_bucket()->set_max(20);
  auto* user_action_event = uma_proto.add_user_action_event();
  user_action_event->set_name_hash(UINT64_C(0x1234509876fedcba));
  user_action_event->set_time_sec(1678886400);

  std::string compressed_log_data = CreateCompressedLogData(uma_proto);

  ::metrics::ReportingInfo reporting_info_proto;  // Actual proto.
  reporting_info_proto.set_attempt_count(3);

  std::string captured_payload;
  h5vcc_metrics::mojom::H5vccMetricType captured_type;

  EXPECT_CALL(mock_listener_, OnMetrics(_, _))
      .WillOnce(
          DoAll(SaveArg<0>(&captured_type), SaveArg<1>(&captured_payload)));
  uploader_->UploadLog(compressed_log_data, metrics::LogMetadata(),
                       "test_hash_uma", "test_signature_uma",
                       reporting_info_proto);
  task_environment_.RunUntilIdle();  // Ensure Mojo message is processed.

  EXPECT_TRUE(upload_complete_called_);
  EXPECT_EQ(response_code_, 200);
  EXPECT_EQ(error_code_, 0);
  EXPECT_TRUE(was_https_);
  EXPECT_FALSE(force_discard_);
  EXPECT_EQ(app_locale_, "");

  ASSERT_EQ(captured_type, h5vcc_metrics::mojom::H5vccMetricType::kCobaltUma);

  std::string decoded_payload_str;
  ASSERT_TRUE(base::Base64UrlDecode(
      captured_payload, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
      &decoded_payload_str));

  browser::metrics::CobaltUMAEvent cobalt_event_received;
  ASSERT_TRUE(cobalt_event_received.ParseFromString(decoded_payload_str));

  // Verify data populated by PopulateCobaltUmaEvent.
  ASSERT_EQ(cobalt_event_received.histogram_event_size(),
            uma_proto.histogram_event_size());
  if (cobalt_event_received.histogram_event_size() > 0 &&
      uma_proto.histogram_event_size() > 0) {
    EXPECT_EQ(cobalt_event_received.histogram_event(0).name_hash(),
              uma_proto.histogram_event(0).name_hash());
    EXPECT_EQ(cobalt_event_received.histogram_event(0).sum(),
              uma_proto.histogram_event(0).sum());
    ASSERT_EQ(cobalt_event_received.histogram_event(0).bucket_size(),
              uma_proto.histogram_event(0).bucket_size());
    if (cobalt_event_received.histogram_event(0).bucket_size() > 0) {
      EXPECT_EQ(cobalt_event_received.histogram_event(0).bucket(0).min(),
                uma_proto.histogram_event(0).bucket(0).min());
      EXPECT_EQ(cobalt_event_received.histogram_event(0).bucket(0).max(),
                uma_proto.histogram_event(0).bucket(0).max());
    }
  }

  ASSERT_EQ(cobalt_event_received.user_action_event_size(),
            uma_proto.user_action_event_size());
  if (cobalt_event_received.user_action_event_size() > 0 &&
      uma_proto.user_action_event_size() > 0) {
    EXPECT_EQ(cobalt_event_received.user_action_event(0).name_hash(),
              uma_proto.user_action_event(0).name_hash());
    EXPECT_DOUBLE_EQ(cobalt_event_received.user_action_event(0).time_sec(),
                     uma_proto.user_action_event(0).time_sec());
  }

  // Verify ReportingInfo part.
  EXPECT_TRUE(cobalt_event_received.has_reporting_info());
  EXPECT_EQ(cobalt_event_received.reporting_info().attempt_count(),
            reporting_info_proto.attempt_count());
}
TEST_F(CobaltMetricsLogUploaderTest, SetOnUploadCompleteCallbackIsUsed) {
  bool custom_callback_fired = false;
  int cb_response_code = -1;

  auto upload_complete_cb =
      [&](int response_code_param, int /*error_code_param*/,
          bool /*was_https_param*/, bool /*force_discard_param*/,
          std::string_view /*app_locale_param*/) {
        custom_callback_fired = true;
        cb_response_code = response_code_param;
      };

  uploader_ = std::make_unique<CobaltMetricsLogUploader>(
      ::metrics::MetricsLogUploader::MetricServiceType::UMA);

  uploader_->setOnUploadComplete(
      base::BindLambdaForTesting(upload_complete_cb));

  ::metrics::ReportingInfo reporting_info;
  uploader_->UploadLog("dummy_data", metrics::LogMetadata(), "hash", "sig",
                       reporting_info);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(custom_callback_fired);
  EXPECT_EQ(cb_response_code, 200);
}

}  // namespace cobalt
