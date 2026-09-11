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
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/process/process_handle.h"
#include "base/test/simple_test_clock.h"
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
       ConstructAndExtractStabilityMetricsPid) {
  base::ProcessId pid = 1234;
  base::FilePath file_path = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, pid);
  EXPECT_EQ(file_path.BaseName().value(), "BrowserStabilityMetrics-4D2.pma");

  // Extracts PID from 3-part filename.
  std::optional<base::ProcessId> extracted =
      ExtractStabilityMetricsPid(file_path, kExpectedAllocatorName);
  ASSERT_TRUE(extracted.has_value());
  EXPECT_EQ(*extracted, pid);

  // Backward compatibility: extracts PID from 4-part filename with timestamp.
  base::FilePath legacy_file =
      metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000-4D2.pma");
  std::optional<base::ProcessId> legacy_extracted =
      ExtractStabilityMetricsPid(legacy_file, kExpectedAllocatorName);
  ASSERT_TRUE(legacy_extracted.has_value());
  EXPECT_EQ(*legacy_extracted, pid);

  // Allocator name matching.
  EXPECT_FALSE(
      ExtractStabilityMetricsPid(file_path, "OtherAllocator").has_value());
  // Empty expected name accepts any matching format.
  EXPECT_TRUE(ExtractStabilityMetricsPid(file_path, "").has_value());
}

TEST_F(CobaltStabilityMetricsHelperTest,
       HandlesEmptyAndNonExistentDirectories) {
  base::FilePath non_existent = metrics_dir().AppendASCII("non_existent_dir");
  EXPECT_EQ(ExtractPriorSessionPid(non_existent, kExpectedAllocatorName,
                                   /*current_pid=*/100),
            std::nullopt);
  EXPECT_TRUE(ExtractPriorSessionPids(non_existent, kExpectedAllocatorName,
                                      /*current_pid=*/100)
                  .empty());

  base::FilePath empty_dir = metrics_dir().AppendASCII("empty_dir");
  ASSERT_TRUE(base::CreateDirectory(empty_dir));
  EXPECT_EQ(ExtractPriorSessionPid(empty_dir, kExpectedAllocatorName,
                                   /*current_pid=*/100),
            std::nullopt);
  EXPECT_TRUE(ExtractPriorSessionPids(empty_dir, kExpectedAllocatorName,
                                      /*current_pid=*/100)
                  .empty());
}

TEST_F(CobaltStabilityMetricsHelperTest, ExtractsPriorSessionPid) {
  base::FilePath old_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 1234);
  base::FilePath prior_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 5678);
  ASSERT_TRUE(base::WriteFile(old_file, ""));
  ASSERT_TRUE(base::WriteFile(prior_file, ""));

  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  ASSERT_TRUE(base::TouchFile(old_file, clock.Now(), clock.Now()));

  clock.Advance(base::Hours(1));
  ASSERT_TRUE(base::TouchFile(prior_file, clock.Now(), clock.Now()));

  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   /*current_pid=*/9999),
            5678);
  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  EXPECT_THAT(pids, ::testing::ElementsAre(5678));
}

TEST_F(CobaltStabilityMetricsHelperTest, FiltersOutCurrentProcessPid) {
  base::ProcessId current_pid = 4321;
  base::FilePath current_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, current_pid);
  ASSERT_TRUE(base::WriteFile(current_file, ""));

  base::FilePath prior_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 1111);
  ASSERT_TRUE(base::WriteFile(prior_file, ""));

  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   current_pid),
            1111);
  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, current_pid);
  EXPECT_THAT(pids, ::testing::ElementsAre(1111));
}

TEST_F(CobaltStabilityMetricsHelperTest, RejectsMismatchedAllocatorNames) {
  base::FilePath other_allocator_file =
      ConstructStabilityMetricsFilePath(metrics_dir(), "OtherAllocator", 1234);
  ASSERT_TRUE(base::WriteFile(other_allocator_file, ""));

  base::FilePath expected_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 5678);
  ASSERT_TRUE(base::WriteFile(expected_file, ""));

  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   /*current_pid=*/9999),
            5678);
  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  EXPECT_THAT(pids, ::testing::ElementsAre(5678));
}

