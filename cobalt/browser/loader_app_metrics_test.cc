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

#include "base/test/metrics/histogram_tester.h"
#include "starboard/extension/loader_app_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace {

constexpr char kCrashpadInstallationStatus[] =
    "Cobalt.LoaderApp.CrashpadInstallationStatus";
constexpr char kElfLoadDuration[] = "Cobalt.LoaderApp.ElfLoadDuration";
constexpr char kElfDecompressionDuration[] =
    "Cobalt.LoaderApp.ElfDecompressionDuration";
constexpr char kElfLoadUnexplainedDuration[] =
    "Cobalt.LoaderApp.ElfLoadUnexplainedDuration";
constexpr char kMaxSampledUsedCpuMemoryDuringElfLoad[] =
    "Cobalt.LoaderApp.MaxSampledUsedCPUMemoryDuringELFLoad";

class LoaderAppMetricsTest : public testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
};

// The mutators need to be defined but their implementions aren't important: the
// code-under-test only uses the accessors and these are stubbed in the tests.
void SetCrashpadInstallationStatus(CrashpadInstallationStatus status) {}
void SetElfLibraryStoredCompressed(bool compressed) {}
void SetElfLoadDurationMicroseconds(int64_t microseconds) {}
void SetElfDecompressionDurationMicroseconds(int64_t microseconds) {}
void RecordUsedCpuBytesDuringElfLoad(int64_t microseconds) {}

