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

#include "cobalt/browser/global_features.h"

#include "base/no_destructor.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

namespace cobalt {
GlobalFeatures::GlobalFeatures(
    std::unique_ptr<PrefService> experiment_config,
    std::unique_ptr<PrefService> local_state,
    std::unique_ptr<metrics_services_manager::MetricsServicesManager>
        metrics_services_manager,
    CobaltMetricsServicesManagerClient* metrics_services_manager_client)
    : experiment_config_(std::move(experiment_config)),
      local_state_(std::move(local_state)),
      metrics_services_manager_(std::move(metrics_services_manager)),
      metrics_services_manager_client_(metrics_services_manager_client) {}

GlobalFeatures::~GlobalFeatures() = default;

metrics_services_manager::MetricsServicesManager*
GlobalFeatures::GetMetricsServicesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/407734389): Add testing for DCHECK
  DCHECK(metrics_services_manager_);
  return metrics_services_manager_.get();
}

metrics::MetricsService* GlobalFeatures::metrics_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* metrics_services_manager = GetMetricsServicesManager();
  return metrics_services_manager->GetMetricsService();
}

PrefService* GlobalFeatures::local_state() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_state_.get();
}

}  // namespace cobalt
