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


#ifndef COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICES_MANAGER_H_
#define COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICES_MANAGER_H_

#include <memory>

#include "base//memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_uploader_callback.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"


namespace cobalt {
namespace browser {
namespace metrics {


// Persistent setting name for the metric event/upload interval.
constexpr char kMetricEventIntervalSettingName[] = "metricEventInterval";

// Persistent setting name for whether metrics are enabled or disabled.
constexpr char kMetricEnabledSettingName[] = "metricsEnabledState";

// A static wrapper around CobaltMetricsServicesManager. We need a way
// to provide a static instance of the "Cobaltified" MetricsServicesManager (and
// its public APIs) to control metrics behavior outside of //cobalt/browser
// (e.g., via H5vcc). Note, it's important that all public methods execute
// on the same thread in which CobaltMetricsServicesManager was constructed.
// This is a requirement of the metrics client code.
class CobaltMetricsServicesManager
    : public metrics_services_manager::MetricsServicesManager {
 public:
  CobaltMetricsServicesManager();

  // Static Singleton getter for metrics services manager.
  static CobaltMetricsServicesManager* GetInstance();

  // Destructs the static instance of CobaltMetricsServicesManager.
  static void DeleteInstance();

  // Sets the upload handler onto the current static instance of
  // CobaltMetricsServicesManager.
  static void SetOnUploadHandler(
      const CobaltMetricsUploaderCallback* uploader_callback);

  // Toggles whether metric reporting is enabled via
  // CobaltMetricsServicesManager.
  static void ToggleMetricsEnabled(bool is_enabled);

  // Sets the upload interval for metrics reporting. That is, how often are
  // metrics snapshotted and attempted to upload.
  static void SetUploadInterval(uint32_t interval_seconds);

 private:
  void SetOnUploadHandlerInternal(
      const CobaltMetricsUploaderCallback* uploader_callback);

  void ToggleMetricsEnabledInternal(bool is_enabled);

  void SetUploadIntervalInternal(uint32_t interval_seconds);

  static CobaltMetricsServicesManager* instance_;

  // The task runner of the thread this class was constructed on. All logic
  // interacting with containing metrics classes must be invoked on this
  // task_runner thread.
  scoped_refptr<base::SingleThreadTaskRunner> const task_runner_;
};

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICES_MANAGER_H_
