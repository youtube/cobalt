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

#include "cobalt/browser/metrics/cobalt_storage_metrics_helper.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cobalt/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include <sys/prctl.h>
#endif

namespace cobalt {

TEST(CobaltStorageMetricsHelperTest, ParseProcIoCountersValid) {
  constexpr std::string_view kSampleProcIo =
      "rchar: 8800000000\n"
      "wchar: 120000000\n"
      "syscr: 3668187\n"
      "syscw: 450123\n"
      "read_bytes: 15728640\n"
      "write_bytes: 524288\n"
      "cancelled_write_bytes: 4096\n";

  ProcessIoCounters counters;
  ASSERT_TRUE(ParseProcIoCounters(kSampleProcIo, &counters));
  EXPECT_EQ(counters.read_bytes, 15728640);
  EXPECT_EQ(counters.write_bytes, 524288);
}

TEST(CobaltStorageMetricsHelperTest, ParseProcIoCountersMissingFields) {
  constexpr std::string_view kIncompleteProcIo =
      "rchar: 1024\n"
      "wchar: 2048\n"
      "read_bytes: 4096\n";

  ProcessIoCounters counters;
  EXPECT_FALSE(ParseProcIoCounters(kIncompleteProcIo, &counters));
  EXPECT_FALSE(ParseProcIoCounters("", &counters));
  EXPECT_FALSE(ParseProcIoCounters(kIncompleteProcIo, nullptr));
}

TEST(CobaltStorageMetricsHelperTest, RecordStartupStorageAndIoMetricsForPaths) {
  base::ScopedTempDir user_data_dir;
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());

  // Write 24 KB into user_data_dir and 64 KB into cache_dir.
  const std::string user_data_payload(24 * 1024, 'u');
  const std::string cache_payload(64 * 1024, 'c');
  ASSERT_TRUE(base::WriteFile(user_data_dir.GetPath().AppendASCII("Cookies"),
                              user_data_payload));
  ASSERT_TRUE(
      base::WriteFile(cache_dir.GetPath().AppendASCII("index"), cache_payload));

  const ProcessIoCounters io_counters{
      .read_bytes = 12 * 1024 * 1024,  // 12,288 KB
      .write_bytes = 350 * 1024,       // 350 KB
  };

  base::HistogramTester histogram_tester;
  RecordStartupStorageAndIoMetricsForPaths(
      {.user_data = user_data_dir.GetPath(), .dir_cache = cache_dir.GetPath()},
      &io_counters);

  histogram_tester.ExpectUniqueSample("Cobalt.Storage.ProcessIo.StartupReadKB",
                                      12288, 1);
  histogram_tester.ExpectUniqueSample("Cobalt.Storage.ProcessIo.StartupWriteKB",
                                      350, 1);
  // Logical sizes (sum of file sizes).
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.UserData.StartupTotalSizeKB", 24, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.StartupTotalSizeKB", 64, 1);
  // /proc/self/io does not report major page faults.
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.ProcessIo.StartupMajorPageFaults", 0);
  // The same walk records the allocated footprint.
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.Footprint.Startup.UserData.SizeKB", 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.Footprint.Startup.UserData.FileCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.Footprint.Startup.DirCache.FileCount", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.Footprint.Startup.Bucket.Cookies.SizeKB", 1);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.Footprint.Startup.AppData.SizeKB", 0);
  histogram_tester.ExpectUniqueSample("Cobalt.Storage.Footprint.WalkTruncated",
                                      false, 1);
}

TEST(CobaltStorageMetricsHelperTest, ProcessIoCountersFromRusage) {
  ProcessIoCounters counters;
  ASSERT_TRUE(ProcessIoCountersFromRusage(/*in_blocks=*/30720,
                                          /*out_blocks=*/700,
                                          /*major_page_faults=*/1234,
                                          &counters));
  EXPECT_EQ(counters.read_bytes, 30720 * 512);
  EXPECT_EQ(counters.write_bytes, 700 * 512);
  EXPECT_EQ(counters.major_page_faults, 1234);

  // Reads can legitimately be 0 when every page was already cached.
  EXPECT_TRUE(ProcessIoCountersFromRusage(0, 8, 0, &counters));

  // Both block counts at 0 means the kernel does not account task I/O.
  EXPECT_FALSE(ProcessIoCountersFromRusage(0, 0, 5, &counters));
  EXPECT_FALSE(ProcessIoCountersFromRusage(-1, 8, 0, &counters));
  EXPECT_FALSE(ProcessIoCountersFromRusage(8, -1, 0, &counters));
  EXPECT_FALSE(ProcessIoCountersFromRusage(8, 8, -1, &counters));
  EXPECT_FALSE(ProcessIoCountersFromRusage(8, 8, 0, nullptr));
}

