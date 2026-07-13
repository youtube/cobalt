// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_H5VCC_CORE_METRIC_COMPONENTS_H5VCC_CORE_METRIC_COMPONENTS_IMPL_H_
#define COBALT_BROWSER_H5VCC_CORE_METRIC_COMPONENTS_H5VCC_CORE_METRIC_COMPONENTS_IMPL_H_

#include <string>
#include <vector>

#include "cobalt/browser/h5vcc_core_metric_components/public/mojom/h5vcc_core_metric_components.mojom.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/common/cobalt_thread_checker.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace cobalt {
namespace browser {
struct StabilityReport;
}  // namespace browser
}  // namespace cobalt

namespace h5vcc_core_metric_components {

class H5vccCoreMetricComponentsImpl
    : public content::DocumentService<mojom::H5vccCoreMetricComponents> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::H5vccCoreMetricComponents> receiver);

  H5vccCoreMetricComponentsImpl(const H5vccCoreMetricComponentsImpl&) = delete;
  H5vccCoreMetricComponentsImpl& operator=(const H5vccCoreMetricComponentsImpl&) = delete;

  // Mojom impl:
  void GetPendingReports(GetPendingReportsCallback callback) override;
  void AcknowledgeReports(const std::vector<std::string>& acked_uuids,
                          AcknowledgeReportsCallback callback) override;
  void MarkHangRecovered(const std::string& cmc_join_uuid,
                         MarkHangRecoveredCallback callback) override;

 private:
  H5vccCoreMetricComponentsImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::H5vccCoreMetricComponents> receiver);

  void OnPendingReportsReceived(
      GetPendingReportsCallback callback,
      std::vector<cobalt::browser::StabilityReport> reports);

  COBALT_THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<H5vccCoreMetricComponentsImpl> weak_factory_{this};
};

}  // namespace h5vcc_core_metric_components

#endif  // COBALT_BROWSER_H5VCC_CORE_METRIC_COMPONENTS_H5VCC_CORE_METRIC_COMPONENTS_IMPL_H_
