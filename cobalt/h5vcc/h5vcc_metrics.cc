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

#include "cobalt/h5vcc/h5vcc_metrics.h"

#include <string>

#include "cobalt/browser/metrics/cobalt_h5vcc_metrics_uploader_callback.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"
#include "cobalt/h5vcc/metric_event_handler_wrapper.h"

namespace cobalt {
namespace h5vcc {

void H5vccMetrics::OnMetricEvent(
    const h5vcc::MetricEventHandlerWrapper::ScriptValue& eventHandler) {
  auto callback =
      new cobalt::browser::metrics::CobaltH5vccMetricsUploaderCallback(
          new h5vcc::MetricEventHandlerWrapper(this, eventHandler));
  uploader_callback_.reset(callback);
  browser::metrics::CobaltMetricsServiceClient* client =
      static_cast<browser::metrics::CobaltMetricsServiceClient*>(
          browser::metrics::CobaltMetricsServicesManager::GetInstance()
              ->GetMetricsServiceClient());
  DCHECK(client);
  client->SetOnUploadHandler(uploader_callback_.get());
}

void H5vccMetrics::ToggleMetricsEnabled(bool isEnabled) {
  browser::metrics::CobaltMetricsServicesManagerClient* client =
      static_cast<browser::metrics::CobaltMetricsServicesManagerClient*>(
          browser::metrics::CobaltMetricsServicesManager::GetInstance()
              ->GetMetricsServicesManagerClient());
  DCHECK(client);
  is_enabled_ = isEnabled;
  client->GetEnabledStateProvider()->SetConsentGiven(isEnabled);
  client->GetEnabledStateProvider()->SetReportingEnabled(isEnabled);
  browser::metrics::CobaltMetricsServicesManager::GetInstance()
      ->UpdateUploadPermissions(isEnabled);
}

bool H5vccMetrics::GetMetricsEnabled() { return is_enabled_; }

}  // namespace h5vcc
}  // namespace cobalt
