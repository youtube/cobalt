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

#include "cobalt/browser/loader_app_metrics.h"

#include <cstring>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "starboard/extension/loader_app_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace {

constexpr char kElfLoadDuration[] = "Cobalt.LoaderApp.ElfLoadDuration";
constexpr char kElfDecompressionDuration[] =
    "Cobalt.LoaderApp.ElfDecompressionDuration";
constexpr char kElfLoadUnexplainedDuration[] =
    "Cobalt.LoaderApp.ElfLoadUnexplainedDuration";
constexpr char kMaxSampledUsedCpuMemoryDuringElfLoad[] =
    "Cobalt.LoaderApp.MaxSampledUsedCPUMemoryDuringELFLoad";
constexpr char kSlotSelectionStatus[] = "Cobalt.LoaderApp.SlotSelectionStatus";

class LoaderAppMetricsTest : public testing::Test {
 protected:
  void SetExtension(const StarboardExtensionLoaderAppMetricsApi* extension) {
    SetGetExtensionForTesting(base::BindRepeating(
        [](const StarboardExtensionLoaderAppMetricsApi* ext,
           const char* name) -> const void* {
          if (std::strcmp(name, kStarboardExtensionLoaderAppMetricsName) == 0) {
            return ext;
          }
          return nullptr;
        },
        extension));
  }

  void TearDown() override {
    SetGetExtensionForTesting(GetExtensionCallback());
  }

  base::HistogramTester histogram_tester_;
};

// The mutators need to be defined but their implementations aren't important:
// the code-under-test only uses the accessors and these are stubbed in the
// tests.
void SetCrashpadInstallationStatus(CrashpadInstallationStatus status) {}
void SetElfLibraryStoredCompressed(bool compressed) {}
void SetElfLoadDurationMicroseconds(int64_t microseconds) {}
void SetElfDecompressionDurationMicroseconds(int64_t microseconds) {}
void RecordUsedCpuBytesDuringElfLoad(int64_t bytes) {}
void SetSlotSelectionStatus(SlotSelectionStatus status) {}

