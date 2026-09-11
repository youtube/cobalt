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

#include "cobalt/browser/metrics/cobalt_stability_metrics_helper.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/process_handle.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

constexpr char kExpectedAllocatorName[] = "BrowserStabilityMetrics";

class CobaltStabilityMetricsHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_ANDROID)
    base::StatisticsRecorder::ForgetHistogramForTesting(
        kSystemExitReasonHistogram);
    ResetPriorSessionExitReasonsForTesting();
#endif
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
#if BUILDFLAG(IS_ANDROID)
    base::StatisticsRecorder::ForgetHistogramForTesting(
        kSystemExitReasonHistogram);
    ResetPriorSessionExitReasonsForTesting();
#endif
  }

  const base::FilePath& metrics_dir() const { return temp_dir_.GetPath(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(CobaltStabilityMetricsHelperTest,
       HandlesEmptyAndNonExistentDirectories) {
  base::FilePath non_existent = metrics_dir().AppendASCII("non_existent_dir");
  EXPECT_TRUE(ExtractPriorSessionPids(non_existent, kExpectedAllocatorName,
                                      /*current_pid=*/100)
                  .empty());

  base::FilePath empty_dir = metrics_dir().AppendASCII("empty_dir");
  ASSERT_TRUE(base::CreateDirectory(empty_dir));
  EXPECT_TRUE(ExtractPriorSessionPids(empty_dir, kExpectedAllocatorName,
                                      /*current_pid=*/100)
                  .empty());
}

TEST_F(CobaltStabilityMetricsHelperTest, ExtractsAndDeduplicatesPriorPids) {
  base::Time stamp1 = base::Time::FromTimeT(1700000000);
  base::Time stamp2 = base::Time::FromTimeT(1700000100);

  // Two files with the same PID 1234 but different timestamps.
  base::FilePath f1 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp1, 1234);
  base::FilePath f2 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp2, 1234);
  ASSERT_TRUE(base::WriteFile(f1, ""));
  ASSERT_TRUE(base::WriteFile(f2, ""));

  // One file with a distinct PID 5678.
  base::FilePath f3 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp1, 5678);
  ASSERT_TRUE(base::WriteFile(f3, ""));

  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  EXPECT_THAT(pids, ::testing::UnorderedElementsAre(1234, 5678));
}

TEST_F(CobaltStabilityMetricsHelperTest, FiltersOutCurrentProcessPid) {
  base::Time stamp = base::Time::FromTimeT(1700000000);
  base::ProcessId current_pid = 4321;

  base::FilePath current_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, current_pid);
  ASSERT_TRUE(base::WriteFile(current_file, ""));

  base::FilePath prior_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 1111);
  ASSERT_TRUE(base::WriteFile(prior_file, ""));

  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, current_pid);
  EXPECT_THAT(pids, ::testing::ElementsAre(1111));
}

TEST_F(CobaltStabilityMetricsHelperTest, RejectsMismatchedAllocatorNames) {
  base::Time stamp = base::Time::FromTimeT(1700000000);

  base::FilePath other_allocator_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), "OtherAllocator", stamp, 1234);
  ASSERT_TRUE(base::WriteFile(other_allocator_file, ""));

  base::FilePath expected_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 5678);
  ASSERT_TRUE(base::WriteFile(expected_file, ""));

  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  EXPECT_THAT(pids, ::testing::ElementsAre(5678));
}

TEST_F(CobaltStabilityMetricsHelperTest, IgnoresCorruptFilenamesAndSubdirs) {
  base::Time stamp = base::Time::FromTimeT(1700000000);

  // Non-.pma extension.
  ASSERT_TRUE(
      base::WriteFile(metrics_dir().AppendASCII("not_a_pma_file.txt"), ""));
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000-1234.tmp"),
      ""));

  // Malformed PMA filenames that fail ParseFilePath.
  ASSERT_TRUE(
      base::WriteFile(metrics_dir().AppendASCII("corrupt_name.pma"), ""));
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-invalidhex-1234.pma"),
      ""));
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000-nothex.pma"),
      ""));

  // Subdirectories should not be traversed or counted as files.
  ASSERT_TRUE(base::CreateDirectory(metrics_dir().AppendASCII("subdir.pma")));
  ASSERT_TRUE(base::CreateDirectory(
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 9876)));

  // Valid file.
  base::FilePath valid_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 7777);
  ASSERT_TRUE(base::WriteFile(valid_file, ""));

  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  EXPECT_THAT(pids, ::testing::ElementsAre(7777));
}

