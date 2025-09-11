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

#include "cobalt/browser/loader_app_metrics.h"

#include <cstring>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "starboard/extension/loader_app_metrics.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {

void RecordLoaderAppMetrics() {
  const auto* loader_app_metrics =
      static_cast<const StarboardExtensionLoaderAppMetricsApi*>(
          SbSystemGetExtension(kStarboardExtensionLoaderAppMetricsName));

  if (!loader_app_metrics) {
    LOG(WARNING) << "LoaderAppMetrics: Extension not found.";
    return;
  }

  if (strcmp(loader_app_metrics->name,
             kStarboardExtensionLoaderAppMetricsName) != 0) {
    LOG(ERROR) << "LoaderAppMetrics: Extension name mismatch.";
    return;
  }

  if (loader_app_metrics->version < 2) {
    LOG(WARNING) << "LoaderAppMetrics: Extension version too low ("
                 << loader_app_metrics->version << "). Need at least 2.";
    return;
  }

  LOG(INFO) << "LoaderAppMetrics: Extension found and version is OK.";

  if (loader_app_metrics->GetElfLibraryStoredCompressed()) {
    LOG(INFO) << "LoaderAppMetrics: ELF was stored compressed.";
    int64_t decompression_duration_us =
        loader_app_metrics->GetElfDecompressionDurationMicroseconds();
    if (decompression_duration_us >= 0) {
      base::UmaHistogramTimes("Cobalt.Loader.Decompression.Duration",
                              base::Microseconds(decompression_duration_us));
      LOG(INFO) << "LoaderAppMetrics: Logging ELF Decompression Duration: "
                << decompression_duration_us << " us";
    } else {
      LOG(WARNING) << "LoaderAppMetrics: Decompression duration is negative, "
                      "not logging.";
    }
  } else {
    LOG(INFO) << "LoaderAppMetrics: ELF was not stored compressed. Skipping "
                 "decompression metric.";
  }
}

}  // namespace browser
}  // namespace cobalt
