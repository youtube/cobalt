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

#include "base/values.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"
#include "cobalt/h5vcc/metric_event_handler_wrapper.h"

namespace cobalt {
namespace h5vcc {

void H5vccMetrics::OnMetricEvent(
    const h5vcc::MetricEventHandlerWrapper::ScriptValue& event_handler) {
  if (!uploader_callback_) {
    run_event_handler_callback_ = std::make_unique<
        cobalt::browser::metrics::CobaltMetricsUploaderCallback>(
        base::BindRepeating(&H5vccMetrics::RunEventHandler,
                            base::Unretained(this)));
    browser::metrics::CobaltMetricsServicesManager::GetInstance()
        ->SetOnUploadHandler(run_event_handler_callback_.get());
  }

  uploader_callback_ =
      new h5vcc::MetricEventHandlerWrapper(this, event_handler);
}

void H5vccMetrics::RunEventHandler(
    const cobalt::h5vcc::H5vccMetricType& metric_type,
    const std::string& serialized_proto) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&H5vccMetrics::RunEventHandlerInternal, base::Unretained(this),
                 metric_type, serialized_proto));
}

void H5vccMetrics::RunEventHandlerInternal(
    const cobalt::h5vcc::H5vccMetricType& metric_type,
    const std::string& serialized_proto) {
  uploader_callback_->callback.value().Run(metric_type, serialized_proto);
}

void H5vccMetrics::Enable() { ToggleMetricsEnabled(true); }

void H5vccMetrics::Disable() { ToggleMetricsEnabled(false); }

void H5vccMetrics::ToggleMetricsEnabled(bool is_enabled) {
  persistent_settings_->SetPersistentSetting(
      browser::metrics::kMetricEnabledSettingName,
      std::make_unique<base::Value>(is_enabled));
  browser::metrics::CobaltMetricsServicesManager::GetInstance()
      ->ToggleMetricsEnabled(is_enabled);
}

bool H5vccMetrics::IsEnabled() {
  return browser::metrics::CobaltMetricsServicesManager::GetInstance()
      ->IsMetricsReportingEnabled();
}

void H5vccMetrics::SetMetricEventInterval(uint32_t interval_seconds) {
  persistent_settings_->SetPersistentSetting(
      browser::metrics::kMetricEventIntervalSettingName,
      std::make_unique<base::Value>(static_cast<int>(interval_seconds)));
  browser::metrics::CobaltMetricsServicesManager::GetInstance()
      ->SetUploadInterval(interval_seconds);
}

}  // namespace h5vcc
}  // namespace cobalt
