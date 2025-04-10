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

#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"

namespace cobalt {

GlobalFeatures* g_global_features = nullptr;

GlobalFeatures::GlobalFeatures(
    std::unique_ptr<PrefService> experiment_config,
    std::unique_ptr<PrefService> local_state,
    std::unique_ptr<metrics_services_manager::MetricsServicesManager>
        metrics_service_manager,
    CobaltMetricsServicesManagerClient* metrics_service_manager_client)
    : experiment_config_(std::move(experiment_config)),
      local_state_(std::move(local_state)),
      metrics_services_manager_(std::move(metrics_service_manager)),
      metrics_services_manager_client_(metrics_service_manager_client) {
  DCHECK(!g_global_features);
  g_global_features = this;
}

GlobalFeatures::~GlobalFeatures() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  g_global_features = nullptr;
}

metrics_services_manager::MetricsServicesManager*
GlobalFeatures::GetMetricsServicesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metrics_services_manager_) {
    auto client =
        std::make_unique<CobaltMetricsServicesManagerClient>(local_state());
    metrics_services_manager_client_ = client.get();
    metrics_services_manager_ =
        std::make_unique<metrics_services_manager::MetricsServicesManager>(
            std::move(client));
  }
  return metrics_services_manager_.get();
}

metrics::MetricsService* GlobalFeatures::metrics_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* metrics_services_manager = GetMetricsServicesManager();
  if (metrics_services_manager) {
    return metrics_services_manager->GetMetricsService();
  }
  return nullptr;
}

PrefService* GlobalFeatures::local_state() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_state_.get();
}

}  // namespace cobalt
