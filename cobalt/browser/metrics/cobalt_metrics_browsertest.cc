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

#include "base/base_paths.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_detailed_metrics_delegate.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"
#include "cobalt/browser/metrics/cobalt_stability_metrics_helper.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class CobaltMetricsBrowserTest : public content::ContentBrowserTest {
 public:
  CobaltMetricsBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kCobaltMetricsIntervalFeature,
        {{"memory-metrics-interval", "1"}, {"cpu-metrics-interval", "1"}});
  }
  ~CobaltMetricsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
    if (auto* instrumentation =
            memory_instrumentation::MemoryInstrumentation::GetInstance()) {
      static base::NoDestructor<CobaltDetailedMetricsDelegate> delegate;
      instrumentation->SetDetailedMetricsDelegate(delegate.get());
    }
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO: b/489836051 - Investigate memory metrics recording failures on
// Starboard.
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
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

  // Load a page that allocates WTF elements and a JS ArrayBuffer to ensure
  // PartitionAlloc partitions are populated
  std::string html_content = R"(
    <html>
    <body>
      <script>
        const ab = new ArrayBuffer(1024 * 1024);
        const div = document.createElement('div');
        div.style.width = '100px';
        document.body.appendChild(div);
      </script>
    </body>
    </html>
  )";
  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // Trigger a memory dump manually for testing and wait for it.
  base::RunLoop run_loop;
  static_cast<CobaltMetricsServiceClient*>(client)
      ->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  std::string registered_histograms;
  base::StatisticsRecorder::WriteGraph("Memory.Experimental.Browser2",
                                       &registered_histograms);
  LOG(INFO) << "Registered Memory.Experimental.Browser2 histograms:\n"
            << registered_histograms;

  auto check_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool exists = histogram && histogram->SnapshotSamples()->TotalCount() > 0;
    if (!exists) {
      LOG(WARNING) << "Histogram not found or empty: " << name;
    }
    return exists;
  };

  auto check_non_zero_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool valid = histogram && histogram->SnapshotSamples()->sum() > 0;
    if (!valid) {
      LOG(WARNING) << "Histogram not found, empty, or zero: " << name;
    }
    return valid;
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
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer"));
  EXPECT_TRUE(
      check_non_zero_histogram("Memory.Experimental.Browser2.PartitionAlloc."
                               "AllocatedObjects.ArrayBuffer"));
  EXPECT_TRUE(check_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.Buffer"));
  EXPECT_TRUE(check_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.AllocatedObjects.Buffer"));
  EXPECT_TRUE(check_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.MaxCommittedSize.Buffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.CommittedSize.Allocator"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.AllocatedObjects.Allocator"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.MaxCommittedSize.Allocator"));
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

  check_histogram("Memory.Browser.LibChrobaltPss");
  check_histogram("Memory.Browser.LibChrobaltRss");
  check_histogram("Memory.Browser.PartitionAllocRss");
#if BUILDFLAG(IS_ANDROID)
  check_histogram("Memory.Browser.MallocRss");
#endif
}

// TODO: b/489836051 - Investigate periodic memory metrics recording failures on
// Starboard.
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
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

  // Load a page that allocates WTF elements and a JS ArrayBuffer to ensure
  // PartitionAlloc partitions are populated
  std::string html_content = R"(
    <html>
    <body>
      <script>
        const ab = new ArrayBuffer(1024 * 1024);
        const div = document.createElement('div');
        div.style.width = '100px';
        document.body.appendChild(div);
      </script>
    </body>
    </html>
  )";
  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // Trigger a memory dump manually for testing and wait for it.
  // This replaces the fixed delay and is more robust.
  base::RunLoop run_loop;
  static_cast<CobaltMetricsServiceClient*>(client)
      ->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
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

  auto check_non_zero_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool valid = histogram && histogram->SnapshotSamples()->sum() > 0;
    if (!valid) {
      LOG(WARNING) << "Histogram not found, empty, or zero: " << name;
    }
    return valid;
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
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer"));
  EXPECT_TRUE(
      check_non_zero_histogram("Memory.Experimental.Browser2.PartitionAlloc."
                               "AllocatedObjects.ArrayBuffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.Buffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.AllocatedObjects.Buffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.MaxCommittedSize.Buffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.CommittedSize.Allocator"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.AllocatedObjects.Allocator"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.MaxCommittedSize.Allocator"));
  check_histogram("Memory.Experimental.Browser2.V8");
  check_histogram("Memory.Experimental.Browser2.Skia");

  check_histogram("Memory.Browser.LibChrobaltPss");
  check_histogram("Memory.Browser.LibChrobaltRss");
  check_histogram("Memory.Browser.PartitionAllocRss");
#if BUILDFLAG(IS_ANDROID)
  check_histogram("Memory.Browser.MallocRss");
#endif
}

