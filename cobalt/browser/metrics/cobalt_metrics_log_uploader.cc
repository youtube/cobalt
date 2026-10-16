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
#include "base/logging.h"
#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_log_uploader.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/cobalt_uma_event.pb.h"
#include "third_party/metrics_proto/reporting_info.pb.h"

namespace cobalt {

using browser::metrics::CobaltUMAEvent;

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

void CobaltMetricsLogUploader::UploadLog(
    const std::string& compressed_log_data,
    const metrics::LogMetadata& log_metadata,
    const std::string& log_hash,
    const std::string& log_signature,
    const metrics::ReportingInfo& reporting_info) {
  // Always run the upload complete callback as a success. Arguments to callback
  // don't matter much here as we're not really doing anything but forwarding to
  // the H5vcc API.
  base::ScopedClosureRunner closure(
      base::BindOnce(on_upload_complete_, /*status*/ 200, /* error_code */ 0,
                     /*was_https*/ true, false, ""));

  if (!metrics_listener_.is_bound()) {
    return;
  }

  // For now, we only support UMA.
  if (service_type_ != ::metrics::MetricsLogUploader::MetricServiceType::UMA) {
    return;
  }

  std::string uncompressed_serialized_proto;
  ::metrics::DecodeLogData(compressed_log_data, &uncompressed_serialized_proto);
  ::metrics::ChromeUserMetricsExtension uma_event;
  uma_event.ParseFromString(uncompressed_serialized_proto);
  CobaltUMAEvent cobalt_uma_event;
  PopulateCobaltUmaEvent(uma_event, reporting_info, cobalt_uma_event);
  std::string base64_encoded_proto;
  // Base64 encode the payload as web client's can't consume it without
  // corrupting the data (see b/293431381). Also, use a URL/web safe
  // encoding so it can be safely included in any web network request.
  base::Base64UrlEncode(cobalt_uma_event.SerializeAsString(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &base64_encoded_proto);
  DLOG(INFO) << "UMA Payload uploading! Hash: " << log_hash;
  metrics_listener_->OnMetrics(
      h5vcc_metrics::mojom::H5vccMetricType::kCobaltUma, base64_encoded_proto);
}

void CobaltMetricsLogUploader::SetMetricsListener(
    ::mojo::PendingRemote<::h5vcc_metrics::mojom::MetricsListener> listener) {
  metrics_listener_.Bind(std::move(listener));
  metrics_listener_.set_disconnect_handler(base::BindOnce(
      &CobaltMetricsLogUploader::OnCloseConnection, GetWeakPtr()));
}

void CobaltMetricsLogUploader::OnCloseConnection() {
  metrics_listener_.reset();
}
}  // namespace cobalt
