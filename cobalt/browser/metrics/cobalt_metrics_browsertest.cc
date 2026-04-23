// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class CobaltMetricsBrowserTest : public content::ContentBrowserTest {
 public:
  CobaltMetricsBrowserTest() = default;
  ~CobaltMetricsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    // Set a short interval for memory metrics to verify periodic recording.
    command_line->AppendSwitchASCII(
        "enable-features",
        "CobaltMetricsInterval:memory-metrics-interval/1,"
        "CobaltMetricsInterval:cpu-metrics-interval/1");
  }
};

// TODO: b/489836051 - Investigate memory metrics recording failures on
// Starboard.
#if BUILDFLAG(IS_STARBOARD)
#define MAYBE_RecordsMemoryMetrics DISABLED_RecordsMemoryMetrics
#else
#define MAYBE_RecordsMemoryMetrics RecordsMemoryMetrics
#endif
IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest, MAYBE_RecordsMemoryMetrics) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  // Ensure metrics recording is started.
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = manager_client->metrics_service_client();
  ASSERT_TRUE(client);

  // Trigger a memory dump manually for testing and wait for it.
  base::RunLoop run_loop;
  static_cast<CobaltMetricsServiceClient*>(client)->ScheduleRecordForTesting(
      run_loop.QuitClosure());
  run_loop.Run();

  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  auto check_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool exists = histogram && histogram->SnapshotSamples()->TotalCount() > 0;
    if (!exists) {
      LOG(WARNING) << "Histogram not found or empty: " << name;
    }
    return exists;
  };

  // Verify process-specific and region-specific metrics.
  // We check for histograms that we confirmed in the logs to have data.
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.Malloc"));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.JavaHeap"));
#endif
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.Small.Sqlite"));

  // Process-wide metrics
  EXPECT_TRUE(check_histogram("Memory.Browser.ResidentSet"));
  EXPECT_TRUE(check_histogram("Memory.Browser.PrivateMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Browser.SharedMemoryFootprint"));

  // Global aggregate metrics
  EXPECT_TRUE(check_histogram("Memory.Total.ResidentSet"));
  EXPECT_TRUE(check_histogram("Memory.Total.PrivateMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Total.SharedMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Total.PrivateFootprintSwap"));
  EXPECT_TRUE(check_histogram("Memory.Total.VmSize"));

  // Sub-region memory metrics
  EXPECT_TRUE(
      check_histogram("Memory.Experimental.Browser2.Malloc.AllocatedObjects"));

  // These might be 0 or missing depending on the environment/build.
  // We check for them to ensure they are at least attempted.
  check_histogram("Memory.Experimental.Browser2.BlinkGC");
  check_histogram("Memory.Experimental.Browser2.BlinkGC.AllocatedObjects");
  check_histogram("Memory.Experimental.Browser2.PartitionAlloc");
  check_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.AllocatedObjects");
  check_histogram("Memory.Experimental.Browser2.V8");
  check_histogram("Memory.Experimental.Browser2.V8.AllocatedObjects");
  check_histogram("Memory.Experimental.Browser2.Skia");
  check_histogram("Memory.Experimental.Browser2.Skia.Small.SkGlyphCache");
  check_histogram("Memory.Experimental.Browser2.Small.FontCaches");
  check_histogram("Memory.Experimental.Browser2.Small.LevelDatabase");
  check_histogram("Memory.Experimental.Browser2.Small.UI");
  check_histogram("Memory.Experimental.Browser2.Tiny.NumberOfDocuments");
  check_histogram("Memory.Experimental.Browser2.Tiny.NumberOfFrames");
  check_histogram("Memory.Experimental.Browser2.Tiny.NumberOfLayoutObjects");
  check_histogram("Memory.Experimental.Browser2.Small.NumberOfNodes");

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  check_histogram("Memory.Browser.LibChrobaltPss");
  check_histogram("Memory.Browser.LibChrobaltRss");
#endif
}

// TODO: b/489836051 - Investigate periodic memory metrics recording failures on
// Starboard.
#if BUILDFLAG(IS_STARBOARD)
#define MAYBE_PeriodicRecordsMemoryMetrics DISABLED_PeriodicRecordsMemoryMetrics
#else
#define MAYBE_PeriodicRecordsMemoryMetrics PeriodicRecordsMemoryMetrics
#endif
IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       MAYBE_PeriodicRecordsMemoryMetrics) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  // Ensure metrics recording is started.
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = manager_client->metrics_service_client();
  ASSERT_TRUE(client);

  // Trigger a memory dump manually for testing and wait for it.
  // This replaces the fixed delay and is more robust.
  base::RunLoop run_loop;
  static_cast<CobaltMetricsServiceClient*>(client)->ScheduleRecordForTesting(
      run_loop.QuitClosure());
  run_loop.Run();
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  auto check_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool exists = histogram && histogram->SnapshotSamples()->TotalCount() > 0;
    if (!exists) {
      LOG(WARNING) << "Histogram not found or empty: " << name;
    }
    return exists;
  };

  // We expect at least one sample from the periodic collection.
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.Malloc"));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.JavaHeap"));
#endif
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.Small.Sqlite"));

  // Process-wide metrics
  EXPECT_TRUE(check_histogram("Memory.Browser.ResidentSet"));
  EXPECT_TRUE(check_histogram("Memory.Browser.PrivateMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Browser.SharedMemoryFootprint"));

  // Global aggregate metrics
  EXPECT_TRUE(check_histogram("Memory.Total.ResidentSet"));
  EXPECT_TRUE(check_histogram("Memory.Total.PrivateMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Total.SharedMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Total.PrivateFootprintSwap"));
  EXPECT_TRUE(check_histogram("Memory.Total.VmSize"));

  // Sub-region memory metrics
  EXPECT_TRUE(
      check_histogram("Memory.Experimental.Browser2.Malloc.AllocatedObjects"));

  // media decoder buffer memory metrics
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  EXPECT_TRUE(check_histogram("Memory.Media.AllocatedEncodedBuffer"));
#endif

  // Check for the specific regions requested by the user.
  check_histogram("Memory.Experimental.Browser2.BlinkGC");
  check_histogram("Memory.Experimental.Browser2.BlinkGC.AllocatedObjects");
  check_histogram("Memory.Experimental.Browser2.PartitionAlloc");
  check_histogram("Memory.Experimental.Browser2.V8");
  check_histogram("Memory.Experimental.Browser2.Skia");

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  check_histogram("Memory.Browser.LibChrobaltPss");
  check_histogram("Memory.Browser.LibChrobaltRss");
#endif
}

}  // namespace cobalt
