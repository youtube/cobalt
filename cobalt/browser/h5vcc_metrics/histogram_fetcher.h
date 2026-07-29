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

#ifndef COBALT_BROWSER_H5VCC_METRICS_HISTOGRAM_FETCHER_H_
#define COBALT_BROWSER_H5VCC_METRICS_HISTOGRAM_FETCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/metrics/histogram_samples.h"
#include "cobalt/common/cobalt_thread_checker.h"

namespace metrics {
class MetricsServiceClient;
class MetricsStateManager;
}  // namespace metrics

namespace h5vcc_metrics {

// This class is responsible for fetching histograms and calculating deltas
// from the last time it was called.
class HistogramFetcher {
 public:
  HistogramFetcher();
  ~HistogramFetcher();

  // Fetches all histograms, calculates the delta since the last call, and
  // returns the result as a base64-encoded CobaltUMAEvent proto.
  std::string FetchHistograms(metrics::MetricsStateManager* state_manager,
                              metrics::MetricsServiceClient* service_client);

 private:
  // Stores the last snapshot of histogram samples.
  std::map<uint64_t, std::unique_ptr<base::HistogramSamples>>
      last_histogram_samples_;

  COBALT_THREAD_CHECKER(thread_checker_);
};

}  // namespace h5vcc_metrics

#endif  // COBALT_BROWSER_H5VCC_METRICS_HISTOGRAM_FETCHER_H_
