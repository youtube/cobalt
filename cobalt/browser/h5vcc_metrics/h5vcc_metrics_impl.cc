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
#include "base/threading/thread_checker.h"
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cobalt::GlobalFeatures::GetInstance()
      ->metrics_services_manager_client()
      ->metrics_service_client()
      ->SetMetricsListener(std::move(listener));
}

void H5vccMetricsImpl::Enable(bool enable, EnableCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto global_features = cobalt::GlobalFeatures::GetInstance();
  auto enabled_state_provider =
      global_features->metrics_services_manager_client()
          ->GetEnabledStateProvider();
  enabled_state_provider->SetReportingEnabled(enable);
  global_features->metrics_services_manager()->UpdateUploadPermissions(true);
  std::move(callback).Run();
}

void H5vccMetricsImpl::SetMetricEventInterval(
    uint64_t interval_seconds,
    SetMetricEventIntervalCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cobalt::GlobalFeatures::GetInstance()
      ->metrics_services_manager_client()
      ->metrics_service_client()
      ->SetUploadInterval(base::Seconds(interval_seconds));
  std::move(callback).Run();
}

void H5vccMetricsImpl::RequestHistograms(RequestHistogramsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Synchronously fetch subprocess histograms that live in shared memory.
  // This is the only mechanism available to embedders like Cobalt, as the
  // fully async HistogramSynchronizer is a content internal implementation.
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  auto* manager_client =
      cobalt::GlobalFeatures::GetInstance()->metrics_services_manager_client();
  auto* service_client = manager_client->metrics_service_client();
  auto* state_manager = manager_client->GetMetricsStateManager();
  CHECK(state_manager);

  if (state_manager->client_id().empty()) {
    std::move(callback).Run(std::string());
    return;
  }

  ::metrics::ChromeUserMetricsExtension uma_proto;
  // We create a temporary MetricsLog object to facilitate serializing
  // histograms. This class requires a session ID in its constructor, but the
  // value is not used for the purposes of this function. The session ID is
  // stored in the system profile, but this function only extracts the
  // histogram data, and the rest of the log (including the system profile) is
  // discarded. Therefore, a placeholder value of 1 is sufficient.
  metrics::MetricsLog log(state_manager->client_id(), /*session_id=*/1,
                          metrics::MetricsLog::LogType::ONGOING_LOG,
                          service_client);

  for (base::HistogramBase* const histogram :
       base::StatisticsRecorder::GetHistograms()) {
    auto samples = histogram->SnapshotSamples();
    if (samples->TotalCount() == 0) {
      continue;
    }

    auto it =
        last_histogram_samples_.find(std::string(histogram->histogram_name()));
    if (it != last_histogram_samples_.end()) {
      samples->Subtract(*it->second);
      it->second = histogram->SnapshotSamples();
    } else {
      last_histogram_samples_.emplace(std::string(histogram->histogram_name()),
                                      histogram->SnapshotSamples());
    }
    if (samples->TotalCount() > 0) {
      log.RecordHistogramDelta(histogram->histogram_name(), *samples);
    }
  }
  std::string encoded_log;
  log.FinalizeLog(false, service_client->GetVersionString(),
                  log.GetCurrentClockTime(false), &encoded_log);
  uma_proto.CopyFrom(*log.uma_proto());

  cobalt::browser::metrics::CobaltUMAEvent cobalt_proto;
  cobalt_proto.mutable_histogram_event()->CopyFrom(uma_proto.histogram_event());

  std::string serialized_proto;
  cobalt_proto.SerializeToString(&serialized_proto);

  std::string base64_encoded_proto;
  base::Base64UrlEncode(serialized_proto,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &base64_encoded_proto);
  std::move(callback).Run(base64_encoded_proto);
}

}  // namespace h5vcc_metrics
