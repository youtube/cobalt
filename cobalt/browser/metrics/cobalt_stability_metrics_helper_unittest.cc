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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/process_handle.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

constexpr char kExpectedAllocatorName[] = "BrowserStabilityMetrics";

class CobaltStabilityMetricsHelperTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& metrics_dir() const { return temp_dir_.GetPath(); }

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

TEST_F(CobaltStabilityMetricsHelperTest,
       ClearOtherStabilityMetricsPmaFiles_MergesAndDeletesFiles) {
  base::Time stamp = base::Time::FromTimeT(1700000000);
  base::ProcessId prior_pid = 1234;
  base::ProcessId current_pid = 5678;

  base::FilePath prior_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, prior_pid);

  // Create a PMA file containing a histogram sample using
  // GlobalHistogramAllocator.
  ASSERT_TRUE(base::GlobalHistogramAllocator::CreateWithFile(
      prior_file, 64 * 1024, 1, kExpectedAllocatorName));
  ASSERT_TRUE(base::GlobalHistogramAllocator::Get());
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      "Cobalt.Test.PriorSessionSample", 1, 100, 10,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  ASSERT_TRUE(histogram);
  histogram->Add(42);
  // Release the global allocator so the file is unmapped and can be operated
  // on.
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Create a corrupt PMA file.
  base::FilePath corrupt_pma = metrics_dir().AppendASCII("corrupt.pma");
  ASSERT_TRUE(base::WriteFile(corrupt_pma, "invalid pma data"));

  // Create a non-PMA file.
  base::FilePath non_pma = metrics_dir().AppendASCII("keep_me.txt");
  ASSERT_TRUE(base::WriteFile(non_pma, "leave untouched"));

  // Create a current PID file.
  base::FilePath current_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, current_pid);
  ASSERT_TRUE(base::WriteFile(current_file, "current"));

  ClearOtherStabilityMetricsPmaFiles(metrics_dir(), kExpectedAllocatorName,
                                     current_pid);

  // Verify prior PMA and corrupt PMA files are deleted.
  EXPECT_FALSE(base::PathExists(prior_file));
  EXPECT_FALSE(base::PathExists(corrupt_pma));
  EXPECT_FALSE(base::PathExists(current_file));

  // Verify non-PMA file is preserved.
  EXPECT_TRUE(base::PathExists(non_pma));

  // Verify the histogram sample from prior session was merged into
  // StatisticsRecorder.
  base::HistogramBase* merged =
      base::StatisticsRecorder::FindHistogram("Cobalt.Test.PriorSessionSample");
  ASSERT_TRUE(merged);
  std::unique_ptr<base::HistogramSamples> samples = merged->SnapshotSamples();
  EXPECT_EQ(samples->GetCount(42), 1);
}

TEST(CobaltProcessStateSummaryTest, SnapshotSerializationRoundtrip) {
  ProcessStateSnapshot original;
  original.pid = 12345;
  original.peak_rss_kb = 450 * 1024;
  original.peak_pmf_kb = 380 * 1024;
  original.peak_v8_code_kb = 64 * 1024;
  original.uptime_sec = 3600;
  original.last_trim_level = 15;
  original.flags = kFlagForeground | kFlagMediaPlaying;

  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::SerializeSnapshot(original);
  EXPECT_EQ(serialized.size(), kProcessStateSummaryPayloadSize);
  EXPECT_EQ(serialized.size(), 28u);

  std::optional<ProcessStateSnapshot> deserialized =
      CobaltProcessStateSummaryManager::DeserializeSnapshot(serialized);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(*deserialized, original);
  EXPECT_EQ(deserialized->pid, 12345u);
  EXPECT_EQ(deserialized->uptime_minutes(), 60u);
}

