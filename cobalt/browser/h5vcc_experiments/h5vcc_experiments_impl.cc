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

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"

namespace h5vcc_experiments {

namespace {

bool GetFeatureInternal(const std::string& feature_name) {
  return true;
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
  NOTIMPLEMENTED();
  std::move(callback).Run(GetFeatureInternal(feature_name));
}

void H5vccExperimentsImpl::GetFeatureParam(
    const std::string& feature_param_name,
    GetFeatureParamCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run("");
}

void H5vccExperimentsImpl::ResetExperimentState(
    ResetExperimentStateCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

}  // namespace h5vcc_experiments
