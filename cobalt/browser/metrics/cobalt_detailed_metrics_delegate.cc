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

#include "cobalt/browser/metrics/cobalt_detailed_metrics_delegate.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer_patterns.h"
#endif

namespace cobalt {
namespace browser {
namespace metrics {

memory_instrumentation::mojom::CobaltMemoryCategory
CobaltDetailedMetricsDelegate::GetCategory(
    const std::string& mapped_file) const {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return memory_instrumentation::GetMemoryCategoryForMappedFile(mapped_file);
#else
  return memory_instrumentation::mojom::CobaltMemoryCategory::kUnknown;
#endif
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
