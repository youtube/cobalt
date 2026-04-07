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

#include <string>

#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer.h"

namespace cobalt {
namespace browser {
namespace metrics {

class CobaltDetailedMetricsDelegate
    : public memory_instrumentation::SmapsCategorizer::Delegate {
 public:
  CobaltDetailedMetricsDelegate() = default;
  ~CobaltDetailedMetricsDelegate() override = default;

  memory_instrumentation::mojom::CobaltMemoryCategory GetCategory(
      const std::string& mapped_file) const override;
};

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_DETAILED_METRICS_DELEGATE_H_
