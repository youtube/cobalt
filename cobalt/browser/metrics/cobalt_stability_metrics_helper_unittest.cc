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
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
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
  // Verify active PMA file for current PID is preserved.
  EXPECT_TRUE(base::PathExists(current_file));

  // Verify non-PMA file is preserved.
  EXPECT_TRUE(base::PathExists(non_pma));

  // Verify the histogram sample from prior session was merged into
  // StatisticsRecorder.
  base::HistogramBase* merged =
      base::StatisticsRecorder::FindHistogram("Cobalt.Test.PriorSessionSample");
  ASSERT_TRUE(merged);
  std::unique_ptr<base::HistogramSamples> samples = merged->SnapshotSamples();
  EXPECT_EQ(samples->GetCount(42), 1);

  // When called with kNullProcessId, current_file is also cleared.
  ClearOtherStabilityMetricsPmaFiles(metrics_dir(), kExpectedAllocatorName,
                                     base::kNullProcessId);
  EXPECT_FALSE(base::PathExists(current_file));
}

TEST_F(CobaltStabilityMetricsHelperTest,
       ClearOtherStabilityMetricsPmaFiles_HandlesEmptyAndNonExistentDirs) {
  base::FilePath non_existent = metrics_dir().AppendASCII("non_existent_dir");
  EXPECT_NO_FATAL_FAILURE(ClearOtherStabilityMetricsPmaFiles(
      non_existent, kExpectedAllocatorName, /*current_pid=*/100));

  base::FilePath empty_dir = metrics_dir().AppendASCII("empty_dir");
  ASSERT_TRUE(base::CreateDirectory(empty_dir));
  EXPECT_NO_FATAL_FAILURE(ClearOtherStabilityMetricsPmaFiles(
      empty_dir, kExpectedAllocatorName, /*current_pid=*/100));
}

TEST_F(CobaltStabilityMetricsHelperTest,
       ClearOtherStabilityMetricsPmaFiles_DeletesMismatchedAndInvalidPids) {
  base::Time stamp = base::Time::FromTimeT(1700000000);

  // Mismatched allocator name .pma file
  base::FilePath mismatched_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), "UnrelatedAllocator", stamp, 1234);
  ASSERT_TRUE(base::WriteFile(mismatched_file, "some data"));

  // Zero PID .pma file
  base::FilePath zero_pid_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 0);
  ASSERT_TRUE(base::WriteFile(zero_pid_file, "some data"));

  // Malformed / negative PID .pma file
  base::FilePath negative_pid_file =
      metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000--1.pma");
  ASSERT_TRUE(base::WriteFile(negative_pid_file, "some data"));

  ClearOtherStabilityMetricsPmaFiles(metrics_dir(), kExpectedAllocatorName,
                                     /*current_pid=*/5678);

  EXPECT_FALSE(base::PathExists(mismatched_file));
  EXPECT_FALSE(base::PathExists(zero_pid_file));
  EXPECT_FALSE(base::PathExists(negative_pid_file));
}

TEST_F(CobaltStabilityMetricsHelperTest,
       ClearOtherStabilityMetricsPmaFiles_DeletesZeroByteAndTruncatedPmaFiles) {
  base::Time stamp = base::Time::FromTimeT(1700000000);

  // 0-byte validly named .pma file
  base::FilePath zero_byte_pma =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 1234);
  ASSERT_TRUE(base::WriteFile(zero_byte_pma, ""));

  // Truncated header (e.g. 10 bytes) .pma file
  base::FilePath truncated_pma =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 1235);
  ASSERT_TRUE(base::WriteFile(truncated_pma, "short_junk"));

  ClearOtherStabilityMetricsPmaFiles(metrics_dir(), kExpectedAllocatorName,
                                     /*current_pid=*/5678);

  EXPECT_FALSE(base::PathExists(zero_byte_pma));
  EXPECT_FALSE(base::PathExists(truncated_pma));
}