TEST_F(LoaderAppMetricsTest, NoExtensionRecordsNoSamples) {
  RecordLoaderAppMetrics(nullptr);

  histogram_tester_.ExpectTotalCount(kCrashpadInstallationStatus, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest, V1ExtensionRecordsCrashpadInstallationSampleOnly) {
  StarboardExtensionLoaderAppMetricsApi stub_v1_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName, 1,
      &SetCrashpadInstallationStatus, []() {
        return CrashpadInstallationStatus::kFailedCrashpadHandlerBinaryNotFound;
      }};

  RecordLoaderAppMetrics(&stub_v1_loader_app_metrics_api);

  histogram_tester_.ExpectUniqueSample(
      kCrashpadInstallationStatus,
      CrashpadInstallationStatus::kFailedCrashpadHandlerBinaryNotFound, 1);

  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithUncompressedElfRecordsCrashpadInstallationSampleOnly) {
  auto get_crashpad_installation_status = []() {
    return CrashpadInstallationStatus::kSucceeded;
  };
  auto get_elf_library_stored_compressed = []() { return false; };
  auto get_elf_duration_microseconds = []() -> int64_t { return 0; };
  auto get_elf_decompression_duration_microseconds = []() -> int64_t {
    return 0;
  };
  auto get_max_sampled_used_cpu_bytes_during_elf_load = []() -> int64_t {
    return 0;
  };

  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      get_crashpad_installation_status,
      &SetElfLibraryStoredCompressed,
      get_elf_library_stored_compressed,
      &SetElfLoadDurationMicroseconds,
      get_elf_duration_microseconds,
      &SetElfDecompressionDurationMicroseconds,
      get_elf_decompression_duration_microseconds,
      &RecordUsedCpuBytesDuringElfLoad,
      get_max_sampled_used_cpu_bytes_during_elf_load};

  RecordLoaderAppMetrics(&stub_v2_loader_app_metrics_api);

  histogram_tester_.ExpectUniqueSample(
      kCrashpadInstallationStatus, CrashpadInstallationStatus::kSucceeded, 1);

  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithNegativeElfLoadDurationRecordsNoDurationSamples) {
  auto get_crashpad_installation_status = []() {
    return CrashpadInstallationStatus::kSucceeded;
  };
  auto get_elf_library_stored_compressed = []() { return true; };
  auto get_elf_duration_microseconds = []() -> int64_t { return -1; };
  auto get_elf_decompression_duration_microseconds = []() -> int64_t {
    return 999;
  };
  auto get_max_sampled_used_cpu_bytes_during_elf_load = []() -> int64_t {
    return 99;
  };

  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      get_crashpad_installation_status,
      &SetElfLibraryStoredCompressed,
      get_elf_library_stored_compressed,
      &SetElfLoadDurationMicroseconds,
      get_elf_duration_microseconds,
      &SetElfDecompressionDurationMicroseconds,
      get_elf_decompression_duration_microseconds,
      &RecordUsedCpuBytesDuringElfLoad,
      get_max_sampled_used_cpu_bytes_during_elf_load};

  RecordLoaderAppMetrics(&stub_v2_loader_app_metrics_api);

  histogram_tester_.ExpectTotalCount(kCrashpadInstallationStatus, 1);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 1);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithNegativeDecompressionDurationRecordsNoDurationSamples) {
  auto get_crashpad_installation_status = []() {
    return CrashpadInstallationStatus::kSucceeded;
  };
  auto get_elf_library_stored_compressed = []() { return true; };
  auto get_elf_duration_microseconds = []() -> int64_t { return 999; };
  auto get_elf_decompression_duration_microseconds = []() -> int64_t {
    return -1;
  };
  auto get_max_sampled_used_cpu_bytes_during_elf_load = []() -> int64_t {
    return 99;
  };

  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      get_crashpad_installation_status,
      &SetElfLibraryStoredCompressed,
      get_elf_library_stored_compressed,
      &SetElfLoadDurationMicroseconds,
      get_elf_duration_microseconds,
      &SetElfDecompressionDurationMicroseconds,
      get_elf_decompression_duration_microseconds,
      &RecordUsedCpuBytesDuringElfLoad,
      get_max_sampled_used_cpu_bytes_during_elf_load};

  RecordLoaderAppMetrics(&stub_v2_loader_app_metrics_api);

  histogram_tester_.ExpectTotalCount(kCrashpadInstallationStatus, 1);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 1);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithDecompressionGTLoadDurationRecordsNoDurationSamples) {
  auto get_crashpad_installation_status = []() {
    return CrashpadInstallationStatus::kSucceeded;
  };
  auto get_elf_library_stored_compressed = []() { return true; };
  auto get_elf_duration_microseconds = []() -> int64_t { return 999; };
  auto get_elf_decompression_duration_microseconds = []() -> int64_t {
    return 1000;
  };
  auto get_max_sampled_used_cpu_bytes_during_elf_load = []() -> int64_t {
    return 99;
  };

  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      get_crashpad_installation_status,
      &SetElfLibraryStoredCompressed,
      get_elf_library_stored_compressed,
      &SetElfLoadDurationMicroseconds,
      get_elf_duration_microseconds,
      &SetElfDecompressionDurationMicroseconds,
      get_elf_decompression_duration_microseconds,
      &RecordUsedCpuBytesDuringElfLoad,
      get_max_sampled_used_cpu_bytes_during_elf_load};

  RecordLoaderAppMetrics(&stub_v2_loader_app_metrics_api);

  histogram_tester_.ExpectTotalCount(kCrashpadInstallationStatus, 1);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 1);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithNegativeMaxSampledUsedCpuBytesDoesNotRecordThatSample) {
  auto get_crashpad_installation_status = []() {
    return CrashpadInstallationStatus::kSucceeded;
  };
  auto get_elf_library_stored_compressed = []() { return true; };
  auto get_elf_duration_microseconds = []() -> int64_t { return 999; };
  auto get_elf_decompression_duration_microseconds = []() -> int64_t {
    return 99;
  };
  auto get_max_sampled_used_cpu_bytes_during_elf_load = []() -> int64_t {
    return -1;
  };

  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      get_crashpad_installation_status,
      &SetElfLibraryStoredCompressed,
      get_elf_library_stored_compressed,
      &SetElfLoadDurationMicroseconds,
      get_elf_duration_microseconds,
      &SetElfDecompressionDurationMicroseconds,
      get_elf_decompression_duration_microseconds,
      &RecordUsedCpuBytesDuringElfLoad,
      get_max_sampled_used_cpu_bytes_during_elf_load};

  RecordLoaderAppMetrics(&stub_v2_loader_app_metrics_api);

  histogram_tester_.ExpectTotalCount(kCrashpadInstallationStatus, 1);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 1);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 1);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 1);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest, V2ExtensionWithValidDataRecordsAllSamples) {
  auto get_crashpad_installation_status = []() {
    return CrashpadInstallationStatus::kSucceeded;
  };
  auto get_elf_library_stored_compressed = []() { return true; };
  auto get_elf_duration_microseconds = []() -> int64_t { return 10'000; };
  auto get_elf_decompression_duration_microseconds = []() -> int64_t {
    return 7'000;
  };
  auto get_max_sampled_used_cpu_bytes_during_elf_load = []() -> int64_t {
    return 99'000'000;
  };

  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      get_crashpad_installation_status,
      &SetElfLibraryStoredCompressed,
      get_elf_library_stored_compressed,
      &SetElfLoadDurationMicroseconds,
      get_elf_duration_microseconds,
      &SetElfDecompressionDurationMicroseconds,
      get_elf_decompression_duration_microseconds,
      &RecordUsedCpuBytesDuringElfLoad,
      get_max_sampled_used_cpu_bytes_during_elf_load};

  RecordLoaderAppMetrics(&stub_v2_loader_app_metrics_api);

  histogram_tester_.ExpectUniqueSample(
      kCrashpadInstallationStatus, CrashpadInstallationStatus::kSucceeded, 1);

  // The duration histogram samples are in milliseconds.
  histogram_tester_.ExpectUniqueSample(kElfLoadDuration, 10, 1);
  histogram_tester_.ExpectUniqueSample(kElfDecompressionDuration, 7, 1);
  histogram_tester_.ExpectUniqueSample(kElfLoadUnexplainedDuration, 3, 1);
  histogram_tester_.ExpectUniqueSample(kMaxSampledUsedCpuMemoryDuringElfLoad,
                                       99, 1);  // The sample is in MB
}

}  // namespace
}  // namespace browser
}  // namespace cobalt
