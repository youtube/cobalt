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

#ifndef COBALT_BROWSER_METRICS_COBALT_METRICS_LOG_UPLOADER_H_
#define COBALT_BROWSER_METRICS_COBALT_METRICS_LOG_UPLOADER_H_

#include <string>

#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom.h"
#include "components/metrics/metrics_log_uploader.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace metrics {
class ReportingInfo;
}

namespace cobalt {

// Cobalt-specific UMA logs uploader. For Cobalt, we pass the metric payload
// to a JavaScript listener to be handled by the web application.
class CobaltMetricsLogUploader : public metrics::MetricsLogUploader {
 public:
  explicit CobaltMetricsLogUploader(
      ::metrics::MetricsLogUploader::MetricServiceType service_type =
          ::metrics::MetricsLogUploader::MetricServiceType::UMA)
      : service_type_(service_type) {}

  ~CobaltMetricsLogUploader() = default;

  void UploadLog(const std::string& compressed_log_data,
                 const metrics::LogMetadata& log_metadata,
                 const std::string& log_hash,
                 const std::string& log_signature,
                 const metrics::ReportingInfo& reporting_info) override;

  base::WeakPtr<CobaltMetricsLogUploader> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Virtual for overriding in tests.
  virtual void setOnUploadComplete(
      const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
    on_upload_complete_ = on_upload_complete;
  }

  // Virtual for overriding in tests.
  virtual void SetMetricsListener(
      ::mojo::PendingRemote<::h5vcc_metrics::mojom::MetricsListener> listener);

 private:
  // Cleans up and resets any open listeners.
  void OnCloseConnection();

  mojo::Remote<h5vcc_metrics::mojom::MetricsListener> metrics_listener_;
  const ::metrics::MetricsLogUploader::MetricServiceType service_type_;
  ::metrics::MetricsLogUploader::UploadCallback on_upload_complete_;

  // NOTE: Do not add member variables after weak_factory_
  // It should be the first one destroyed among all members.
  // See base/memory/weak_ptr.h.
  base::WeakPtrFactory<CobaltMetricsLogUploader> weak_factory_{this};
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_METRICS_LOG_UPLOADER_H_