TEST_F(CobaltStabilityMetricsHelperTest,
       ClearOtherStabilityMetricsPmaFiles_MergesMultiplePriorSessions) {
  base::Time stamp1 = base::Time::FromTimeT(1700000100);
  base::Time stamp2 = base::Time::FromTimeT(1700000200);
  base::ProcessId prior_pid1 = 2001;
  base::ProcessId prior_pid2 = 2002;

  base::FilePath f1 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp1, prior_pid1);
  base::FilePath f2 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp2, prior_pid2);

  // Write histogram 1 in f1
  ASSERT_TRUE(base::GlobalHistogramAllocator::CreateWithFile(
      f1, 64 * 1024, 1, kExpectedAllocatorName));
  base::HistogramBase* h1 = base::Histogram::FactoryGet(
      "Cobalt.Test.MultiSessionA", 1, 100, 10,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  ASSERT_TRUE(h1);
  h1->Add(10);
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Write histogram 2 in f2
  ASSERT_TRUE(base::GlobalHistogramAllocator::CreateWithFile(
      f2, 64 * 1024, 2, kExpectedAllocatorName));
  base::HistogramBase* h2 = base::Histogram::FactoryGet(
      "Cobalt.Test.MultiSessionB", 1, 100, 10,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  ASSERT_TRUE(h2);
  h2->Add(20);
  base::GlobalHistogramAllocator::ReleaseForTesting();

  ClearOtherStabilityMetricsPmaFiles(metrics_dir(), kExpectedAllocatorName,
                                     /*current_pid=*/9999);

  EXPECT_FALSE(base::PathExists(f1));
  EXPECT_FALSE(base::PathExists(f2));

  base::HistogramBase* merged1 =
      base::StatisticsRecorder::FindHistogram("Cobalt.Test.MultiSessionA");
  ASSERT_TRUE(merged1);
  EXPECT_EQ(merged1->SnapshotSamples()->GetCount(10), 1);

  base::HistogramBase* merged2 =
      base::StatisticsRecorder::FindHistogram("Cobalt.Test.MultiSessionB");
  ASSERT_TRUE(merged2);
  EXPECT_EQ(merged2->SnapshotSamples()->GetCount(20), 1);
}

TEST_F(CobaltStabilityMetricsHelperTest,
       GetTotalStabilityMetricsPmaDirSizeBytes_CountsOnlyPmaFiles) {
  EXPECT_EQ(GetTotalStabilityMetricsPmaDirSizeBytes(metrics_dir()), 0);

  base::FilePath pma1 = metrics_dir().AppendASCII("test1.pma");
  base::FilePath pma2 = metrics_dir().AppendASCII("test2.pma");
  base::FilePath txt = metrics_dir().AppendASCII("test.txt");

  ASSERT_TRUE(base::WriteFile(pma1, std::string(100, 'a')));
  ASSERT_TRUE(base::WriteFile(pma2, std::string(200, 'b')));
  ASSERT_TRUE(base::WriteFile(txt, std::string(500, 'c')));

  EXPECT_EQ(GetTotalStabilityMetricsPmaDirSizeBytes(metrics_dir()), 300);
}

TEST_F(CobaltStabilityMetricsHelperTest,
       EnsurePmaDirectoryBudget_UnderBudget_NoPruning) {
  base::Time stamp = base::Time::FromTimeT(1700000000);
  base::FilePath f1 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp, 1001);
  ASSERT_TRUE(base::WriteFile(f1, std::string(128 * 1024, 'x')));

  // Total existing = 128 KB, adding 128 KB, budget = 512 KB -> OK, no pruning.
  EXPECT_TRUE(EnsurePmaDirectoryBudget(metrics_dir(), kExpectedAllocatorName,
                                       512 * 1024, 128 * 1024));
  EXPECT_TRUE(base::PathExists(f1));
}

TEST_F(CobaltStabilityMetricsHelperTest,
       EnsurePmaDirectoryBudget_OverBudget_PrunesOldestFirst) {
  base::Time stamp_old = base::Time::FromTimeT(1700000000);
  base::Time stamp_mid = base::Time::FromTimeT(1700000100);
  base::Time stamp_new = base::Time::FromTimeT(1700000200);

  base::FilePath f_old =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp_old, 1001);
  base::FilePath f_mid =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp_mid, 1002);
  base::FilePath f_new =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), kExpectedAllocatorName, stamp_new, 1003);

  // 3 files of 128 KB each = 384 KB
  ASSERT_TRUE(base::WriteFile(f_old, std::string(128 * 1024, 'x')));
  ASSERT_TRUE(base::WriteFile(f_mid, std::string(128 * 1024, 'y')));
  ASSERT_TRUE(base::WriteFile(f_new, std::string(128 * 1024, 'z')));

  // Budget is 384 KB total. Adding 128 KB would make it 512 KB > 384 KB.
  // Oldest file (f_old) should be pruned.
  EXPECT_TRUE(EnsurePmaDirectoryBudget(metrics_dir(), kExpectedAllocatorName,
                                       384 * 1024, 128 * 1024));

  EXPECT_FALSE(base::PathExists(f_old));
  EXPECT_TRUE(base::PathExists(f_mid));
  EXPECT_TRUE(base::PathExists(f_new));
  EXPECT_EQ(GetTotalStabilityMetricsPmaDirSizeBytes(metrics_dir()), 256 * 1024);
}

TEST_F(CobaltStabilityMetricsHelperTest,
       EnsurePmaDirectoryBudget_DeletesCorruptAndMismatchedPmaFiles) {
  base::FilePath corrupt_pma = metrics_dir().AppendASCII("invalid_file.pma");
  ASSERT_TRUE(base::WriteFile(corrupt_pma, "garbage"));

  base::Time stamp = base::Time::FromTimeT(1700000000);
  base::FilePath mismatched_pma =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir(), "OtherAllocator", stamp, 1001);
  ASSERT_TRUE(base::WriteFile(mismatched_pma, "other data"));

  EXPECT_TRUE(EnsurePmaDirectoryBudget(metrics_dir(), kExpectedAllocatorName,
                                       512 * 1024, 128 * 1024));

  EXPECT_FALSE(base::PathExists(corrupt_pma));
  EXPECT_FALSE(base::PathExists(mismatched_pma));
}

TEST_F(CobaltStabilityMetricsHelperTest,
       EnsurePmaDirectoryBudget_RejectsWhenSingleFileExceedsMax) {
  EXPECT_FALSE(EnsurePmaDirectoryBudget(metrics_dir(), kExpectedAllocatorName,
                                        512 * 1024, 1024 * 1024));
}

}  // namespace
}  // namespace cobalt
