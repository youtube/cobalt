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

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/prefs/pref_registry_simple.h"

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
// of BrowserProcess class in Chrome. This class is a singleton and can only be
// accessed on a single SequenceTaskRunner.
// TODO(b/407734389): Add unittests.
class GlobalFeatures {
 public:
  ~GlobalFeatures() = default;

  static GlobalFeatures* GetInstance();

  // Registers experiment config prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  base::FeatureList::Accessor* accessor() { return accessor_.get(); }
  metrics_services_manager::MetricsServicesManager* metrics_services_manager();
  metrics::MetricsService* metrics_service();
  CobaltMetricsServicesManagerClient* metrics_services_manager_client();
  PrefService* experiment_config();
  PrefService* metrics_local_state();
  PrefService* local_state();
  const std::vector<uint32_t>& active_experiment_ids() {
    return active_experiment_ids_;
  }

  void set_accessor(std::unique_ptr<base::FeatureList::Accessor> accessor);

 private:
  friend class base::NoDestructor<GlobalFeatures>;

  GlobalFeatures();

  // Initialize a PrefService instance for the experiment config.
  void CreateExperimentConfig();
  // Initialize CobaltMetricsServicesManagerClient instance and use it to
  // initialize MetricsServicesManager.
  void CreateMetricsServices();
  // Initialize a PrefService instance for local state for Metrics services.
  void CreateMetricsLocalState();
  // Initialize a PrefService instance for local state.
  void CreateLocalState();
  // Record the active experiments ids in the member variable.
  void InitializeActiveExperimentIds();

  std::unique_ptr<base::FeatureList::Accessor> accessor_;

  // Finch config/state.
  std::unique_ptr<PrefService> experiment_config_;

  // UMA config/state.
  std::unique_ptr<PrefService> metrics_local_state_;

  // |metrics_services_manager_| owns this.
  raw_ptr<CobaltMetricsServicesManagerClient, DanglingUntriaged>
      metrics_services_manager_client_;

  // Must be destroyed before |metrics_local_state_|.
  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  std::vector<uint32_t> active_experiment_ids_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_GLOBAL_FEATURES_H_
