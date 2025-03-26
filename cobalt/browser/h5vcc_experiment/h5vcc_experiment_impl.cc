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

#include "cobalt/browser/h5vcc_experiment/h5vcc_experiment_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"

namespace h5vcc_experiment {

// TODO (b/395126160): refactor mojom implementation on Android
H5vccExperimentImpl::H5vccExperimentImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccExperiment> receiver)
    : content::DocumentService<mojom::H5vccExperiment>(render_frame_host,
                                                       std::move(receiver)) {}

void H5vccExperimentImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccExperiment> receiver) {
  new H5vccExperimentImpl(*render_frame_host, std::move(receiver));
}

void H5vccExperimentImpl::SetFeatures(const std::string& experiment_config,
                                      SetFeaturesCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void H5vccExperimentImpl::GetFeatureValue(const std::string& feature_name,
                                          const std::string& feature_param_name,
                                          GetFeatureValueCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run("");
}

void H5vccExperimentImpl::ResetExperimentState(
    ResetExperimentStateCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

}  // namespace h5vcc_experiment