// TODO: b/489836051 - Investigate periodic memory metrics recording failures on
// Starboard.
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
#define MAYBE_RecordsCpuMetrics DISABLED_RecordsCpuMetrics
#else
#define MAYBE_RecordsCpuMetrics RecordsCpuMetrics
#endif
IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest, MAYBE_RecordsCpuMetrics) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = static_cast<CobaltMetricsServiceClient*>(
      manager_client->metrics_service_client());
  ASSERT_TRUE(client);

  // Trigger CPU metrics dump manually for testing and wait for it.
  // This replaces the fixed delay and is more robust.
  base::RunLoop run_loop;
  client->ScheduleCpuRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  EXPECT_GE(
      histogram_tester.GetAllSamples("CPU.Total.UsageInPercentage").size(), 1u);
  // verify ProcessMetrics::GetPlatformIndependentCPUUsage() returns 0
  // on the first call
  EXPECT_GE(histogram_tester.GetBucketCount("CPU.Total.UsageInPercentage", 0),
            1);
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       StabilityMetricsPersistentAllocatorInitialized) {
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  ASSERT_NE(allocator, nullptr);
  EXPECT_EQ(allocator->Name(), "BrowserStabilityMetrics");

  base::PersistentMemoryAllocator* mem_allocator =
      allocator->memory_allocator();
  ASSERT_NE(mem_allocator, nullptr);

  // Verify the strict 512 KiB cap
  EXPECT_EQ(mem_allocator->size(), 512u * 1024u);

  // Verify allocator ID "STAB"
  EXPECT_EQ(mem_allocator->Id(), 0x53544142u);

  // Verify the allocator is valid and not corrupt
  EXPECT_FALSE(mem_allocator->IsCorrupt());
  EXPECT_GT(mem_allocator->used(), 0u);
  EXPECT_LT(mem_allocator->used(), mem_allocator->size());
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       StartupMilestonesAndNavigationMetricsRecorded) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  // Navigate to a test page to trigger navigation lifecycle events
  GURL url("data:text/html;charset=utf-8,<h1>Startup Milestone Test</h1>");
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // Sync histograms from persistent memory / providers
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  // Milestone 17 was recorded in PreCreateThreads (before test body)
  base::HistogramBase* milestone_hist = base::StatisticsRecorder::FindHistogram(
      "Cobalt.Startup.MilestoneReached");
  ASSERT_TRUE(milestone_hist);
  EXPECT_GE(milestone_hist->SnapshotSamples()->GetCount(17), 1);

  // Milestone 22 (DidStartNavigation) and 26 (DidFinishNavigation)
  EXPECT_EQ(
      histogram_tester.GetBucketCount("Cobalt.Startup.MilestoneReached", 22),
      1);
  EXPECT_EQ(
      histogram_tester.GetBucketCount("Cobalt.Startup.MilestoneReached", 26),
      1);

  // Verify navigation duration metric was recorded exactly once
  histogram_tester.ExpectTotalCount(
      "Cobalt.Startup.Time.NavigationDispatchToCommit", 1);

  // Second navigation should not re-record startup milestones or duration
  GURL second_url("data:text/html;charset=utf-8,<h1>Second Page</h1>");
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), second_url));
  EXPECT_EQ(
      histogram_tester.GetBucketCount("Cobalt.Startup.MilestoneReached", 22),
      1);
  EXPECT_EQ(
      histogram_tester.GetBucketCount("Cobalt.Startup.MilestoneReached", 26),
      1);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Startup.Time.NavigationDispatchToCommit", 1);
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       StabilityMetricsCapacityMonitoring) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  // Verify PercentFull is sampled and within valid bounds (0 - 100%)
  base::HistogramBase* percent_hist = base::StatisticsRecorder::FindHistogram(
      "Cobalt.StabilityMetrics.PercentFull");
  ASSERT_TRUE(percent_hist);
  auto percent_samples = percent_hist->SnapshotSamples();
  EXPECT_GT(percent_samples->TotalCount(), 0);
  EXPECT_LE(percent_samples->sum() / percent_samples->TotalCount(), 100);

  // Verify UsedKilobytes is sampled and within 0 - 512 KB
  base::HistogramBase* used_kb_hist = base::StatisticsRecorder::FindHistogram(
      "Cobalt.StabilityMetrics.UsedKilobytes");
  ASSERT_TRUE(used_kb_hist);
  auto used_samples = used_kb_hist->SnapshotSamples();
  EXPECT_GT(used_samples->TotalCount(), 0);
  EXPECT_LE(used_samples->sum() / used_samples->TotalCount(), 512);

  // Verify we have not hit near-capacity condition under normal operation
  // and that the false bucket (the denominator) is recorded.
  base::HistogramBase* near_capacity_hist =
      base::StatisticsRecorder::FindHistogram(
          "Cobalt.StabilityMetrics.IsNearCapacity");
  ASSERT_TRUE(near_capacity_hist);
  EXPECT_EQ(near_capacity_hist->SnapshotSamples()->GetCount(1), 0);
  EXPECT_GT(near_capacity_hist->SnapshotSamples()->GetCount(0), 0);
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       FileMetricsProviderRegistrationAndPrefs) {
  auto* features = GlobalFeatures::GetInstance();
  ASSERT_TRUE(features);
  PrefService* local_state = features->metrics_local_state();
  ASSERT_TRUE(local_state);

  // Verify FileMetricsProvider prefs were properly registered for
  // BrowserStabilityMetrics
  EXPECT_TRUE(
      local_state->FindPreference(metrics::prefs::kMetricsFileMetricsMetadata));
  EXPECT_TRUE(
      local_state->FindPreference(metrics::prefs::kMetricsLastSeenPrefix +
                                  std::string("BrowserStabilityMetrics")));
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       StabilityMetricsPidExtraction) {
  base::FilePath base_dir;
  bool path_ok = false;
#if BUILDFLAG(IS_ANDROID)
  path_ok = base::PathService::Get(base::DIR_ANDROID_APP_DATA, &base_dir);
#else
  path_ok = base::PathService::Get(base::DIR_TEMP, &base_dir);
#endif
  ASSERT_TRUE(path_ok);

  base::FilePath metrics_dir = base_dir.AppendASCII("BrowserStabilityMetrics");

  // Verify that a prior session file name constructed with a known PID
  // and timestamp correctly yields the PID upon ParseFilePath.
  base::ProcessId simulated_pid = 12345;
  base::Time simulated_stamp = base::Time::FromTimeT(1700000000);
  base::FilePath simulated_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, "BrowserStabilityMetrics", simulated_stamp,
          simulated_pid);

  std::string parsed_name;
  base::Time parsed_stamp;
  base::ProcessId parsed_pid = 0;
  EXPECT_TRUE(base::GlobalHistogramAllocator::ParseFilePath(
      simulated_file, &parsed_name, &parsed_stamp, &parsed_pid));
  EXPECT_EQ(parsed_name, "BrowserStabilityMetrics");
  EXPECT_EQ(parsed_stamp.ToTimeT(), simulated_stamp.ToTimeT());
  EXPECT_EQ(parsed_pid, simulated_pid);
  EXPECT_GT(parsed_pid, 0);
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       ProcessStateSummaryManagerInBrowserProcess) {
  auto* manager = CobaltProcessStateSummaryManager::GetInstance();
  ASSERT_TRUE(manager != nullptr);

  // Update memory footprint with realistic numbers (in KB)
  manager->UpdateMemoryFootprint(450 * 1024, 380 * 1024, 64 * 1024);
  manager->UpdateTrimMemoryLevel(15);  // TRIM_MEMORY_RUNNING_CRITICAL
  manager->SetFlags(kFlagForeground | kFlagMediaPlaying);

  // Verify roundtrip serialization in browser process environment
  ProcessStateSnapshot snapshot;
  snapshot.peak_rss_kb = 450 * 1024;
  snapshot.peak_pmf_kb = 380 * 1024;
  snapshot.peak_v8_code_kb = 64 * 1024;
  snapshot.uptime_sec = 1800;  // 30 minutes
  snapshot.last_trim_level = 15;
  snapshot.flags = kFlagForeground | kFlagMediaPlaying;

  std::vector<uint8_t> payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(snapshot);
  EXPECT_EQ(payload.size(), kProcessStateSummaryPayloadSize);

  auto deserialized =
      CobaltProcessStateSummaryManager::DeserializeSnapshot(payload);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(*deserialized, snapshot);
  EXPECT_EQ(deserialized->uptime_minutes(), 30u);
}

