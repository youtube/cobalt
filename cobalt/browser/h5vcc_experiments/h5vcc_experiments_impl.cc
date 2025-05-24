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
#include "build/build_config.h"
#include "cobalt/browser/cobalt_experiment_names.h"
#include "cobalt/browser/global_features.h"
#include "components/prefs/pref_service.h"

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

std::string GetFeatureParamInternal(const std::string& feature_param_name) {
  return base::GetFieldTrialParamValue(cobalt::kCobaltExperimentName,
                                       feature_param_name);
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
  auto* experiment_config_ptr =
      cobalt::GlobalFeatures::GetInstance()->experiment_config();

  experiment_config_ptr->SetDict(
      cobalt::kExperimentConfigFeatures,
      std::move(experiment_config.Find(cobalt::kExperimentConfigFeatures)
                    ->GetDict()));
  experiment_config_ptr->SetDict(
      cobalt::kExperimentConfigFeatureParams,
      std::move(experiment_config.Find(cobalt::kExperimentConfigFeatureParams)
                    ->GetDict()));
  experiment_config_ptr->SetList(
      cobalt::kExperimentConfigExpIds,
      std::move(
          experiment_config.Find(cobalt::kExperimentConfigExpIds)->GetList()));
  experiment_config_ptr->CommitPendingWrite();
  std::move(callback).Run();
}

void H5vccExperimentsImpl::ResetExperimentState(
    ResetExperimentStateCallback callback) {
  PrefService* experiment_config =
      cobalt::GlobalFeatures::GetInstance()->experiment_config();
  experiment_config->ClearPref(cobalt::kExperimentConfig);
  experiment_config->CommitPendingWrite();
  std::move(callback).Run();
}

void H5vccExperimentsImpl::GetActiveExperimentIds(
    GetActiveExperimentIdsCallback callback) {
  std::move(callback).Run(
      cobalt::GlobalFeatures::GetInstance()->active_experiment_ids());
}

void H5vccExperimentsImpl::GetFeature(const std::string& feature_name,
                                      GetFeatureCallback callback) {
  std::move(callback).Run(GetFeatureInternal(feature_name));
}

void H5vccExperimentsImpl::GetFeatureParam(
    const std::string& feature_param_name,
    GetFeatureParamCallback callback) {
  std::string param_value = base::GetFieldTrialParamValue(
      cobalt::kCobaltExperimentName, feature_param_name);
  std::move(callback).Run(param_value);
}

}  // namespace h5vcc_experiments
