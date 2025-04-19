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

#ifndef COBALT_BROWSER_GLOBAL_FEATURES_H_
#define COBALT_BROWSER_GLOBAL_FEATURES_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"

class PrefService;

namespace metrics {
class MetricsService;
}  // namespace metrics

namespace metrics_services_manager {
class MetricsServicesManager;
}  // namespace metrics_services_manager

namespace cobalt {
class CobaltMetricsServicesManagerClient;

// This class owns features that are globally scoped. It follows the structure
// of BrowserProcess class in Chrome.
// TODO(b/407734389): Add unittests.
class GlobalFeatures {
 public:
  GlobalFeatures(
      std::unique_ptr<PrefService> experiment_config,
      std::unique_ptr<PrefService> local_state,
      std::unique_ptr<metrics_services_manager::MetricsServicesManager> manager,
      CobaltMetricsServicesManagerClient* client);

  ~GlobalFeatures();

  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager();
  metrics::MetricsService* metrics_service();
  PrefService* local_state();

 private:
  std::unique_ptr<PrefService> experiment_config_;

  std::unique_ptr<PrefService> local_state_;

  // |metrics_services_manager_| owns this.
  raw_ptr<CobaltMetricsServicesManagerClient, DanglingUntriaged>
      metrics_services_manager_client_ = nullptr;

  // Must be destroyed before |local_state_|.
  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_GLOBAL_FEATURES_H_
