// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "starboard/extension/loader_app_metrics.h"

namespace cobalt {
namespace browser {

namespace {

void RecordLoaderAppTimeMetrics(
    const StarboardExtensionLoaderAppMetricsApi* metrics_extension) {
  int64_t elf_load_duration_us =
      metrics_extension->GetElfLoadDurationMicroseconds();
  int64_t elf_decompression_duration_us =
      metrics_extension->GetElfDecompressionDurationMicroseconds();

  if (elf_load_duration_us < 0) {
    return;
  }
  if (elf_decompression_duration_us < 0) {
    return;
  }
  if (elf_decompression_duration_us > elf_load_duration_us) {
    // The decompression duration is considered to be contained within the ELF
    // load duration, so this inequality is unexpected.
    return;
  }

  int64_t unexplained_duration_us =
      elf_load_duration_us - elf_decompression_duration_us;

  base::UmaHistogramTimes(
      "Cobalt.LoaderApp.ElfLoadDuration",
      base::TimeDelta::FromMicroseconds(elf_load_duration_us));
  base::UmaHistogramTimes(
      "Cobalt.LoaderApp.ElfDecompressionDuration",
      base::TimeDelta::FromMicroseconds(elf_decompression_duration_us));

  // "Unexplained" just means means that we haven't yet attempted to break this
  // chunk of time into its components. It's included here to give context for
  // the other durations, as recommended by
  // https://chromium.googlesource.com/chromium/src/+/lkgr/docs/speed/diagnostic_metrics.md#summation-diagnostics.
  base::UmaHistogramTimes(
      "Cobalt.LoaderApp.ElfLoadUnexplainedDuration",
      base::TimeDelta::FromMicroseconds(unexplained_duration_us));

  LOG(INFO) << "Recorded samples for Cobalt.LoaderApp duration metrics";
}

void RecordLoaderAppSpaceMetrics(
    const StarboardExtensionLoaderAppMetricsApi* metrics_extension) {
  if (metrics_extension->GetMaxSampledUsedCpuBytesDuringElfLoad() < 0) {
    return;
  }

  base::UmaHistogramMemoryMB(
      "Cobalt.LoaderApp.MaxSampledUsedCPUMemoryDuringELFLoad",
      metrics_extension->GetMaxSampledUsedCpuBytesDuringElfLoad() / 1000000);
  LOG(INFO) << "Recorded sample for "
            << "Cobalt.LoaderApp.MaxSampledUsedCPUMemoryDuringELFLoad";
}

}  // namespace

void RecordLoaderAppMetrics(
    const StarboardExtensionLoaderAppMetricsApi* metrics_extension) {
  if (metrics_extension &&
      strcmp(metrics_extension->name,
             kStarboardExtensionLoaderAppMetricsName) == 0) {
    if (metrics_extension->version >= 1) {
      base::UmaHistogramEnumeration(
          "Cobalt.LoaderApp.CrashpadInstallationStatus",
          metrics_extension->GetCrashpadInstallationStatus());
      LOG(INFO) << "Recorded sample for "
                << "Cobalt.LoaderApp.CrashpadInstallationStatus";
    }
    if (metrics_extension->version >= 2) {
      if (!metrics_extension->GetElfLibraryStoredCompressed()) {
        // We're only interested in load performance when the ELF library is
        // stored compressed and must therefore be decompressed at load time.
        return;
      }

      RecordLoaderAppTimeMetrics(metrics_extension);
      RecordLoaderAppSpaceMetrics(metrics_extension);
    }
  }
}

}  // namespace browser
}  // namespace cobalt
