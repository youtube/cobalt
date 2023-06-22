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
#include "third_party/metrics_proto/reporting_info.pb.h"

namespace cobalt {
namespace browser {
namespace metrics {

CobaltMetricsLogUploader::CobaltMetricsLogUploader(
    ::metrics::MetricsLogUploader::MetricServiceType service_type,
    const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
    : service_type_(service_type), on_upload_complete_(on_upload_complete) {}

void CobaltMetricsLogUploader::UploadLog(
    const std::string& compressed_log_data, const std::string& log_hash,
    const ::metrics::ReportingInfo& reporting_info) {
  if (service_type_ == ::metrics::MetricsLogUploader::UMA) {
    std::string uncompressed_serialized_proto;
    ::metrics::DecodeLogData(compressed_log_data,
                             &uncompressed_serialized_proto);
    if (upload_handler_ != nullptr) {
      upload_handler_->Run(h5vcc::H5vccMetricType::kH5vccMetricTypeUma,
                           uncompressed_serialized_proto);
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
