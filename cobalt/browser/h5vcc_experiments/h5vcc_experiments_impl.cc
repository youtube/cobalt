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

#include "cobalt/browser/h5vcc_experiments/h5vcc_experiments_impl.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace h5vcc_experiments {

namespace {

h5vcc_experiments::mojom::OverrideState GetFeatureInternal(
    const std::string& feature_name) {
  auto* accessor = cobalt::GlobalFeatures::GetInstance()->accessor();
  base::FeatureList::OverrideState override =
      accessor->GetOverrideStateByFeatureName(feature_name);
  if (override == base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE) {
    return h5vcc_experiments::mojom::OverrideState::OVERRIDE_ENABLE_FEATURE;
  } else if (override ==
             base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE) {
    return h5vcc_experiments::mojom::OverrideState::OVERRIDE_DISABLE_FEATURE;
  }
  return h5vcc_experiments::mojom::OverrideState::OVERRIDE_USE_DEFAULT;
}

}  // namespace

// static
void H5vccExperimentsImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccExperiments> receiver) {
  new H5vccExperimentsImpl(*render_frame_host, std::move(receiver));
}

H5vccExperimentsImpl::H5vccExperimentsImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccExperiments> receiver)
    : content::DocumentService<mojom::H5vccExperiments>(render_frame_host,
                                                        std::move(receiver)) {}

void H5vccExperimentsImpl::SetExperimentState(
    base::Value::Dict experiment_config,
    SetExperimentStateCallback callback) {
  auto* global_features = cobalt::GlobalFeatures::GetInstance();
  auto* experiment_config_ptr = global_features->experiment_config();
  // A valid experiment config is supplied by h5vcc and we store the current
  // config as the safe config.
  global_features->experiment_config_manager()->StoreSafeConfig();
  // Note: It's important to clear the crash streak. Crashes that occur after a
  // successful config fetch do not prevent updating to a new update, and
  // therefore do not necessitate falling back to a safe config.
  global_features->metrics_local_state()->SetInteger(
      variations::prefs::kVariationsCrashStreak, 0);
  // At this point, have set the crash streak to 0 in the metrics_local_state
  // file. Do the same for the Variations beacon file to keep them in sync by
  // calling WriteBeaconValue(true). This eventually leads to the read
  // of the 0 value we just set in metrics_local_state and updates the
  // beacon file accordingly.
  //
  // WriteBeaconValue(true) also means we're saying the current session
  // exited cleanly, or that we're in a "clean state". This solves an edge
  // case where we recieve a new config, reset kVariationsCrashStreak to 0
  // here, and then crash in the same session. Without WriteBeaconValue(true),
  // we increment the crash streak from 0 to 1 on next startup, indicating
  // that the crash is associated with the newly recieved and applied config.
  // We should maintain a crash streak of 0 after recieving and applying a
  // new config on next startup.
  global_features->metrics_services_manager_client()
      ->GetMetricsStateManager()
      ->clean_exit_beacon()
      ->WriteBeaconValue(true);

  experiment_config_ptr->SetInt64(variations::prefs::kVariationsLastFetchTime,
                                  base::Time::Now().ToInternalValue());
  experiment_config_ptr->SetDict(
      cobalt::kExperimentConfigFeatures,
      std::move(experiment_config.Find(cobalt::kExperimentConfigFeatures)
                    ->GetDict()));
  experiment_config_ptr->SetDict(
      cobalt::kExperimentConfigFeatureParams,
      std::move(experiment_config.Find(cobalt::kExperimentConfigFeatureParams)
                    ->GetDict()));
  experiment_config_ptr->SetString(
      cobalt::kExperimentConfigActiveConfigData,
      std::move(
          experiment_config.Find(cobalt::kExperimentConfigActiveConfigData)
              ->GetString()));
  experiment_config_ptr->SetString(
      cobalt::kLatestConfigHash,
      std::move(
          experiment_config.Find(cobalt::kLatestConfigHash)->GetString()));
  // TODO: b/442825834 - Remove CommitPendingWrite to decrease storage writes
  // TODO: b/456583508 - Without CommitPendingWrite, we should still write to
  // storage if we shutdown early
  global_features->metrics_local_state()->CommitPendingWrite();
  experiment_config_ptr->CommitPendingWrite();
  std::move(callback).Run();
}

void H5vccExperimentsImpl::ResetExperimentState(
    ResetExperimentStateCallback callback) {
  PrefService* experiment_config =
      cobalt::GlobalFeatures::GetInstance()->experiment_config();
  experiment_config->ClearPref(cobalt::kExperimentConfig);
  // TODO: b/442825834 - Remove CommitPendingWrite to decrease storage writes
  // TODO: b/456583508 - Without CommitPendingWrite, we should still write to
  // storage if we shutdown early
  experiment_config->CommitPendingWrite();
  std::move(callback).Run();
}

void H5vccExperimentsImpl::GetActiveExperimentConfigData(
    GetActiveExperimentConfigDataCallback callback) {
  std::move(callback).Run(
      cobalt::GlobalFeatures::GetInstance()->active_config_data());
}

void H5vccExperimentsImpl::GetFeature(const std::string& feature_name,
                                      GetFeatureCallback callback) {
  std::move(callback).Run(GetFeatureInternal(feature_name));
}

void H5vccExperimentsImpl::GetLatestExperimentConfigHashData(
    GetLatestExperimentConfigHashDataCallback callback) {
  std::move(callback).Run(
      cobalt::GlobalFeatures::GetInstance()->latest_config_hash());
}

void H5vccExperimentsImpl::SetLatestExperimentConfigHashData(
    const std::string& hash_data,
    SetLatestExperimentConfigHashDataCallback callback) {
  cobalt::GlobalFeatures::GetInstance()->experiment_config()->SetString(
      cobalt::kLatestConfigHash, hash_data);
  // CommitPendingWrite not called here due to the same reason as above.
  std::move(callback).Run();
}

void H5vccExperimentsImpl::SetFinchParameters(
    base::Value::Dict settings,
    SetFinchParametersCallback callback) {
  cobalt::GlobalFeatures::GetInstance()->experiment_config()->SetDict(
      cobalt::kFinchParameters, std::move(settings));
  // CommitPendingWrite not called here due to the same reason as above.
  std::move(callback).Run();
}

}  // namespace h5vcc_experiments
