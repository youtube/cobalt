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
#include "base/threading/platform_thread.h"
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
  base::FilePath prior_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 5678);
  ASSERT_TRUE(base::WriteFile(prior_file, ""));

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
  base::FilePath f_non_pma = metrics_dir().AppendASCII("unrelated.txt");

  ASSERT_TRUE(base::WriteFile(f_old, ""));
  // Ensure f_prior has a newer timestamp than f_old.
  base::PlatformThread::Sleep(base::Milliseconds(100));
  ASSERT_TRUE(base::WriteFile(f_prior, ""));
  ASSERT_TRUE(base::WriteFile(f_corrupt, ""));
  ASSERT_TRUE(base::WriteFile(f_non_pma, ""));

  // Clear other PMA files prior to writing new PMA file.
  ClearOtherStabilityMetricsPmaFiles(metrics_dir(), kExpectedAllocatorName,
                                     /*current_pid=*/9999);

  // Only the single prior session PMA file and non-pma file should exist.
  EXPECT_FALSE(base::PathExists(f_old));
  EXPECT_FALSE(base::PathExists(f_corrupt));
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
  // Does not crash on empty directory.
  ClearOtherStabilityMetricsPmaFiles(empty_dir, kExpectedAllocatorName, 9999);

  base::FilePath current_proc_file = ConstructStabilityMetricsFilePath(
      empty_dir, kExpectedAllocatorName, 9999);
  ASSERT_TRUE(base::WriteFile(current_proc_file, ""));

  // Current PID should not be treated as prior session and should be removed.
  ClearOtherStabilityMetricsPmaFiles(empty_dir, kExpectedAllocatorName, 9999);
  EXPECT_FALSE(base::PathExists(current_proc_file));
}

TEST_F(CobaltStabilityMetricsHelperTest,
       ExtractPriorSessionPid_OutFilePathAndCleanup) {
  base::FilePath prior_file = ConstructStabilityMetricsFilePath(
      metrics_dir(), kExpectedAllocatorName, 5555);
  ASSERT_TRUE(base::WriteFile(prior_file, ""));

  base::FilePath out_path;
  std::optional<base::ProcessId> pid = ExtractPriorSessionPid(
      metrics_dir(), kExpectedAllocatorName, /*current_pid=*/9999, &out_path);
  ASSERT_TRUE(pid.has_value());
  EXPECT_EQ(*pid, 5555);
  EXPECT_EQ(out_path, prior_file);
  EXPECT_TRUE(base::PathExists(out_path));

  // Clean up the file after reading it.
  EXPECT_TRUE(base::DeleteFile(out_path));
  EXPECT_FALSE(base::PathExists(out_path));

  // Subsequent extraction finds no prior session file.
  EXPECT_EQ(ExtractPriorSessionPid(metrics_dir(), kExpectedAllocatorName,
                                   /*current_pid=*/9999),
            std::nullopt);
}

}  // namespace
}  // namespace cobalt
