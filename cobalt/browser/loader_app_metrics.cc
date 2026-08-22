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

namespace {

const StarboardExtensionLoaderAppMetricsApi* GetLoaderAppMetricsExtension() {
  const auto* metrics_extension =
      static_cast<const StarboardExtensionLoaderAppMetricsApi*>(
          SbSystemGetExtension(kStarboardExtensionLoaderAppMetricsName));

  if (!metrics_extension) {
    LOG(WARNING) << "LoaderAppMetrics: Extension not found.";
    return nullptr;
  }

  if (strcmp(metrics_extension->name,
             kStarboardExtensionLoaderAppMetricsName) != 0) {
    LOG(ERROR) << "LoaderAppMetrics: Extension name mismatch.";
    return nullptr;
  }
  return metrics_extension;
}

void RecordLoaderAppTimeMetrics(
    const StarboardExtensionLoaderAppMetricsApi* metrics_extension) {
  int64_t elf_decompression_duration_us =
      metrics_extension->GetElfDecompressionDurationMicroseconds();

  if (elf_decompression_duration_us < 0) {
    LOG(ERROR) << "LoaderAppMetrics: Decompression duration is negative, "
                  "not logging.";
    return;
  }

  base::UmaHistogramTimes("Cobalt.LoaderApp.ElfDecompressionDuration",
                          base::Microseconds(elf_decompression_duration_us));
  LOG(INFO) << "LoaderAppMetrics: Logging ELF Decompression Duration: "
            << elf_decompression_duration_us << " us";
}

void RecordLoaderAppSpaceMetrics(
    const StarboardExtensionLoaderAppMetricsApi* metrics_extension) {
  int64_t max_sampled_used_cpu_bytes =
      metrics_extension->GetMaxSampledUsedCpuBytesDuringElfLoad();

  if (max_sampled_used_cpu_bytes < 0) {
    LOG(ERROR) << "LoaderAppMetrics: Max sampled used CPU bytes is negative, "
                  "not logging.";
    return;
  }

  base::UmaHistogramMemoryMB(
      "Cobalt.LoaderApp.MaxSampledUsedCPUMemoryDuringELFLoad",
      static_cast<int>(max_sampled_used_cpu_bytes / 1000000));
  LOG(INFO) << "LoaderAppMetrics: Logging Max Sampled Used CPU Memory During "
               "ELF Load: "
            << max_sampled_used_cpu_bytes << " bytes ("
            << (max_sampled_used_cpu_bytes / 1000000) << " MB)";
}

}  // namespace

void RecordLoaderAppMetrics() {
  const auto* metrics_extension = GetLoaderAppMetricsExtension();
  if (!metrics_extension) {
    return;
  }

  if (metrics_extension->version < 2) {
    LOG(WARNING) << "LoaderAppMetrics: Extension version too low ("
                 << metrics_extension->version << "). Need at least 2.";
    return;
  }

  if (metrics_extension->version >= 3) {
    base::UmaHistogramEnumeration("Cobalt.LoaderApp.SlotSelectionStatus",
                                  metrics_extension->GetSlotSelectionStatus());
  }

  if (!metrics_extension->GetElfLibraryStoredCompressed()) {
    LOG(INFO) << "LoaderAppMetrics: ELF was not stored compressed. Skipping "
                 "decompression metric.";
    return;
  }

  RecordLoaderAppTimeMetrics(metrics_extension);
  RecordLoaderAppSpaceMetrics(metrics_extension);
}

}  // namespace browser
}  // namespace cobalt