TEST_F(CobaltStabilityMetricsHelperTest, RejectsZeroAndNegativePids) {
  base::Time stamp = base::Time::FromTimeT(1700000000);

  // Zero PID.
  base::FilePath zero_pid_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 0);
  ASSERT_TRUE(base::WriteFile(zero_pid_file, ""));

  // Negative PID in filename.
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000--1.pma"),
      ""));

  // Valid PID.
  base::FilePath valid_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 8888);
  ASSERT_TRUE(base::WriteFile(valid_file, ""));

  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  EXPECT_THAT(pids, ::testing::ElementsAre(8888));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(CobaltStabilityMetricsHelperTest, WasPriorSessionLowMemoryKilled) {
  // Before any sample is recorded to the histogram, returns false.
  EXPECT_FALSE(WasPriorSessionLowMemoryKilled());

  // Record an unrelated histogram: WasPriorSessionLowMemoryKilled remains
  // false.
  base::UmaHistogramExactLinear("Unrelated.Histogram.Name",
                                kAndroidExitReasonLowMemory,
                                kAndroidExitReasonNumEntries);
  EXPECT_FALSE(WasPriorSessionLowMemoryKilled());

  // Record a non-LMK exit reason (Exit self).
  base::UmaHistogramExactLinear(kSystemExitReasonHistogram,
                                kAndroidExitReasonExitSelf,
                                kAndroidExitReasonNumEntries);
  EXPECT_FALSE(WasPriorSessionLowMemoryKilled());

  // Record an LMK exit reason (Low memory).
  base::UmaHistogramExactLinear(kSystemExitReasonHistogram,
                                kAndroidExitReasonLowMemory,
                                kAndroidExitReasonNumEntries);
  EXPECT_TRUE(WasPriorSessionLowMemoryKilled());

  // Additional samples (including subsequent non-LMK exit reasons) preserve
  // the positive LMK detection because at least one prior session was LMK'd.
  base::UmaHistogramExactLinear(kSystemExitReasonHistogram,
                                kAndroidExitReasonExitSelf,
                                kAndroidExitReasonNumEntries);
  base::UmaHistogramExactLinear(kSystemExitReasonHistogram,
                                kAndroidExitReasonLowMemory,
                                kAndroidExitReasonNumEntries);
  EXPECT_TRUE(WasPriorSessionLowMemoryKilled());
}

TEST_F(CobaltStabilityMetricsHelperTest,
       WasPriorSessionLowMemoryKilled_HistogramIsolation) {
  // Verifies that ForgetHistogramForTesting cleans up the histogram,
  // preventing inter-test state leakage.
  EXPECT_FALSE(WasPriorSessionLowMemoryKilled());
  base::UmaHistogramExactLinear(kSystemExitReasonHistogram,
                                kAndroidExitReasonLowMemory,
                                kAndroidExitReasonNumEntries);
  EXPECT_TRUE(WasPriorSessionLowMemoryKilled());
  base::StatisticsRecorder::ForgetHistogramForTesting(
      kSystemExitReasonHistogram);
  EXPECT_FALSE(WasPriorSessionLowMemoryKilled());
}

