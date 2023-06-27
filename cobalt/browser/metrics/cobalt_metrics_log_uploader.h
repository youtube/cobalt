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

#ifndef COBALT_BROWSER_METRICS_COBALT_METRICS_LOG_UPLOADER_H_
#define COBALT_BROWSER_METRICS_COBALT_METRICS_LOG_UPLOADER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "cobalt/browser/metrics/cobalt_metrics_uploader_callback.h"
#include "cobalt/h5vcc/metric_event_handler_wrapper.h"
#include "components/metrics/metrics_log_uploader.h"
#include "third_party/metrics_proto/reporting_info.pb.h"


namespace cobalt {
namespace browser {
namespace metrics {

class ReportingInfo;

// A Cobalt implementation of MetricsLogUploader that intercepts metric's logs
// when they're ready to be sent to the server and forwards them to the web
// client via the H5vcc API.
class CobaltMetricsLogUploader : public ::metrics::MetricsLogUploader {
 public:
  CobaltMetricsLogUploader(
      ::metrics::MetricsLogUploader::MetricServiceType service_type,
      const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete);

  virtual ~CobaltMetricsLogUploader() {}

  // Uploads a log with the specified |compressed_log_data| and |log_hash|.
  // |log_hash| is expected to be the hex-encoded SHA1 hash of the log data
  // before compression.
  void UploadLog(const std::string& compressed_log_data,
                 const std::string& log_hash,
                 const ::metrics::ReportingInfo& reporting_info);

  // Sets the event handler wrapper to be called when metrics are ready for
  // upload. This should be the JavaScript H5vcc callback implementation.
  void SetOnUploadHandler(
      const CobaltMetricsUploaderCallback* metric_event_handler);

 private:
  const ::metrics::MetricsLogUploader::MetricServiceType service_type_;
  const ::metrics::MetricsLogUploader::UploadCallback on_upload_complete_;
  const CobaltMetricsUploaderCallback* upload_handler_ = nullptr;
};

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_METRICS_LOG_UPLOADER_H_