struct PriorSessionAttributionTestParams {
  int exit_reason;
  const char* expected_suffix;
  uint32_t peak_rss_kb;
  uint32_t peak_pmf_kb;
  uint32_t peak_v8_code_kb;
  uint32_t uptime_sec;
  uint8_t last_trim_level;
  uint8_t flags;
};

const PriorSessionAttributionTestParams kPriorSessionAttributionTestCases[] = {
    // Standard Android ApplicationExitInfo reason codes (0 through 17)
    {0, "Anr", 350 * 1024, 300 * 1024, 50 * 1024, 1800, 10, kFlagForeground},
    {1, "Crash", 250 * 1024, 200 * 1024, 35 * 1024, 300, 0, kFlagForeground},
    {2, "CrashNative", 400 * 1024, 340 * 1024, 60 * 1024, 1200, 5,
     kFlagForeground | kFlagMediaPlaying},
    {3, "DependencyDied", 220 * 1024, 180 * 1024, 25 * 1024, 600, 15, 0},
    {4, "ExcessiveResourceUsage", 550 * 1024, 480 * 1024, 85 * 1024, 7200, 80,
     kFlagForeground},
    {5, "ExitSelf", 180 * 1024, 150 * 1024, 20 * 1024, 900, 0, 0},
    {6, "InitializationFailure", 120 * 1024, 90 * 1024, 10 * 1024, 30, 0, 0},
    {7, "LowMemory", 512 * 1024, 450 * 1024, 80 * 1024, 3600, 80,
     kFlagForeground},
    {8, "Other", 300 * 1024, 240 * 1024, 40 * 1024, 1500, 20,
     kFlagMediaPlaying},
    {9, "PermissionChange", 210 * 1024, 170 * 1024, 25 * 1024, 750, 40, 0},
    {10, "Signaled", 280 * 1024, 230 * 1024, 38 * 1024, 1100, 0,
     kFlagForeground},
    {11, "Unknown", 190 * 1024, 160 * 1024, 22 * 1024, 400, 0, 0},
    {12, "UserRequested", 260 * 1024, 210 * 1024, 30 * 1024, 2400, 60,
     kFlagForeground},
    {13, "UserStopped", 230 * 1024, 190 * 1024, 28 * 1024, 1800, 40, 0},
    {14, "ApiFailed", 170 * 1024, 140 * 1024, 18 * 1024, 500, 0, 0},
    {15, "Freezer", 310 * 1024, 270 * 1024, 45 * 1024, 4200, 20, 0},
    {16, "PackageStateChange", 200 * 1024, 160 * 1024, 24 * 1024, 600, 0, 0},
    {17, "PackageUpdated", 205 * 1024, 165 * 1024, 26 * 1024, 700, 0, 0},
    // Unmapped/fallback exit reason code -> "Other"
    {99, "Other", 290 * 1024, 235 * 1024, 39 * 1024, 1300, 5, kFlagForeground},
};

