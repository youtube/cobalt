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

#include "cobalt/browser/h5vcc_metrics/histogram_fetcher.h"

#include "base/base64url.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/statistics_recorder.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/cobalt_uma_event.pb.h"

namespace h5vcc_metrics {

HistogramFetcher::HistogramFetcher() = default;
HistogramFetcher::~HistogramFetcher() = default;

std::string HistogramFetcher::FetchHistograms(
    metrics::MetricsStateManager* state_manager,
    metrics::MetricsServiceClient* service_client) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(state_manager);
  CHECK(service_client);

  // Synchronously fetch subprocess histograms that live in shared memory.
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  if (state_manager->client_id().empty()) {
    return std::string();
  }

  ::metrics::ChromeUserMetricsExtension uma_proto;
  metrics::MetricsLog log(state_manager->client_id(), /*session_id=*/1,
                          metrics::MetricsLog::LogType::ONGOING_LOG,
                          service_client);

  for (base::HistogramBase* const histogram :
       base::StatisticsRecorder::GetHistograms()) {
    // Take two snapshots back-to-back because there is no copy constructor
    // for histogram snapshots and the first snapshot used for the delta
    // calculation is done in place. The second becomes the baseline for the
    // next call. A small race window still exists between these two calls.
    auto samples = histogram->SnapshotSamples();
    auto new_baseline = histogram->SnapshotSamples();

    if (samples->TotalCount() == 0) {
      continue;
    }

    const uint64_t histogram_hash = histogram->name_hash();
    auto it = last_histogram_samples_.find(histogram_hash);

    if (it != last_histogram_samples_.end()) {
      samples->Subtract(*it->second);
    }

    // Store the full, unmodified snapshot for the next iteration.
    last_histogram_samples_[histogram_hash] = std::move(new_baseline);

    // `samples` now contains the delta if a previous snapshot existed, or the
    // full snapshot if it's the first time.
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
  return base64_encoded_proto;
}

}  // namespace h5vcc_metrics