TEST_F(CobaltStabilityMetricsHelperTest,
       GetWasPriorSessionLowMemoryKilledAsync_ResolvesExpectedValue) {
  bool called = false;
  bool result = true;
  GetWasPriorSessionLowMemoryKilledAsync(
      base::BindLambdaForTesting([&](bool was_lmk) {
        called = true;
        result = was_lmk;
      }));
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_R) {
    EXPECT_FALSE(called);
    OnPriorSessionExitReasonsRecorded();
    EXPECT_FALSE(called);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(called);
    EXPECT_FALSE(result);
  } else {
    EXPECT_FALSE(called);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(called);
    EXPECT_FALSE(result);
  }

  base::UmaHistogramExactLinear(kSystemExitReasonHistogram,
                                kAndroidExitReasonLowMemory,
                                kAndroidExitReasonNumEntries);
  OnPriorSessionExitReasonsRecorded();
  task_environment_.RunUntilIdle();

  called = false;
  result = false;
  GetWasPriorSessionLowMemoryKilledAsync(
      base::BindLambdaForTesting([&](bool was_lmk) {
        called = true;
        result = was_lmk;
      }));
  // Even when exit reasons are already recorded, callback execution is posted
  // to the sequence rather than invoked synchronously on the caller's stack.
  EXPECT_FALSE(called);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_TRUE(result);
}

TEST_F(CobaltStabilityMetricsHelperTest,
       GetWasPriorSessionLowMemoryKilledAsync_MultipleConcurrentCallbacks) {
  int callback_count = 0;
  std::vector<bool> results;

  for (int i = 0; i < 3; ++i) {
    GetWasPriorSessionLowMemoryKilledAsync(
        base::BindLambdaForTesting([&](bool was_lmk) {
          ++callback_count;
          results.push_back(was_lmk);
        }));
  }

  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_R) {
    EXPECT_EQ(callback_count, 0);
    base::UmaHistogramExactLinear(kSystemExitReasonHistogram,
                                  kAndroidExitReasonLowMemory,
                                  kAndroidExitReasonNumEntries);
    OnPriorSessionExitReasonsRecorded();
    EXPECT_EQ(callback_count, 0);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(callback_count, 3);
    EXPECT_THAT(results, ::testing::ElementsAre(true, true, true));
  } else {
    EXPECT_EQ(callback_count, 0);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(callback_count, 3);
    EXPECT_THAT(results, ::testing::ElementsAre(false, false, false));
  }
}

TEST_F(CobaltStabilityMetricsHelperTest,
       GetWasPriorSessionLowMemoryKilledAsync_ReentrantCallback) {
  bool outer_called = false;
  bool inner_called = false;
  bool inner_result = false;

  GetWasPriorSessionLowMemoryKilledAsync(
      base::BindLambdaForTesting([&](bool was_lmk) {
        outer_called = true;
        GetWasPriorSessionLowMemoryKilledAsync(
            base::BindLambdaForTesting([&](bool nested_lmk) {
              inner_called = true;
              inner_result = nested_lmk;
            }));
        // While the outer callback runs, the nested call should be enqueued
        // on the task runner and not yet executed, preserving non-reentrant
        // dispatch.
        EXPECT_FALSE(inner_called);
      }));

  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_R) {
    EXPECT_FALSE(outer_called);
    base::UmaHistogramExactLinear(kSystemExitReasonHistogram,
                                  kAndroidExitReasonLowMemory,
                                  kAndroidExitReasonNumEntries);
    OnPriorSessionExitReasonsRecorded();
    EXPECT_FALSE(outer_called);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(outer_called);
    EXPECT_TRUE(inner_called);
    EXPECT_TRUE(inner_result);
  } else {
    EXPECT_FALSE(outer_called);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(outer_called);
    EXPECT_TRUE(inner_called);
    EXPECT_FALSE(inner_result);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST(CobaltProcessStateSummaryTest, SnapshotSerializationRoundtrip) {
  ProcessStateSnapshot original;
  original.peak_rss_kb = 450 * 1024;
  original.peak_pmf_kb = 380 * 1024;
  original.peak_v8_code_kb = 64 * 1024;
  original.uptime_sec = 3600;
  original.last_trim_level = 15;
  original.flags = kFlagForeground | kFlagMediaPlaying;

  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::SerializeSnapshot(original);
  EXPECT_EQ(serialized.size(), kProcessStateSummaryPayloadSize);
  EXPECT_EQ(serialized.size(), 20u);

  std::optional<ProcessStateSnapshot> deserialized =
      CobaltProcessStateSummaryManager::DeserializeSnapshot(serialized);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(*deserialized, original);
  EXPECT_EQ(deserialized->uptime_minutes(), 60u);
}

TEST(CobaltProcessStateSummaryTest, RejectsTruncatedAndCorruptPayloads) {
  // Truncated payload (< 20 bytes).
  std::vector<uint8_t> short_buffer(19, 0);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(short_buffer)
          .has_value());

  ProcessStateSnapshot original;
  original.peak_rss_kb = 100 * 1024;
  std::vector<uint8_t> valid_buffer =
      CobaltProcessStateSummaryManager::SerializeSnapshot(original);

  // Invalid magic byte.
  std::vector<uint8_t> corrupt_magic = valid_buffer;
  corrupt_magic[0] = 0xAA;
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(corrupt_magic)
          .has_value());

  // Unsupported version.
  std::vector<uint8_t> corrupt_version = valid_buffer;
  corrupt_version[1] = 2;
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(corrupt_version)
          .has_value());
}