TEST(CobaltProcessStateSummaryTest, RejectsTruncatedAndCorruptPayloads) {
  // Truncated payload (< 28 bytes).
  std::vector<uint8_t> short_buffer(27, 0);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(short_buffer)
          .has_value());

  // Oversized payload (> 28 bytes).
  std::vector<uint8_t> long_buffer(29, 0);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(long_buffer)
          .has_value());

  ProcessStateSnapshot original;
  original.pid = 54321;
  original.peak_rss_kb = 100 * 1024;
  original.peak_pmf_kb = 80 * 1024;
  std::vector<uint8_t> valid_buffer =
      CobaltProcessStateSummaryManager::SerializeSnapshot(original);
  ASSERT_EQ(valid_buffer.size(), 28u);

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

  // Corrupt checksum (tamper with checksum bytes).
  std::vector<uint8_t> corrupt_checksum = valid_buffer;
  corrupt_checksum[24] ^= 0xFF;
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(corrupt_checksum)
          .has_value());

  // Corrupt payload body (tamper with a byte in [0..23], causing checksum
  // mismatch).
  std::vector<uint8_t> corrupt_body = valid_buffer;
  corrupt_body[10] ^= 0x01;
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(corrupt_body)
          .has_value());

  // Anti-garble: All-zero memory fields (peak_rss == 0 && peak_pmf == 0).
  ProcessStateSnapshot zero_mem;
  zero_mem.pid = 123;
  zero_mem.peak_rss_kb = 0;
  zero_mem.peak_pmf_kb = 0;
  std::vector<uint8_t> zero_mem_buffer =
      CobaltProcessStateSummaryManager::SerializeSnapshot(zero_mem);
  EXPECT_TRUE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(zero_mem_buffer)
          .has_value());

  // Anti-garble: Uninitialized / 0xFFFFFFFF memory fields.
  ProcessStateSnapshot uninit_mem;
  uninit_mem.pid = 123;
  uninit_mem.peak_rss_kb = 0xFFFFFFFF;
  uninit_mem.peak_pmf_kb = 100 * 1024;
  std::vector<uint8_t> uninit_mem_buffer =
      CobaltProcessStateSummaryManager::SerializeSnapshot(uninit_mem);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(uninit_mem_buffer)
          .has_value());

  // Anti-garble: Implausible memory (> 64 GB).
  ProcessStateSnapshot giant_mem;
  giant_mem.pid = 123;
  giant_mem.peak_rss_kb = 70 * 1024 * 1024;  // 70 GB
  giant_mem.peak_pmf_kb = 100 * 1024;
  std::vector<uint8_t> giant_mem_buffer =
      CobaltProcessStateSummaryManager::SerializeSnapshot(giant_mem);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(giant_mem_buffer)
          .has_value());

  // Anti-garble: Implausible uptime (> 180 days).
  ProcessStateSnapshot giant_uptime = original;
  giant_uptime.uptime_sec = 200 * 86400;
  std::vector<uint8_t> giant_uptime_buffer =
      CobaltProcessStateSummaryManager::SerializeSnapshot(giant_uptime);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(giant_uptime_buffer)
          .has_value());

  // Anti-garble: Invalid trim level (> 100).
  ProcessStateSnapshot invalid_trim = original;
  invalid_trim.last_trim_level = 101;
  std::vector<uint8_t> invalid_trim_buffer =
      CobaltProcessStateSummaryManager::SerializeSnapshot(invalid_trim);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(invalid_trim_buffer)
          .has_value());

  // Anti-garble: Unknown flag bits.
  ProcessStateSnapshot unknown_flags = original;
  unknown_flags.flags = 0x80;  // Bit 7 is unknown
  std::vector<uint8_t> unknown_flags_buffer =
      CobaltProcessStateSummaryManager::SerializeSnapshot(unknown_flags);
  EXPECT_FALSE(CobaltProcessStateSummaryManager::DeserializeSnapshot(
                   unknown_flags_buffer)
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
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.LastTrimLevel.AllExits", 80, 1);
}

}  // namespace
}  // namespace cobalt