TEST_F(CobaltStabilityMetricsHelperTest, IgnoresCorruptFilenamesAndSubdirs) {
  // Non-.pma extension.
  ASSERT_TRUE(
      base::WriteFile(metrics_dir().AppendASCII("not_a_pma_file.txt"), ""));
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-1234.tmp"), ""));

  // Malformed PMA filenames that fail PID extraction.
  ASSERT_TRUE(
      base::WriteFile(metrics_dir().AppendASCII("corrupt_name.pma"), ""));
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-invalidhex.pma"), ""));
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-1-2-3-4.pma"), ""));

  // Subdirectories should not be traversed or counted as files.
  ASSERT_TRUE(base::CreateDirectory(metrics_dir().AppendASCII("subdir.pma")));
  ASSERT_TRUE(base::CreateDirectory(ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 9876)));

  // Valid file.
  base::FilePath valid_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 7777);
  ASSERT_TRUE(base::WriteFile(valid_file, ""));

  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   /*current_pid=*/9999),
            7777);
  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  EXPECT_THAT(pids, ::testing::ElementsAre(7777));
}

TEST_F(CobaltStabilityMetricsHelperTest, RejectsZeroAndNegativePids) {
  // Zero PID in filename.
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-0.pma"), ""));

  // Negative PID in filename.
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics--1.pma"), ""));

  // Malformed hex in 4-part filename.
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-invalidhex-1234.pma"),
      ""));

  // Malformed legacy 4-part filenames with negative PID or negative timestamp.
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000--1.pma"),
      ""));
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000-0.pma"), ""));
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics--65550000-1234.pma"),
      ""));
  // Out-of-range PID exceeding numeric bounds for ProcessId.
  ASSERT_TRUE(base::WriteFile(
      metrics_dir().AppendASCII("BrowserStabilityMetrics-7FFFFFFFFFFFFFFF.pma"),
      ""));

  // Direct extraction should return std::nullopt.
  EXPECT_EQ(ExtractStabilityMetricsPid(
                metrics_dir().AppendASCII("BrowserStabilityMetrics-0.pma"),
                kExpectedAllocatorName),
            std::nullopt);
  EXPECT_EQ(ExtractStabilityMetricsPid(
                metrics_dir().AppendASCII("BrowserStabilityMetrics--1.pma"),
                kExpectedAllocatorName),
            std::nullopt);
  EXPECT_EQ(ExtractStabilityMetricsPid(
                metrics_dir().AppendASCII(
                    "BrowserStabilityMetrics-invalidhex-1234.pma"),
                kExpectedAllocatorName),
            std::nullopt);
  EXPECT_EQ(
      ExtractStabilityMetricsPid(
          metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000--1.pma"),
          kExpectedAllocatorName),
      std::nullopt);
  EXPECT_EQ(
      ExtractStabilityMetricsPid(
          metrics_dir().AppendASCII("BrowserStabilityMetrics-65550000-0.pma"),
          kExpectedAllocatorName),
      std::nullopt);
  EXPECT_EQ(ExtractStabilityMetricsPid(
                metrics_dir().AppendASCII(
                    "BrowserStabilityMetrics--65550000-1234.pma"),
                kExpectedAllocatorName),
            std::nullopt);
  EXPECT_EQ(ExtractStabilityMetricsPid(
                metrics_dir().AppendASCII(
                    "BrowserStabilityMetrics-7FFFFFFFFFFFFFFF.pma"),
                kExpectedAllocatorName),
            std::nullopt);

  // Before adding valid_file, prior session extraction finds no valid files.
  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   /*current_pid=*/9999),
            std::nullopt);

  // Valid PID.
  base::FilePath valid_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 8888);
  ASSERT_TRUE(base::WriteFile(valid_file, ""));

  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   /*current_pid=*/9999),
            8888);
  std::vector<base::ProcessId> pids = ExtractPriorSessionPids(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  EXPECT_THAT(pids, ::testing::ElementsAre(8888));
}

