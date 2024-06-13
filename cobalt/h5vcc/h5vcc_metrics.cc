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
#include <utility>

#include "base/values.h"
#include "cobalt/base/event.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/base/on_metric_upload_event.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager.h"
#include "cobalt/h5vcc/h5vcc_metric_type.h"
#include "cobalt/h5vcc/metric_event_handler_wrapper.h"
#include "cobalt/web/environment_settings_helper.h"

namespace cobalt {
namespace h5vcc {


H5vccMetrics::H5vccMetrics(
    persistent_storage::PersistentSettings* persistent_settings,
    base::EventDispatcher* event_dispatcher)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      persistent_settings_(persistent_settings),
      event_dispatcher_(event_dispatcher) {
  DCHECK(event_dispatcher_);
  on_metric_upload_event_callback_ =
      base::Bind(&H5vccMetrics::OnMetricUploadEvent, base::Unretained(this));
  event_dispatcher_->AddEventCallback(base::OnMetricUploadEvent::TypeId(),
                                      on_metric_upload_event_callback_);
}

H5vccMetrics::~H5vccMetrics() {
  event_dispatcher_->RemoveEventCallback(base::OnMetricUploadEvent::TypeId(),
                                         on_metric_upload_event_callback_);
}

void H5vccMetrics::OnMetricUploadEvent(const base::Event* event) {
  std::unique_ptr<base::OnMetricUploadEvent> on_metric_upload_event(
      new base::OnMetricUploadEvent(event));
  RunEventHandler(on_metric_upload_event->metric_type(),
                  on_metric_upload_event->serialized_proto());
}

void H5vccMetrics::OnMetricEvent(
    const h5vcc::MetricEventHandlerWrapper::ScriptValue& event_handler) {
  uploader_callback_ =
      new h5vcc::MetricEventHandlerWrapper(this, event_handler);
}

void H5vccMetrics::RunEventHandler(
    const cobalt::h5vcc::H5vccMetricType& metric_type,
    const std::string& serialized_proto) {
  if (task_runner_ && task_runner_->HasAtLeastOneRef()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&H5vccMetrics::RunEventHandlerInternal,
                   base::Unretained(this), metric_type, serialized_proto));
  }
}

void H5vccMetrics::RunEventHandlerInternal(
    const cobalt::h5vcc::H5vccMetricType& metric_type,
    const std::string& serialized_proto) {
  if (uploader_callback_ != nullptr && uploader_callback_->HasAtLeastOneRef()) {
    uploader_callback_->callback.value().Run(metric_type, serialized_proto);
  }
}

script::HandlePromiseVoid H5vccMetrics::Enable(
    script::EnvironmentSettings* environment_settings) {
  auto* global_wrappable = web::get_global_wrappable(environment_settings);
  script::HandlePromiseVoid promise =
      web::get_script_value_factory(environment_settings)
          ->CreateBasicPromise<void>();
  auto promise_reference =
      std::make_unique<script::ValuePromiseVoid::Reference>(global_wrappable,
                                                            promise);
  ToggleMetricsEnabled(
      true,
      base::BindOnce(
          [](std::unique_ptr<script::ValuePromiseVoid::Reference>
                 promise_reference) { promise_reference->value().Resolve(); },
          std::move(promise_reference)));
  return promise;
}

script::HandlePromiseVoid H5vccMetrics::Disable(
    script::EnvironmentSettings* environment_settings) {
  auto* global_wrappable = web::get_global_wrappable(environment_settings);
  script::HandlePromiseVoid promise =
      web::get_script_value_factory(environment_settings)
          ->CreateBasicPromise<void>();
  auto promise_reference =
      std::make_unique<script::ValuePromiseVoid::Reference>(global_wrappable,
                                                            promise);
  ToggleMetricsEnabled(
      false,
      base::BindOnce(
          [](std::unique_ptr<script::ValuePromiseVoid::Reference>
                 promise_reference) { promise_reference->value().Resolve(); },
          std::move(promise_reference)));
  return promise;
}

void H5vccMetrics::ToggleMetricsEnabled(bool is_enabled,
                                        base::OnceClosure done_callback) {
  persistent_settings_->Set(browser::metrics::kMetricEnabledSettingName,
                            base::Value(is_enabled));
  browser::metrics::CobaltMetricsServicesManager::GetInstance()
      ->ToggleMetricsEnabled(is_enabled, std::move(done_callback));
}

bool H5vccMetrics::IsEnabled() {
  return browser::metrics::CobaltMetricsServicesManager::GetInstance()
      ->IsMetricsReportingEnabled();
}

void H5vccMetrics::SetMetricEventInterval(uint32_t interval_seconds) {
  persistent_settings_->Set(browser::metrics::kMetricEventIntervalSettingName,
                            base::Value(static_cast<int>(interval_seconds)));
  browser::metrics::CobaltMetricsServicesManager::GetInstance()
      ->SetUploadInterval(interval_seconds);
}

}  // namespace h5vcc
}  // namespace cobalt
