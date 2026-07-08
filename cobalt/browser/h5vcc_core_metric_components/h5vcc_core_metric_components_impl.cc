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

#include "cobalt/browser/h5vcc_core_metric_components/h5vcc_core_metric_components_impl.h"

#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "cobalt/browser/core_metric_components_manager.h"
#include "content/public/browser/render_frame_host.h"

namespace h5vcc_core_metric_components {

// static
void H5vccCoreMetricComponentsImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccCoreMetricComponents> receiver) {
  CHECK(render_frame_host);
  new H5vccCoreMetricComponentsImpl(*render_frame_host, std::move(receiver));
}

H5vccCoreMetricComponentsImpl::H5vccCoreMetricComponentsImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccCoreMetricComponents> receiver)
    : content::DocumentService<mojom::H5vccCoreMetricComponents>(
          render_frame_host,
          std::move(receiver)) {}

void H5vccCoreMetricComponentsImpl::GetPendingReports(
    GetPendingReportsCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  cobalt::browser::CoreMetricComponentsManager::GetInstance()
      ->GetPendingReportsForClient(base::BindOnce(
          &H5vccCoreMetricComponentsImpl::OnPendingReportsReceived,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void H5vccCoreMetricComponentsImpl::OnPendingReportsReceived(
    GetPendingReportsCallback callback,
    std::vector<cobalt::browser::StabilityReport> manager_reports) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<mojom::StabilityReportPtr> reports;
  for (const auto& r : manager_reports) {
    auto report = mojom::StabilityReport::New();
    report->cmc_join_uuid = r.cmc_join_uuid;
    report->creation_time = r.creation_time;

    switch (r.report_type) {
      case cobalt::browser::StabilityReportType::kCrash:
        report->report_type = mojom::StabilityReportType::kCrash;
        break;
      case cobalt::browser::StabilityReportType::kHangUnrecovered:
        report->report_type = mojom::StabilityReportType::kHangUnrecovered;
        break;
      case cobalt::browser::StabilityReportType::kHangRecovered:
        report->report_type = mojom::StabilityReportType::kHangRecovered;
        break;
    }

    reports.push_back(std::move(report));
  }

  std::move(callback).Run(std::move(reports));
}

void H5vccCoreMetricComponentsImpl::AcknowledgeReports(
    const std::vector<std::string>& acked_uuids,
    AcknowledgeReportsCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  cobalt::browser::CoreMetricComponentsManager::GetInstance()
      ->AcknowledgeReports(acked_uuids, std::move(callback));
}

void H5vccCoreMetricComponentsImpl::MarkHangRecovered(
    const std::string& cmc_join_uuid,
    MarkHangRecoveredCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  cobalt::browser::CoreMetricComponentsManager::GetInstance()
      ->RecordHangRecovered(cmc_join_uuid);

  std::move(callback).Run();
}

}  // namespace h5vcc_core_metric_components