TEST(CobaltStorageMetricsHelperTest, RecordsStartupMajorPageFaults) {
  ProcessIoCounters io_counters;
  ASSERT_TRUE(ProcessIoCountersFromRusage(/*in_blocks=*/24576,
                                          /*out_blocks=*/2048,
                                          /*major_page_faults=*/4321,
                                          &io_counters));

  base::HistogramTester histogram_tester;
  RecordStartupStorageAndIoMetricsForPaths(StorageRootPaths(), &io_counters);

  histogram_tester.ExpectUniqueSample("Cobalt.Storage.ProcessIo.StartupReadKB",
                                      12288, 1);
  histogram_tester.ExpectUniqueSample("Cobalt.Storage.ProcessIo.StartupWriteKB",
                                      1024, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.ProcessIo.StartupMajorPageFaults", 4321, 1);
}

TEST(CobaltStorageMetricsHelperTest, StorageFootprintUptimeInterval) {
  EXPECT_EQ(GetStorageFootprintUptimeInterval(), base::Hours(24));
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltStorageFootprintSampler,
        {{"CobaltStorageFootprintSampler_uptime_interval", "2m"}});
    EXPECT_EQ(GetStorageFootprintUptimeInterval(), base::Minutes(2));
  }
  {
    // Clamped to at least 1 minute.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kCobaltStorageFootprintSampler,
        {{"CobaltStorageFootprintSampler_uptime_interval", "5s"}});
    EXPECT_EQ(GetStorageFootprintUptimeInterval(), base::Minutes(1));
  }
}

TEST(CobaltStorageMetricsHelperTest, ReadCurrentProcessIoCounters) {
  ProcessIoCounters counters;
  if (!ReadCurrentProcessIoCounters(&counters)) {
    GTEST_SKIP() << "Block I/O counters are unavailable on this platform.";
  }
  EXPECT_GE(counters.read_bytes, 0);
  EXPECT_GE(counters.write_bytes, 0);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_GE(counters.major_page_faults, 0);
#else
  EXPECT_EQ(counters.major_page_faults, -1);
#endif
}

#if BUILDFLAG(IS_ANDROID)
// Release (non-debuggable) APKs run non-dumpable, which makes /proc/self/io
// owned by root and unreadable. The counters must still be available.
TEST(CobaltStorageMetricsHelperTest, ReadCurrentProcessIoCountersNotDumpable) {
  ProcessIoCounters before;
  if (!ReadCurrentProcessIoCounters(&before)) {
    GTEST_SKIP() << "The kernel does not account task I/O.";
  }
  const int was_dumpable = prctl(PR_GET_DUMPABLE);
  ASSERT_EQ(prctl(PR_SET_DUMPABLE, 0), 0);

  ProcessIoCounters after;
  const bool read_ok = ReadCurrentProcessIoCounters(&after);
  ASSERT_EQ(prctl(PR_SET_DUMPABLE, was_dumpable), 0);

  ASSERT_TRUE(read_ok);
  EXPECT_GE(after.read_bytes, before.read_bytes);
  EXPECT_GE(after.write_bytes, before.write_bytes);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST(CobaltStorageMetricsHelperTest, RecordsIOJankPerMinute) {
  if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
    GTEST_SKIP() << "I/O jank monitoring needs consistent TimeTicks.";
  }
  std::optional<base::test::TaskEnvironment> task_environment(
      std::in_place, base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  base::HistogramTester histogram_tester;
  EnableCobaltIOJankMonitoring();

  {
    // A 3 s blocking call on a foreground thread is attributed to three 1 s
    // intervals of the current one-minute monitoring window.
    base::ScopedBlockingCall blocked_for_3s(FROM_HERE,
                                            base::BlockingType::MAY_BLOCK);
    task_environment->FastForwardBy(base::Seconds(3));
  }
  // The window ends 60 s after monitoring started and reports once.
  task_environment->FastForwardBy(base::Minutes(1));

  // Every thread of the process is monitored, so blocking calls outside this
  // test can add jank (see crbug.com/1071166); check lower bounds only.
  histogram_tester.ExpectTotalCount(
      "Cobalt.Responsiveness.IOJankyIntervalsPerMinute", 1);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Responsiveness.IOJanksTotalPerMinute", 1);
  EXPECT_GE(histogram_tester.GetTotalSum(
                "Cobalt.Responsiveness.IOJankyIntervalsPerMinute"),
            3);
  EXPECT_GE(histogram_tester.GetTotalSum(
                "Cobalt.Responsiveness.IOJanksTotalPerMinute"),
            3);

  // Reclaim worker threads before cancelling, as base's own tests do.
  task_environment.reset();
  base::internal::IOJankMonitoringWindow::CancelMonitoringForTesting();
}

TEST(CobaltStorageMetricsHelperTest, RecordSteadyStateWriteRateForInterval) {
  base::HistogramTester histogram_tester;
  const base::TimeTicks t0 = base::TimeTicks::Now();

  // An empty interval should not emit.
  EXPECT_FALSE(RecordSteadyStateWriteRateForInterval(
      /*baseline_write_bytes=*/100 * 1024, t0,
      /*current_write_bytes=*/400 * 1024, t0));
  // Time going backwards should not emit.
  EXPECT_FALSE(RecordSteadyStateWriteRateForInterval(
      /*baseline_write_bytes=*/100 * 1024, t0,
      /*current_write_bytes=*/400 * 1024, t0 - base::Minutes(1)));
  // The write counter going backwards should not emit.
  EXPECT_FALSE(RecordSteadyStateWriteRateForInterval(
      /*baseline_write_bytes=*/400 * 1024, t0,
      /*current_write_bytes=*/100 * 1024, t0 + base::Minutes(5)));
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.ProcessIo.WriteRateKBPerMin", 0);

  // 600 KB written over 2 minutes -> 300 KB/min.
  EXPECT_TRUE(RecordSteadyStateWriteRateForInterval(
      /*baseline_write_bytes=*/100 * 1024, t0,
      /*current_write_bytes=*/700 * 1024, t0 + base::Minutes(2)));
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.ProcessIo.WriteRateKBPerMin", 300, 1);
}

}  // namespace cobalt
