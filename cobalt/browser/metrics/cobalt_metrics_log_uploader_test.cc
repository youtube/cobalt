// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "base/test/mock_callback.h"
#include "cobalt/browser/metrics/cobalt_metrics_uploader_callback.h"
#include "cobalt/h5vcc/h5vcc_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/cobalt_uma_event.pb.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace cobalt {
namespace browser {
namespace metrics {

using cobalt::h5vcc::H5vccMetrics;
using cobalt::h5vcc::MetricEventHandler;
using cobalt::h5vcc::MetricEventHandlerWrapper;
using ::testing::_;
using ::testing::Eq;
using ::testing::StrEq;
using ::testing::StrictMock;


class CobaltMetricsLogUploaderTest : public ::testing::Test {
 public:
  void SetUp() override {
    uploader_ = std::make_unique<CobaltMetricsLogUploader>(
        ::metrics::MetricsLogUploader::MetricServiceType::UMA,
        base::Bind(&CobaltMetricsLogUploaderTest::UploadCompleteCallback,
                   base::Unretained(this)));
  }

  void TearDown() override {}

  void UploadCompleteCallback(int response_code, int error_code,
                              bool was_https) {
    callback_count_++;
  }

 protected:
  std::unique_ptr<CobaltMetricsLogUploader> uploader_;
  int callback_count_ = 0;
};

TEST_F(CobaltMetricsLogUploaderTest, TriggersUploadHandler) {
  base::MockCallback<CobaltMetricsUploaderCallback> mock_upload_handler;
  const auto cb = mock_upload_handler.Get();
  uploader_->SetOnUploadHandler(&cb);
  ::metrics::ReportingInfo dummy_reporting_info;
  dummy_reporting_info.set_attempt_count(33);
  ::metrics::ChromeUserMetricsExtension uma_log;
  uma_log.set_session_id(1234);
  uma_log.set_client_id(1234);
  auto histogram_event = uma_log.add_histogram_event();
  histogram_event->set_name_hash(1234);
  auto user_event = uma_log.add_user_action_event();
  user_event->set_name_hash(42);

  CobaltUMAEvent cobalt_event;
  cobalt_event.mutable_histogram_event()->CopyFrom(uma_log.histogram_event());
  cobalt_event.mutable_user_action_event()->CopyFrom(
      uma_log.user_action_event());
  cobalt_event.mutable_reporting_info()->CopyFrom(dummy_reporting_info);

  std::string compressed_message;
  compression::GzipCompress(uma_log.SerializeAsString(), &compressed_message);
  EXPECT_CALL(mock_upload_handler,
              Run(Eq(h5vcc::H5vccMetricType::kH5vccMetricTypeCobaltUma),
                  StrEq(cobalt_event.SerializeAsString())))
      .Times(1);
  uploader_->UploadLog(compressed_message, "fake_hash", dummy_reporting_info);
  ASSERT_EQ(callback_count_, 1);

  ::metrics::ChromeUserMetricsExtension uma_log2;
  uma_log2.set_session_id(456);
  uma_log2.set_client_id(567);
  CobaltUMAEvent cobalt_event2;
  cobalt_event2.mutable_histogram_event()->CopyFrom(uma_log2.histogram_event());
  cobalt_event2.mutable_user_action_event()->CopyFrom(
      uma_log2.user_action_event());
  cobalt_event2.mutable_reporting_info()->CopyFrom(dummy_reporting_info);
  std::string compressed_message2;
  compression::GzipCompress(uma_log2.SerializeAsString(), &compressed_message2);
  EXPECT_CALL(mock_upload_handler,
              Run(Eq(h5vcc::H5vccMetricType::kH5vccMetricTypeCobaltUma),
                  StrEq(cobalt_event2.SerializeAsString())))
      .Times(1);
  uploader_->UploadLog(compressed_message2, "fake_hash", dummy_reporting_info);
  ASSERT_EQ(callback_count_, 2);
}

TEST_F(CobaltMetricsLogUploaderTest, UnknownMetricTypeDoesntTriggerUpload) {
  uploader_.reset(new CobaltMetricsLogUploader(
      ::metrics::MetricsLogUploader::MetricServiceType::UKM,
      base::Bind(&CobaltMetricsLogUploaderTest::UploadCompleteCallback,
                 base::Unretained(this))));
  base::MockCallback<CobaltMetricsUploaderCallback> mock_upload_handler;
  const auto cb = mock_upload_handler.Get();
  uploader_->SetOnUploadHandler(&cb);
  ::metrics::ReportingInfo dummy_reporting_info;
  ::metrics::ChromeUserMetricsExtension uma_log;
  uma_log.set_session_id(1234);
  uma_log.set_client_id(1234);
  std::string compressed_message;
  compression::GzipCompress(uma_log.SerializeAsString(), &compressed_message);
  EXPECT_CALL(mock_upload_handler, Run(_, _)).Times(0);
  uploader_->UploadLog(compressed_message, "fake_hash", dummy_reporting_info);
  // Even though we don't upload this log, we still need to trigger the complete
  // callback so the metric code can keep running.
  ASSERT_EQ(callback_count_, 1);
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
