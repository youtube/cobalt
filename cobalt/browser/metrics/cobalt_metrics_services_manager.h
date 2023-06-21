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

#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

namespace cobalt {
namespace browser {
namespace metrics {

// A static wrapper around Chromium's MetricsServicesManager. We need a way
// to provide a static instance of the "Cobaltified" MetricsServicesManager (and
// its public APIs) to control metrics behavior outside of //cobalt/browser
// (e.g., via H5vcc).
class CobaltMetricsServicesManager {
 public:
  // Static Singleton getter for metrics services manager.
  static metrics_services_manager::MetricsServicesManager* GetInstance() {
    const auto instance = new metrics_services_manager::MetricsServicesManager(
        std::make_unique<CobaltMetricsServicesManagerClient>());
    return instance;
  }

  // Destroy passed metrics service manager.
  static void DeleteInstance(metrics_services_manager::MetricsServicesManager*
                                 metrics_services_manager) {
    delete metrics_services_manager;
  }
};

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICES_MANAGER_H_
