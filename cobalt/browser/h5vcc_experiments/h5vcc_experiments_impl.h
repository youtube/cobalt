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

#ifndef COBALT_BROWSER_H5VCC_EXPERIMENTS_H5VCC_EXPERIMENTS_IMPL_H_
#define COBALT_BROWSER_H5VCC_EXPERIMENTS_H5VCC_EXPERIMENTS_IMPL_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_experiments {

// Implements the H5vccExperiment Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccExperimentsImpl
    : public content::DocumentService<mojom::H5vccExperiments> {
 public:
  // Creates a H5vccExperimentImpl. The H5vccExperimentImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccExperiments> receiver);

  H5vccExperimentsImpl(const H5vccExperimentsImpl&) = delete;
  H5vccExperimentsImpl& operator=(const H5vccExperimentsImpl&) = delete;

  void SetExperimentState(base::Value::Dict,
                          SetExperimentStateCallback) override;
  void ResetExperimentState(ResetExperimentStateCallback) override;
  void GetActiveExperimentConfigData(
      GetActiveExperimentConfigDataCallback) override;
  void GetFeature(const std::string& feature_name, GetFeatureCallback) override;
  void GetLatestExperimentConfigHashData(
      GetLatestExperimentConfigHashDataCallback) override;
  void SetLatestExperimentConfigHashData(
      const std::string& hash_data,
      SetLatestExperimentConfigHashDataCallback) override;
  void SetFinchParameters(base::Value::Dict settings,
                          SetFinchParametersCallback) override;

 private:
  H5vccExperimentsImpl(content::RenderFrameHost& render_frame_host,
                       mojo::PendingReceiver<mojom::H5vccExperiments> receiver);
};

}  // namespace h5vcc_experiments

#endif  // COBALT_BROWSER_H5VCC_EXPERIMENTS_H5VCC_EXPERIMENTS_IMPL_H_
