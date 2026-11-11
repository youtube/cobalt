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

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  RecordStartupStorageAndIoMetricsForPaths(user_data_dir.GetPath(),
                                           cache_dir.GetPath(), &io_counters);

  histogram_tester.ExpectUniqueSample("Cobalt.Storage.ProcessIo.StartupReadKB",
                                      12288, 1);
  histogram_tester.ExpectUniqueSample("Cobalt.Storage.ProcessIo.StartupWriteKB",
                                      350, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.UserData.StartupTotalSizeKB", 24, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.StartupTotalSizeKB", 64, 1);
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
