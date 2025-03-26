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

#ifndef COBALT_BROWSER_H5VCC_SYSTEM_H5VCC_SYSTEM_IMPL_H_
#define COBALT_BROWSER_H5VCC_SYSTEM_H5VCC_SYSTEM_IMPL_H_

#include <string>

#include "cobalt/browser/h5vcc_experiment/public/mojom/h5vcc_experiment.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_experiment {

// Implements the H5vccExperiment Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccExperimentImpl
    : public content::DocumentService<mojom::H5vccExperiment> {
 public:
  // Creates a H5vccExperimentImpl. The H5vccExperimentImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccExperiment> receiver);

  H5vccExperimentImpl(const H5vccExperimentImpl&) = delete;
  H5vccExperimentImpl& operator=(const H5vccExperimentImpl&) = delete;

  void SetFeatures(const std::string& experiment_config,
                   SetFeaturesCallback) override;
  void GetFeatureValue(const std::string& feature_name,
                       const std::string& feature_param_name,
                       GetFeatureValueCallback) override;
  void ResetExperimentState(ResetExperimentStateCallback) override;

 private:
  H5vccExperimentImpl(content::RenderFrameHost& render_frame_host,
                      mojo::PendingReceiver<mojom::H5vccExperiment> receiver);
};

}  // namespace h5vcc_experiment

#endif  // COBALT_BROWSER_H5VCC_SYSTEM_H5VCC_SYSTEM_IMPL_H_
