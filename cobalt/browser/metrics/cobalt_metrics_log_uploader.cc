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

#include "base/logging.h"
#include "cobalt/browser/metrics/cobalt_metrics_uploader_callback.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_log_uploader.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/cobalt_uma_event.pb.h"
#include "third_party/metrics_proto/reporting_info.pb.h"

namespace cobalt {
namespace browser {
namespace metrics {

// Helper method to create a trimmed down and sanitized version of the UMA
// proto for Cobalt
void PopulateCobaltUmaEvent(
    const ::metrics::ChromeUserMetricsExtension& uma_proto,
    const ::metrics::ReportingInfo reporting_info_proto,
    CobaltUMAEvent& cobalt_proto) {
  cobalt_proto.mutable_histogram_event()->CopyFrom(uma_proto.histogram_event());
  cobalt_proto.mutable_user_action_event()->CopyFrom(
      uma_proto.user_action_event());
  cobalt_proto.mutable_reporting_info()->CopyFrom(reporting_info_proto);
}

CobaltMetricsLogUploader::CobaltMetricsLogUploader(
    ::metrics::MetricsLogUploader::MetricServiceType service_type,
    const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
    : service_type_(service_type), on_upload_complete_(on_upload_complete) {}

void CobaltMetricsLogUploader::UploadLog(
    const std::string& compressed_log_data, const std::string& log_hash,
    const ::metrics::ReportingInfo& reporting_info) {
  if (service_type_ == ::metrics::MetricsLogUploader::UMA) {
    if (upload_handler_ != nullptr) {
      std::string uncompressed_serialized_proto;
      ::metrics::DecodeLogData(compressed_log_data,
                               &uncompressed_serialized_proto);

      ::metrics::ChromeUserMetricsExtension uma_event;
      uma_event.ParseFromString(uncompressed_serialized_proto);
      CobaltUMAEvent cobalt_uma_event;
      PopulateCobaltUmaEvent(uma_event, reporting_info, cobalt_uma_event);
      LOG(INFO) << "Publishing Cobalt metrics upload event. Type: "
                << h5vcc::H5vccMetricType::kH5vccMetricTypeCobaltUma;
      // Publish the trimmed Cobalt UMA proto.
      upload_handler_->Run(h5vcc::H5vccMetricType::kH5vccMetricTypeCobaltUma,
                           cobalt_uma_event.SerializeAsString());
    }
  }

  // Arguments to callback don't matter much here as we're not really doing
  // anything but forwarding to the H5vcc API. Run(http response code, net
  // code, and was https).
  on_upload_complete_.Run(/*status*/ 200, /* error_code */ 0,
                          /*was_https*/ true);
}

void CobaltMetricsLogUploader::SetOnUploadHandler(
    const CobaltMetricsUploaderCallback* upload_handler) {
  upload_handler_ = upload_handler;
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
