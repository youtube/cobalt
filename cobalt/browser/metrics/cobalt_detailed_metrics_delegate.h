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

#ifndef COBALT_BROWSER_METRICS_COBALT_DETAILED_METRICS_DELEGATE_H_
#define COBALT_BROWSER_METRICS_COBALT_DETAILED_METRICS_DELEGATE_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace cobalt {

// Cobalt-specific implementation of the DetailedMetricsDelegate.
// Categorizes /proc/self/smaps mappings into Cobalt-specific categories.
class CobaltDetailedMetricsDelegate
    : public memory_instrumentation::DetailedMetricsDelegate {
 public:
  CobaltDetailedMetricsDelegate();
  ~CobaltDetailedMetricsDelegate() override;

  // DetailedMetricsDelegate implementation.
  void OnSmapsEntry(
      absl::string_view name,
      const memory_instrumentation::SmapsMetrics& metrics) override;
  void GetAndResetStats(base::flat_map<std::string, uint64_t>* stats) override;

  base::WeakPtr<memory_instrumentation::DetailedMetricsDelegate> GetWeakPtr()
      override;

 private:
  base::Lock lock_;
  base::flat_map<std::string, uint64_t> accumulated_stats_;

  base::WeakPtrFactory<CobaltDetailedMetricsDelegate> weak_ptr_factory_{this};
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_DETAILED_METRICS_DELEGATE_H_
