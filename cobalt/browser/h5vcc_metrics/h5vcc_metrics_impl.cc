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

#include "cobalt/browser/h5vcc_metrics/h5vcc_metrics_impl.h"

#include "base/base64url.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/cobalt_uma_event.pb.h"

namespace h5vcc_metrics {

// TODO (b/395126160): refactor mojom implementation on Android
H5vccMetricsImpl::H5vccMetricsImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccMetrics> receiver)
    : content::DocumentService<mojom::H5vccMetrics>(render_frame_host,
                                                    std::move(receiver)) {}

void H5vccMetricsImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccMetrics> receiver) {
  new H5vccMetricsImpl(*render_frame_host, std::move(receiver));
}

void H5vccMetricsImpl::AddListener(
    ::mojo::PendingRemote<mojom::MetricsListener> listener) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cobalt::GlobalFeatures::GetInstance()
      ->metrics_services_manager_client()
      ->metrics_service_client()
      ->SetMetricsListener(std::move(listener));
}

void H5vccMetricsImpl::Enable(bool enable, EnableCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto global_features = cobalt::GlobalFeatures::GetInstance();
  cobalt::CobaltEnabledStateProvider& enabled_state_provider =
      global_features->metrics_services_manager_client()
          ->GetEnabledStateProvider();
  enabled_state_provider.SetReportingEnabled(enable);
  global_features->metrics_services_manager()->UpdateUploadPermissions(true);
  std::move(callback).Run();
}

void H5vccMetricsImpl::SetMetricEventInterval(
    uint64_t interval_seconds,
    SetMetricEventIntervalCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cobalt::GlobalFeatures::GetInstance()
      ->metrics_services_manager_client()
      ->metrics_service_client()
      ->SetUploadInterval(base::Seconds(interval_seconds));
  std::move(callback).Run();
}

void H5vccMetricsImpl::RequestHistograms(RequestHistogramsCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* manager_client =
      cobalt::GlobalFeatures::GetInstance()->metrics_services_manager_client();
  std::move(callback).Run(histogram_fetcher_.FetchHistograms(
      manager_client->GetMetricsStateManager(),
      manager_client->metrics_service_client()));
}

}  // namespace h5vcc_metrics