TEST_F(LoaderAppMetricsTest, NoExtensionRecordsNoSamples) {
  SetExtension(nullptr);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectTotalCount(kSlotSelectionStatus, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest, ExtensionNameMismatchRecordsNoSamples) {
  StarboardExtensionLoaderAppMetricsApi stub_api = {
      "dev.cobalt.extension.WrongName",
      3,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return true; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 7'000; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; },
      &SetSlotSelectionStatus,
      []() { return SlotSelectionStatus::kCurrentSlot; },
  };

  // Provide the extension even when queried for
  // kStarboardExtensionLoaderAppMetricsName. This simulates a bug in a
  // platform's implementation of SbSystemGetExtension().
  SetGetExtensionForTesting(
      base::BindRepeating([](const StarboardExtensionLoaderAppMetricsApi* ext,
                             const char* name) -> const void* { return ext; },
                          &stub_api));

  RecordLoaderAppMetrics();

  histogram_tester_.ExpectTotalCount(kSlotSelectionStatus, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest, V1ExtensionVersionTooLowRecordsNoSamples) {
  StarboardExtensionLoaderAppMetricsApi stub_v1_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName, 1,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; }};

  SetExtension(&stub_v1_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectTotalCount(kSlotSelectionStatus, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest, V2ExtensionVersionTooLowForSlotSelectionSample) {
  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return false; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 7'000; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; }};

  SetExtension(&stub_v2_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectTotalCount(kSlotSelectionStatus, 0);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithUncompressedElfRecordsNoLoadPerformanceSamples) {
  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return false; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 7'000; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; }};

  SetExtension(&stub_v2_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithNegativeElfLoadDurationRecordsOnlySpaceSample) {
  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return true; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return -1; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 999; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; }};

  SetExtension(&stub_v2_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectUniqueSample(kMaxSampledUsedCpuMemoryDuringElfLoad,
                                       99, 1);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithNegativeDecompressionDurationSkipsDurationSubmetrics) {
  // Simulates libcobalt.zst where elf_load_duration is recorded, but
  // decompression duration is not yet provided (-1).
  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return true; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return -1; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; }};

  SetExtension(&stub_v2_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectUniqueSample(kElfLoadDuration, 10, 1);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectUniqueSample(kMaxSampledUsedCpuMemoryDuringElfLoad,
                                       99, 1);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithDecompressionGTLoadDurationSkipsDurationSubmetrics) {
  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return true; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 11'000; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; }};

  SetExtension(&stub_v2_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectUniqueSample(kElfLoadDuration, 10, 1);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectUniqueSample(kMaxSampledUsedCpuMemoryDuringElfLoad,
                                       99, 1);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithNegativeMaxSampledUsedCpuBytesRecordsOnlyTimeSamples) {
  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return true; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 7'000; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return -1; }};

  SetExtension(&stub_v2_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectUniqueSample(kElfLoadDuration, 10, 1);
  histogram_tester_.ExpectUniqueSample(kElfDecompressionDuration, 7, 1);
  histogram_tester_.ExpectUniqueSample(kElfLoadUnexplainedDuration, 3, 1);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

TEST_F(LoaderAppMetricsTest,
       V2ExtensionWithValidDataRecordsAllLoadPerformanceSamples) {
  StarboardExtensionLoaderAppMetricsApi stub_v2_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      2,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return true; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 7'000; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; }};

  SetExtension(&stub_v2_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectUniqueSample(kElfLoadDuration, 10, 1);
  histogram_tester_.ExpectUniqueSample(kElfDecompressionDuration, 7, 1);
  histogram_tester_.ExpectUniqueSample(kElfLoadUnexplainedDuration, 3, 1);
  histogram_tester_.ExpectUniqueSample(kMaxSampledUsedCpuMemoryDuringElfLoad,
                                       99, 1);
}

TEST_F(LoaderAppMetricsTest,
       V3ExtensionRecordsSlotSelectionStatusAndPerformanceMetrics) {
  StarboardExtensionLoaderAppMetricsApi stub_v3_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      3,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return true; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 7'000; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; },
      &SetSlotSelectionStatus,
      []() { return SlotSelectionStatus::kCurrentSlot; }};

  SetExtension(&stub_v3_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectUniqueSample(kSlotSelectionStatus,
                                       SlotSelectionStatus::kCurrentSlot, 1);
  histogram_tester_.ExpectUniqueSample(kElfLoadDuration, 10, 1);
  histogram_tester_.ExpectUniqueSample(kElfDecompressionDuration, 7, 1);
  histogram_tester_.ExpectUniqueSample(kElfLoadUnexplainedDuration, 3, 1);
  histogram_tester_.ExpectUniqueSample(kMaxSampledUsedCpuMemoryDuringElfLoad,
                                       99, 1);
}

TEST_F(LoaderAppMetricsTest,
       V3ExtensionWithUncompressedElfRecordsSlotSelectionStatusOnly) {
  StarboardExtensionLoaderAppMetricsApi stub_v3_loader_app_metrics_api = {
      kStarboardExtensionLoaderAppMetricsName,
      3,
      &SetCrashpadInstallationStatus,
      []() { return CrashpadInstallationStatus::kUnknown; },
      &SetElfLibraryStoredCompressed,
      []() { return false; },
      &SetElfLoadDurationMicroseconds,
      []() -> int64_t { return 10'000; },
      &SetElfDecompressionDurationMicroseconds,
      []() -> int64_t { return 7'000; },
      &RecordUsedCpuBytesDuringElfLoad,
      []() -> int64_t { return 99'000'000; },
      &SetSlotSelectionStatus,
      []() { return SlotSelectionStatus::kRollForward; }};

  SetExtension(&stub_v3_loader_app_metrics_api);
  RecordLoaderAppMetrics();

  histogram_tester_.ExpectUniqueSample(kSlotSelectionStatus,
                                       SlotSelectionStatus::kRollForward, 1);
  histogram_tester_.ExpectTotalCount(kElfLoadDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfDecompressionDuration, 0);
  histogram_tester_.ExpectTotalCount(kElfLoadUnexplainedDuration, 0);
  histogram_tester_.ExpectTotalCount(kMaxSampledUsedCpuMemoryDuringElfLoad, 0);
}

}  // namespace
}  // namespace browser
}  // namespace cobalt