TEST_F(CobaltStabilityMetricsHelperTest, ClearOtherStabilityMetricsPmaFiles) {
  base::FilePath f_old = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 1001);
  base::FilePath f_prior = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 1002);
  base::FilePath f_corrupt =
      metrics_dir().AppendASCII("BrowserStabilityMetrics-invalidhex.pma");
  base::FilePath f_corrupt_stamp =
      metrics_dir().AppendASCII("BrowserStabilityMetrics-invalidhex-1234.pma");
  base::FilePath f_corrupt_neg =
      metrics_dir().AppendASCII("BrowserStabilityMetrics--1.pma");
  base::FilePath f_non_pma = metrics_dir().AppendASCII("unrelated.txt");

  ASSERT_TRUE(base::WriteFile(f_old, ""));
  ASSERT_TRUE(base::WriteFile(f_prior, ""));
  ASSERT_TRUE(base::WriteFile(f_corrupt, ""));
  ASSERT_TRUE(base::WriteFile(f_corrupt_stamp, ""));
  ASSERT_TRUE(base::WriteFile(f_corrupt_neg, ""));
  ASSERT_TRUE(base::WriteFile(f_non_pma, ""));

  // Note: base::FileEnumerator inspects filesystem modification timestamps
  // (GetLastModifiedTime()) read from the underlying OS filesystem rather than
  // base::Time::Now(). Therefore, we advance a mocked clock
  // (base::SimpleTestClock) deterministically and explicitly tie file
  // timestamps to the advanced mocked clock via base::TouchFile(file,
  // clock.Now(), clock.Now()).
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  ASSERT_TRUE(base::TouchFile(f_old, clock.Now(), clock.Now()));

  clock.Advance(base::Hours(1));
  ASSERT_TRUE(base::TouchFile(f_prior, clock.Now(), clock.Now()));

  // Clear other PMA files prior to writing new PMA file.
  std::optional<base::ProcessId> retained_pid =
      ClearOtherStabilityMetricsPmaFiles(metrics_dir(), kExpectedAllocatorName,
                                         /*current_pid=*/9999);

  // Verify ClearOtherStabilityMetricsPmaFiles returns the expected prior PID.
  EXPECT_EQ(retained_pid, 1002);

  // Only the single prior session PMA file and non-pma file should exist.
  EXPECT_FALSE(base::PathExists(f_old));
  EXPECT_FALSE(base::PathExists(f_corrupt));
  EXPECT_FALSE(base::PathExists(f_corrupt_stamp));
  EXPECT_FALSE(base::PathExists(f_corrupt_neg));
  EXPECT_TRUE(base::PathExists(f_prior));
  EXPECT_TRUE(base::PathExists(f_non_pma));

  // ExtractPriorSessionPid returns 1002.
  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   /*current_pid=*/9999),
            1002);
}

TEST_F(CobaltStabilityMetricsHelperTest,
       ClearOtherStabilityMetricsPmaFiles_EmptyAndCurrentPid) {
  base::FilePath empty_dir = metrics_dir().AppendASCII("empty_test");
  ASSERT_TRUE(base::CreateDirectory(empty_dir));
  // Does not crash on empty directory, returns nullopt.
  EXPECT_EQ(ClearOtherStabilityMetricsPmaFiles(empty_dir,
                                               kExpectedAllocatorName, 9999),
            std::nullopt);

  base::FilePath current_proc_file = ConstructStabilityMetricsFilePath(
      empty_dir, kExpectedAllocatorName, 9999);
  ASSERT_TRUE(base::WriteFile(current_proc_file, ""));

  // Current PID should not be treated as prior session, should be removed,
  // and function returns nullopt.
  EXPECT_EQ(ClearOtherStabilityMetricsPmaFiles(empty_dir,
                                               kExpectedAllocatorName, 9999),
            std::nullopt);
  EXPECT_FALSE(base::PathExists(current_proc_file));
}

TEST_F(CobaltStabilityMetricsHelperTest, ExtractPriorSessionPid_FileCleanup) {
  base::FilePath prior_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 5555);
  ASSERT_TRUE(base::WriteFile(prior_file, ""));

  std::optional<base::ProcessId> pid = ExtractPriorSessionPid(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999);
  ASSERT_TRUE(pid.has_value());
  EXPECT_EQ(*pid, 5555);
  EXPECT_TRUE(base::PathExists(prior_file));

  // Clean up the file after reading it.
  EXPECT_TRUE(base::DeleteFile(prior_file));
  EXPECT_FALSE(base::PathExists(prior_file));

  // Subsequent extraction finds no prior session file.
  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   /*current_pid=*/9999),
            std::nullopt);
}

}  // namespace
}  // namespace cobalt