TEST(CobaltProcessStateSummaryTest, ExitReasonToHistogramSuffixMapping) {
  EXPECT_EQ(ExitReasonToHistogramSuffix(0), "Anr");
  EXPECT_EQ(ExitReasonToHistogramSuffix(1), "Crash");
  EXPECT_EQ(ExitReasonToHistogramSuffix(2), "CrashNative");
  EXPECT_EQ(ExitReasonToHistogramSuffix(3), "DependencyDied");
  EXPECT_EQ(ExitReasonToHistogramSuffix(4), "ExcessiveResourceUsage");
  EXPECT_EQ(ExitReasonToHistogramSuffix(5), "ExitSelf");
  EXPECT_EQ(ExitReasonToHistogramSuffix(6), "InitializationFailure");
  EXPECT_EQ(ExitReasonToHistogramSuffix(7), "LowMemory");
  EXPECT_EQ(ExitReasonToHistogramSuffix(8), "Other");
  EXPECT_EQ(ExitReasonToHistogramSuffix(9), "PermissionChange");
  EXPECT_EQ(ExitReasonToHistogramSuffix(10), "Signaled");
  EXPECT_EQ(ExitReasonToHistogramSuffix(11), "Unknown");
  EXPECT_EQ(ExitReasonToHistogramSuffix(12), "UserRequested");
  EXPECT_EQ(ExitReasonToHistogramSuffix(13), "UserStopped");
  EXPECT_EQ(ExitReasonToHistogramSuffix(14), "ApiFailed");
  EXPECT_EQ(ExitReasonToHistogramSuffix(15), "Freezer");
  EXPECT_EQ(ExitReasonToHistogramSuffix(16), "PackageStateChange");
  EXPECT_EQ(ExitReasonToHistogramSuffix(17), "PackageUpdated");
  // Default / fallback for unrecognized code
  EXPECT_EQ(ExitReasonToHistogramSuffix(99), "Other");
  EXPECT_EQ(ExitReasonToHistogramSuffix(-1), "Other");
}

TEST(CobaltProcessStateSummaryTest,
     HistogramEmissionAllExitsAndReasonSpecific) {
  base::HistogramTester histogram_tester;

  ProcessStateSnapshot snapshot;
  snapshot.peak_rss_kb = 400 * 1024;     // 400 MB
  snapshot.peak_pmf_kb = 350 * 1024;     // 350 MB
  snapshot.peak_v8_code_kb = 60 * 1024;  // 60 MB
  snapshot.uptime_sec = 7200;            // 120 minutes
  snapshot.last_trim_level = 80;         // TRIM_MEMORY_COMPLETE
  snapshot.flags = kFlagForeground;

  // Test LowMemory (exit_reason = 7)
  EmitPriorSessionExitSummaryHistograms(7, snapshot);

  // Suffix-specific histograms
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.LowMemory", 400, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.LowMemory", 350, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.LowMemory", 60, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeMinutes.LowMemory", 120, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.LastTrimLevel.LowMemory", 80, 1);

  // AllExits baseline histograms
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.AllExits", 400, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.AllExits", 350, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.AllExits", 60, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeMinutes.AllExits", 120, 1);
}

}  // namespace
}  // namespace cobalt
