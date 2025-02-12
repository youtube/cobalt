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

namespace h5vcc_experiments {

namespace {

// !!!DELETE BEFORE MERGING THE PR!!!
BASE_FEATURE(kFinchTestFeature,
             "FinchTestFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kFinchTestFeatureParam{
    &kFinchTestFeature, "FinchTestFeatureParam", 0};

const char kCobaltExperimentName[] = "CobaltExperiment";

bool GetFeatureInternal(const std::string& feature_name) {
  std::string enable_features;
  std::string disable_features;
  auto feature_list = base::FeatureList::GetInstance();
  // feature_list->base::FeatureList::GetFeatureOverrides(&enable_features,
  //                                                      &disable_features);
  // base::FeatureList::OverrideState override =
  //     feature_list->base::FeatureList::GetOverrideStateByFeatureName(
  //         feature_name);
  // if (override == base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE)
  // {
  //   return true;
  // } else if (override ==
  //            base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE) {
  //   return false;
  // }

  for (const auto& enabled :
       base::FeatureList::SplitFeatureListString(enable_features)) {
    std::string feature;
    std::string study;
    std::string group;
    std::string feature_params;
    if (!base::FeatureList::ParseEnableFeatureString(enabled, &feature, &study,
                                                     &group, &feature_params)) {
      // Log errors?
      continue;
    }
    if (feature == feature_name) {
      return true;
    }
  }
  for (const auto& disabled :
       base::FeatureList::SplitFeatureListString(disable_features)) {
    std::string feature;
    std::string study;
    std::string group;
    std::string feature_params;
    if (!base::FeatureList::ParseEnableFeatureString(disabled, &feature, &study,
                                                     &group, &feature_params)) {
      // Log errors?
      continue;
    }
    if (feature == feature_name) {
      return false;
    }
  }

  // All features should be registered in the experiment config and therefore
  // be in one of the two overrides lists?
  return false;
}

std::string GetFeatureParamInternal(const std::string& feature_param_name) {
  return base::GetFieldTrialParamValue(kCobaltExperimentName,
                                       feature_param_name);
}

}  // namespace

// TODO (b/395126160): refactor mojom implementation on Android
H5vccExperimentsImpl::H5vccExperimentsImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccExperiments> receiver)
    : content::DocumentService<mojom::H5vccExperiments>(render_frame_host,
                                                        std::move(receiver)) {}

void H5vccExperimentsImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccExperiments> receiver) {
  new H5vccExperimentsImpl(*render_frame_host, std::move(receiver));
}

void H5vccExperimentsImpl::SetExperimentState(
    const std::string& experiment_config,
    SetExperimentStateCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void H5vccExperimentsImpl::GetFeature(const std::string& feature_name,
                                      GetFeatureCallback callback) {
  std::move(callback).Run(GetFeatureInternal(feature_name));
}

void H5vccExperimentsImpl::GetFeatureParam(
    const std::string& feature_param_name,
    GetFeatureParamCallback callback) {
  std::move(callback).Run(GetFeatureParamInternal(feature_param_name));
}

void H5vccExperimentsImpl::ResetExperimentState(
    ResetExperimentStateCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

}  // namespace h5vcc_experiments