class CobaltPriorSessionMemoryAttributionBrowserTest
    : public content::ContentBrowserTest,
      public testing::WithParamInterface<PriorSessionAttributionTestParams> {};

IN_PROC_BROWSER_TEST_P(CobaltPriorSessionMemoryAttributionBrowserTest,
                       VerifyStateAttribution) {
  const PriorSessionAttributionTestParams& params = GetParam();
  base::HistogramTester histogram_tester;

  ProcessStateSnapshot snapshot;
  snapshot.peak_rss_kb = params.peak_rss_kb;
  snapshot.peak_pmf_kb = params.peak_pmf_kb;
  snapshot.peak_v8_code_kb = params.peak_v8_code_kb;
  snapshot.uptime_sec = params.uptime_sec;
  snapshot.last_trim_level = params.last_trim_level;
  snapshot.flags = params.flags;

  EmitPriorSessionExitSummaryHistograms(params.exit_reason, snapshot);

  std::string suffix = params.expected_suffix;
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB." + suffix, params.peak_rss_kb / 1024,
      1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB." + suffix, params.peak_pmf_kb / 1024,
      1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB." + suffix,
      params.peak_v8_code_kb / 1024, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeMinutes." + suffix,
      params.uptime_sec / 60, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.LastTrimLevel." + suffix,
      params.last_trim_level, 1);

  // Verify aggregate AllExits baseline for this exit
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.AllExits", params.peak_rss_kb / 1024,
      1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.AllExits", params.peak_pmf_kb / 1024,
      1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.AllExits",
      params.peak_v8_code_kb / 1024, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeMinutes.AllExits", params.uptime_sec / 60,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    AllExitReasonStates,
    CobaltPriorSessionMemoryAttributionBrowserTest,
    testing::ValuesIn(kPriorSessionAttributionTestCases),
    [](const testing::TestParamInfo<PriorSessionAttributionTestParams>& info) {
      return base::StringPrintf("%s_Code%d", info.param.expected_suffix,
                                info.param.exit_reason);
    });

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       PriorSessionMemoryAttributionHistograms) {
  base::HistogramTester cumulative_tester;

  int total_emitted = 0;
  for (const auto& test_case : kPriorSessionAttributionTestCases) {
    base::HistogramTester iteration_tester;

    ProcessStateSnapshot snapshot;
    snapshot.peak_rss_kb = test_case.peak_rss_kb;
    snapshot.peak_pmf_kb = test_case.peak_pmf_kb;
    snapshot.peak_v8_code_kb = test_case.peak_v8_code_kb;
    snapshot.uptime_sec = test_case.uptime_sec;
    snapshot.last_trim_level = test_case.last_trim_level;
    snapshot.flags = test_case.flags;

    EmitPriorSessionExitSummaryHistograms(test_case.exit_reason, snapshot);
    total_emitted++;

    std::string suffix = test_case.expected_suffix;
    iteration_tester.ExpectBucketCount(
        "Cobalt.Stability.Android.PeakRssMB." + suffix,
        test_case.peak_rss_kb / 1024, 1);
    iteration_tester.ExpectBucketCount(
        "Cobalt.Stability.Android.PeakPmfMB." + suffix,
        test_case.peak_pmf_kb / 1024, 1);
    iteration_tester.ExpectBucketCount(
        "Cobalt.Stability.Android.PeakV8CodeMB." + suffix,
        test_case.peak_v8_code_kb / 1024, 1);
    iteration_tester.ExpectBucketCount(
        "Cobalt.Stability.Android.UptimeMinutes." + suffix,
        test_case.uptime_sec / 60, 1);
    iteration_tester.ExpectBucketCount(
        "Cobalt.Stability.Android.LastTrimLevel." + suffix,
        test_case.last_trim_level, 1);
  }

  // Verify aggregate AllExits histograms contain all sequentially emitted
  // samples
  cumulative_tester.ExpectTotalCount(
      "Cobalt.Stability.Android.PeakRssMB.AllExits", total_emitted);
  cumulative_tester.ExpectTotalCount(
      "Cobalt.Stability.Android.PeakPmfMB.AllExits", total_emitted);
  cumulative_tester.ExpectTotalCount(
      "Cobalt.Stability.Android.PeakV8CodeMB.AllExits", total_emitted);
  cumulative_tester.ExpectTotalCount(
      "Cobalt.Stability.Android.UptimeMinutes.AllExits", total_emitted);
}

}  // namespace cobalt
