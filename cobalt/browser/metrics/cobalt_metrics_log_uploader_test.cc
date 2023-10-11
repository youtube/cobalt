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

#include "base/base64url.h"
#include "base/test/mock_callback.h"
#include "cobalt/base/event.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/base/on_metric_upload_event.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"
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
    dispatcher_ = std::make_unique<base::EventDispatcher>();
    dispatcher_->AddEventCallback(
        base::OnMetricUploadEvent::TypeId(),
        base::Bind(&CobaltMetricsLogUploaderTest::OnMetricUploadEventHandler,
                   base::Unretained(this)));

    uploader_ = std::make_unique<CobaltMetricsLogUploader>(
        ::metrics::MetricsLogUploader::MetricServiceType::UMA,
        base::Bind(&CobaltMetricsLogUploaderTest::UploadCompleteCallback,
                   base::Unretained(this)));
  }

  void TearDown() override {}

  void OnMetricUploadEventHandler(const base::Event* event) {
    std::unique_ptr<base::OnMetricUploadEvent> on_metric_upload_event(
        new base::OnMetricUploadEvent(event));
    last_metric_type_ = on_metric_upload_event->metric_type();
    last_serialized_proto_ = on_metric_upload_event->serialized_proto();
  }

  void UploadCompleteCallback(int response_code, int error_code,
                              bool was_https) {
    callback_count_++;
  }

 protected:
  std::unique_ptr<CobaltMetricsLogUploader> uploader_;
  std::unique_ptr<base::EventDispatcher> dispatcher_;
  int callback_count_ = 0;
  cobalt::h5vcc::H5vccMetricType last_metric_type_;
  std::string last_serialized_proto_ = "";
};

TEST_F(CobaltMetricsLogUploaderTest, TriggersUploadHandler) {
  uploader_->SetEventDispatcher(dispatcher_.get());
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
  std::string base64_encoded_proto;
  base::Base64UrlEncode(cobalt_event.SerializeAsString(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &base64_encoded_proto);
  uploader_->UploadLog(compressed_message, "fake_hash", dummy_reporting_info);
  ASSERT_EQ(callback_count_, 1);
  ASSERT_EQ(last_metric_type_,
            cobalt::h5vcc::H5vccMetricType::kH5vccMetricTypeCobaltUma);
  ASSERT_EQ(last_serialized_proto_, base64_encoded_proto);

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
  std::string base64_encoded_proto2;
  base::Base64UrlEncode(cobalt_event2.SerializeAsString(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &base64_encoded_proto2);
  uploader_->UploadLog(compressed_message2, "fake_hash", dummy_reporting_info);
  ASSERT_EQ(last_metric_type_,
            cobalt::h5vcc::H5vccMetricType::kH5vccMetricTypeCobaltUma);
  ASSERT_EQ(last_serialized_proto_, base64_encoded_proto2);
  ASSERT_EQ(callback_count_, 2);
}

TEST_F(CobaltMetricsLogUploaderTest, UnknownMetricTypeDoesntTriggerUpload) {
  uploader_.reset(new CobaltMetricsLogUploader(
      ::metrics::MetricsLogUploader::MetricServiceType::UKM,
      base::Bind(&CobaltMetricsLogUploaderTest::UploadCompleteCallback,
                 base::Unretained(this))));
  uploader_->SetEventDispatcher(dispatcher_.get());
  ::metrics::ReportingInfo dummy_reporting_info;
  ::metrics::ChromeUserMetricsExtension uma_log;
  uma_log.set_session_id(1234);
  uma_log.set_client_id(1234);
  std::string compressed_message;
  compression::GzipCompress(uma_log.SerializeAsString(), &compressed_message);
  uploader_->UploadLog(compressed_message, "fake_hash", dummy_reporting_info);
  ASSERT_EQ(last_serialized_proto_, "");
  // Even though we don't upload this log, we still need to trigger the complete
  // callback so the metric code can keep running.
  ASSERT_EQ(callback_count_, 1);
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
