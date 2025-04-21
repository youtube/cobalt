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
#include "base/path_service.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"

namespace cobalt {

const char kExperimentConfigFeature[] = "features";
const char kExperimentConfigFeatureParams[] = "feature_params";
const char kExperimentConfigExpIds[] = "exp_ids";

constexpr base::FilePath::CharType kExperimentConfigFilename[] =
    FILE_PATH_LITERAL("Experiment Config");

GlobalFeatures::GlobalFeatures() {
  CreateExperimentConfig();
  CreateLocalState();
  CreateMetricsServices();
}

// static
GlobalFeatures* GlobalFeatures::GetInstance() {
  static base::NoDestructor<GlobalFeatures> instance;
  return instance.get();
}

metrics_services_manager::MetricsServicesManager*
GlobalFeatures::metrics_services_manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/407734389): Add testing for DCHECK
  DCHECK(metrics_services_manager_);
  return metrics_services_manager_.get();
}

metrics::MetricsService* GlobalFeatures::metrics_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metrics_services_manager()->GetMetricsService();
}

PrefService* GlobalFeatures::experiment_config() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return experiment_config_.get();
}

PrefService* GlobalFeatures::local_state() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_state_.get();
}

void GlobalFeatures::CreateExperimentConfig() {
  DCHECK(!experiment_config_);
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  RegisterPrefs(pref_registry.get());

  base::FilePath path;
  CHECK(base::PathService::Get(content::SHELL_DIR_USER_DATA, &path));
  path = path.Append(kExperimentConfigFilename);

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(path));

  experiment_config_ = pref_service_factory.Create(pref_registry);
}

void GlobalFeatures::CreateMetricsServices() {
  DCHECK(local_state_) << "CreateLocalState() must have been called previously";
  auto client =
      std::make_unique<CobaltMetricsServicesManagerClient>(local_state_.get());
  metrics_services_manager_client_ = client.get();
  metrics_services_manager_ =
      std::make_unique<metrics_services_manager::MetricsServicesManager>(
          std::move(client));
}

void GlobalFeatures::CreateLocalState() {
  DCHECK(!local_state_);
  // No need to make `pref_registry` a member, `pref_service_` will keep a
  // reference to it.
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  PrefServiceFactory pref_service_factory;
  // TODO(b/397929564): Investigate using a Chrome's memory-mapped file store
  // instead of in-memory.
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<InMemoryPrefStore>());

  local_state_ = pref_service_factory.Create(std::move(pref_registry));
}

// static
void GlobalFeatures::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kExperimentConfigFeature);
  registry->RegisterDictionaryPref(kExperimentConfigFeatureParams);
  registry->RegisterListPref(kExperimentConfigExpIds);
  metrics::MetricsService::RegisterPrefs(registry);
}

}  // namespace cobalt
