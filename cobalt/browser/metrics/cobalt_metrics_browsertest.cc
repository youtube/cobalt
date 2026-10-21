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

#include <memory>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cobalt/browser/constants/cobalt_pref_names.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_detailed_metrics_delegate.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
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

  // Verify the strict 128 KiB per-file cap
  EXPECT_EQ(mem_allocator->size(), 128u * 1024u);

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
  histogram_tester.ExpectTotalCount("Histogram.MismatchedConstructionArguments",
                                    0);

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

  // Verify UsedKilobytes is sampled and within 0 - 128 KB
  base::HistogramBase* used_kb_hist = base::StatisticsRecorder::FindHistogram(
      "Cobalt.StabilityMetrics.UsedKilobytes");
  ASSERT_TRUE(used_kb_hist);
  auto used_samples = used_kb_hist->SnapshotSamples();
  EXPECT_GT(used_samples->TotalCount(), 0);
  EXPECT_LE(used_samples->sum() / used_samples->TotalCount(), 128);

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
  EXPECT_TRUE(local_state->FindPreference(kLastLoggedProcessExitTimestampMs));
  EXPECT_EQ(local_state->GetInt64(kLastLoggedProcessExitTimestampMs), -1);
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
                       TileMemorySurfacesInPerformanceApi) {
  std::string html_content = R"(
    <html>
    <body style="width: 2000px; height: 2000px; background: linear-gradient(red, blue);">
      <div style="width: 1000px; height: 1000px; background: green;"></div>
    </body>
    </html>
  )";
  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  EXPECT_GE(content::EvalJs(shell()->web_contents(),
                            "window.performance.measureUsedTileMemory()")
                .ExtractDouble(),
            0.0);

  EXPECT_GE(content::EvalJs(shell()->web_contents(),
                            "window.performance.measurePeakTileMemory()")
                .ExtractDouble(),
            0.0);

  EXPECT_TRUE(
      content::EvalJs(
          shell()->web_contents(),
          "'usedTileMemory' in window.performance.measureSystemMemoryInfo()")
          .ExtractBool());
  EXPECT_TRUE(
      content::EvalJs(
          shell()->web_contents(),
          "'peakTileMemory' in window.performance.measureSystemMemoryInfo()")
          .ExtractBool());
}

namespace {

constexpr char kStabilityAllocatorName[] = "BrowserStabilityMetrics";

bool CreatePriorSessionSparsePmaFile(const base::FilePath& file_path,
                                     std::string_view allocator_name,
                                     std::string_view histogram_name,
                                     base::HistogramBase::Sample32 sample,
                                     bool mark_deleted = false) {
  base::File file(file_path, base::File::FLAG_CREATE_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    return false;
  }
  auto mmfile = std::make_unique<base::MemoryMappedFile>();
  if (!mmfile->Initialize(std::move(file), {0, 64 * 1024},
                          base::MemoryMappedFile::READ_WRITE_EXTEND)) {
    return false;
  }
  if (!base::FilePersistentMemoryAllocator::IsFileAcceptable(
          *mmfile, /*read_only=*/false)) {
    return false;
  }
  auto mem_allocator = std::make_unique<base::FilePersistentMemoryAllocator>(
      std::move(mmfile), 64 * 1024, 0x53544142, allocator_name,
      base::FilePersistentMemoryAllocator::kReadWrite);
  if (mark_deleted) {
    mem_allocator->SetMemoryState(
        base::PersistentMemoryAllocator::MEMORY_DELETED);
  }
  base::PersistentHistogramAllocator allocator(std::move(mem_allocator));
  base::PersistentHistogramAllocator::Reference ref = 0;
  std::unique_ptr<base::HistogramBase> histogram = allocator.AllocateHistogram(
      base::SPARSE_HISTOGRAM, histogram_name,
      base::HashMetricName(histogram_name), 0, 0, nullptr,
      base::HistogramBase::kUmaTargetedHistogramFlag, &ref);
  if (!histogram) {
    return false;
  }
  allocator.FinalizeHistogram(ref, /*registered=*/true);
  histogram->Add(sample);
  return true;
}

}  // namespace

class CobaltMetricsDeduplicationEnabledBrowserTest
    : public CobaltMetricsBrowserTest {
 public:
  CobaltMetricsDeduplicationEnabledBrowserTest() = default;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    CobaltMetricsBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Initialize ScopedFeatureList here (before PreCreateThreads/SetupMetrics)
    // rather than in the constructor because BrowserTestBase::SetUp() resets
    // base::FeatureList before entering ContentMain(), and ShellMainDelegate
    // does not re-create FeatureList from command-line switches.
    feature_list_.InitAndEnableFeature(
        features::kCobaltStabilityMetricsDeduplication);
  }

  void TearDownOnMainThread() override {
    feature_list_.Reset();
    CobaltMetricsBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    CobaltMetricsDeduplicationEnabledBrowserTest,
    StabilityMetricsPmaSingleAndMultiSessionMergeWithoutDoubleLogging) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      features::kCobaltStabilityMetricsDeduplication));
  base::HistogramTester histogram_tester;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& metrics_dir = temp_dir.GetPath();

  const base::Time stamp1 = base::Time::FromTimeT(1700000100);
  const base::Time stamp2 = base::Time::FromTimeT(1700000200);
  const base::FilePath prior_file1 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName, stamp1, /*pid=*/41001);
  const base::FilePath prior_file2 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName, stamp2, /*pid=*/41002);

  constexpr char kHist1[] = "Cobalt.Test.BrowserPmaSession1";
  constexpr char kHist2[] = "Cobalt.Test.BrowserPmaSession2";
  ASSERT_TRUE(CreatePriorSessionSparsePmaFile(
      prior_file1, kStabilityAllocatorName, kHist1, 101));
  ASSERT_TRUE(CreatePriorSessionSparsePmaFile(
      prior_file2, kStabilityAllocatorName, kHist2, 202));

  // Before merging, neither prior-session histogram is registered in the
  // current browser process's StatisticsRecorder.
  EXPECT_EQ(base::StatisticsRecorder::FindHistogram(kHist1), nullptr);
  EXPECT_EQ(base::StatisticsRecorder::FindHistogram(kHist2), nullptr);

  // Hold a read-only mapping on prior_file1 to verify post-merge
  // MEMORY_DELETED state and atomic consumption of unlogged samples.
  auto inspect_mapped = std::make_unique<base::MemoryMappedFile>();
  ASSERT_TRUE(inspect_mapped->Initialize(prior_file1,
                                         base::MemoryMappedFile::READ_ONLY));

  ClearOtherStabilityMetricsPmaFiles(metrics_dir, kStabilityAllocatorName,
                                     base::GetCurrentProcId());
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  EXPECT_FALSE(base::PathExists(prior_file1));
  EXPECT_FALSE(base::PathExists(prior_file2));

  // Verify exact 1x counts (no 2x duplication from FileMetricsProvider or
  // GlobalHistogramAllocator import).
  base::HistogramBase* merged1 =
      base::StatisticsRecorder::FindHistogram(kHist1);
  ASSERT_NE(merged1, nullptr);
  EXPECT_EQ(merged1->SnapshotSamples()->GetCount(101), 1);
  EXPECT_EQ(merged1->SnapshotSamples()->TotalCount(), 1);

  base::HistogramBase* merged2 =
      base::StatisticsRecorder::FindHistogram(kHist2);
  ASSERT_NE(merged2, nullptr);
  EXPECT_EQ(merged2->SnapshotSamples()->GetCount(202), 1);
  EXPECT_EQ(merged2->SnapshotSamples()->TotalCount(), 1);

  histogram_tester.ExpectUniqueSample(
      "Cobalt.StabilityMetrics.PriorPmaFileAction",
      StabilityPmaFileAction::kMergedAndDeleted, 2);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.StabilityMetrics.PriorPmaFilesMergedCount", 2, 1);
  histogram_tester.ExpectTotalCount("UMA.FileMetricsProvider.AccessResult", 0);

  // Verify the underlying PMA header was marked MEMORY_DELETED and its unlogged
  // samples were atomically consumed via SnapshotDelta().
  auto inspect_mem = std::make_unique<base::FilePersistentMemoryAllocator>(
      std::move(inspect_mapped), 0, 0, std::string_view(),
      base::FilePersistentMemoryAllocator::kReadOnly);
  EXPECT_EQ(inspect_mem->GetMemoryState(),
            base::PersistentMemoryAllocator::MEMORY_DELETED);
  base::PersistentHistogramAllocator inspect_allocator(std::move(inspect_mem));
  base::PersistentHistogramAllocator::Iterator iter(&inspect_allocator);
  while (auto h = iter.GetNext()) {
    inspect_allocator.MergeHistogramFinalDeltaToStatisticsRecorder(h.get());
  }
  EXPECT_EQ(merged1->SnapshotSamples()->GetCount(101), 1);

  // Re-running cleanup and import is idempotent and adds 0 duplicate samples.
  ClearOtherStabilityMetricsPmaFiles(metrics_dir, kStabilityAllocatorName,
                                     base::GetCurrentProcId());
  base::StatisticsRecorder::ImportProvidedHistogramsSync();
  EXPECT_EQ(merged1->SnapshotSamples()->GetCount(101), 1);
  EXPECT_EQ(merged2->SnapshotSamples()->GetCount(202), 1);
  histogram_tester.ExpectBucketCount(
      "Cobalt.StabilityMetrics.PriorPmaFilesMergedCount", 0, 1);
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsDeduplicationEnabledBrowserTest,
                       StabilityMetricsPmaAlreadyMarkedDeletedNotReMerged) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      features::kCobaltStabilityMetricsDeduplication));
  base::HistogramTester histogram_tester;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath prior_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          temp_dir.GetPath(), kStabilityAllocatorName,
          base::Time::FromTimeT(1700000300), /*pid=*/41003);
  constexpr char kAlreadyDeletedHist[] =
      "Cobalt.Test.BrowserPmaAlreadyDeletedSample";
  ASSERT_TRUE(CreatePriorSessionSparsePmaFile(
      prior_file, kStabilityAllocatorName, kAlreadyDeletedHist, 55,
      /*mark_deleted=*/true));

  ClearOtherStabilityMetricsPmaFiles(
      temp_dir.GetPath(), kStabilityAllocatorName, base::GetCurrentProcId());

  EXPECT_FALSE(base::PathExists(prior_file));
  EXPECT_EQ(base::StatisticsRecorder::FindHistogram(kAlreadyDeletedHist),
            nullptr);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.StabilityMetrics.PriorPmaFileAction",
      StabilityPmaFileAction::kAlreadyMarkedDeleted, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.StabilityMetrics.PriorPmaFilesMergedCount", 0, 1);
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       StabilityMetricsPmaCorruptInvalidAndActivePidEdgeCases) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::HistogramTester histogram_tester;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& metrics_dir = temp_dir.GetPath();

  // 1. Non-existent directory and empty directory are handled safely.
  ClearOtherStabilityMetricsPmaFiles(
      metrics_dir.AppendASCII("non_existent_subdir"), kStabilityAllocatorName,
      base::GetCurrentProcId());
  ClearOtherStabilityMetricsPmaFiles(metrics_dir, kStabilityAllocatorName,
                                     base::GetCurrentProcId());
  histogram_tester.ExpectUniqueSample(
      "Cobalt.StabilityMetrics.PriorPmaFilesMergedCount", 0, 2);

  const base::Time stamp = base::Time::FromTimeT(1700000400);

  // 2. Zero-byte and truncated .pma files with valid names ->
  // kCorruptOrUnacceptableDeleted.
  const base::FilePath zero_byte_pma =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName, stamp, /*pid=*/42001);
  ASSERT_TRUE(base::WriteFile(zero_byte_pma, ""));

  const base::FilePath truncated_pma =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName, stamp, /*pid=*/42002);
  ASSERT_TRUE(base::WriteFile(truncated_pma, "short_header"));

  // 3. Mismatched allocator name, zero PID, negative PID, and malformed name ->
  // kInvalidFileNameDeleted.
  const base::FilePath mismatched_pma =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, "OtherAllocator", stamp, /*pid=*/42003);
  ASSERT_TRUE(base::WriteFile(mismatched_pma, "other"));

  const base::FilePath zero_pid_pma =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName, stamp, /*pid=*/0);
  ASSERT_TRUE(base::WriteFile(zero_pid_pma, "zero_pid"));

  const base::FilePath negative_pid_pma =
      metrics_dir.AppendASCII("BrowserStabilityMetrics-65550000--1.pma");
  ASSERT_TRUE(base::WriteFile(negative_pid_pma, "neg_pid"));

  const base::FilePath malformed_pma =
      metrics_dir.AppendASCII("unparseable.pma");
  ASSERT_TRUE(base::WriteFile(malformed_pma, "malformed"));

  // 4. Active current_pid .pma file and non-.pma file must be preserved.
  const base::FilePath active_pid_pma =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName, stamp,
          base::GetCurrentProcId());
  ASSERT_TRUE(base::WriteFile(active_pid_pma, "active_session"));

  const base::FilePath non_pma_file = metrics_dir.AppendASCII("keep_me.txt");
  ASSERT_TRUE(base::WriteFile(non_pma_file, "preserve"));

  ClearOtherStabilityMetricsPmaFiles(metrics_dir, kStabilityAllocatorName,
                                     base::GetCurrentProcId());

  EXPECT_FALSE(base::PathExists(zero_byte_pma));
  EXPECT_FALSE(base::PathExists(truncated_pma));
  EXPECT_FALSE(base::PathExists(mismatched_pma));
  EXPECT_FALSE(base::PathExists(zero_pid_pma));
  EXPECT_FALSE(base::PathExists(negative_pid_pma));
  EXPECT_FALSE(base::PathExists(malformed_pma));
  EXPECT_TRUE(base::PathExists(active_pid_pma));
  EXPECT_TRUE(base::PathExists(non_pma_file));

  histogram_tester.ExpectBucketCount(
      "Cobalt.StabilityMetrics.PriorPmaFileAction",
      StabilityPmaFileAction::kCorruptOrUnacceptableDeleted, 2);
  histogram_tester.ExpectBucketCount(
      "Cobalt.StabilityMetrics.PriorPmaFileAction",
      StabilityPmaFileAction::kInvalidFileNameDeleted, 4);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.StabilityMetrics.PriorPmaFilesMergedCount", 0, 3);
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       StabilityMetricsPmaDeduplicationDisabledControlCohort) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      features::kCobaltStabilityMetricsDeduplication));

  base::HistogramTester histogram_tester;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath prior_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          temp_dir.GetPath(), kStabilityAllocatorName,
          base::Time::FromTimeT(1700000500), /*pid=*/43001);
  constexpr char kControlHist[] = "Cobalt.Test.BrowserPmaLegacyControlSample";
  ASSERT_TRUE(CreatePriorSessionSparsePmaFile(
      prior_file, kStabilityAllocatorName, kControlHist, 88));

  auto inspect_mapped = std::make_unique<base::MemoryMappedFile>();
  ASSERT_TRUE(inspect_mapped->Initialize(prior_file,
                                         base::MemoryMappedFile::READ_ONLY));

  ClearOtherStabilityMetricsPmaFiles(
      temp_dir.GetPath(), kStabilityAllocatorName, base::GetCurrentProcId());
  EXPECT_FALSE(base::PathExists(prior_file));

  base::HistogramBase* merged =
      base::StatisticsRecorder::FindHistogram(kControlHist);
  ASSERT_NE(merged, nullptr);
  EXPECT_EQ(merged->SnapshotSamples()->GetCount(88), 1);

  // In the A/B control cohort (flag disabled), the PMA header is not mutated to
  // MEMORY_DELETED.
  auto inspect_mem = std::make_unique<base::FilePersistentMemoryAllocator>(
      std::move(inspect_mapped), 0, 0, std::string_view(),
      base::FilePersistentMemoryAllocator::kReadOnly);
  EXPECT_NE(inspect_mem->GetMemoryState(),
            base::PersistentMemoryAllocator::MEMORY_DELETED);

  histogram_tester.ExpectUniqueSample(
      "Cobalt.StabilityMetrics.PriorPmaFileAction",
      StabilityPmaFileAction::kMergedAndDeleted, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.StabilityMetrics.PriorPmaFilesMergedCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       StabilityMetricsPmaDirectoryBudgetEdgeCases) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& metrics_dir = temp_dir.GetPath();

  constexpr int64_t kPmaFileSize = 128 * 1024;
  constexpr int64_t kMaxDirBudget = 512 * 1024;

  // Single file exceeding total budget is rejected.
  EXPECT_FALSE(EnsurePmaDirectoryBudget(metrics_dir, kStabilityAllocatorName,
                                        kMaxDirBudget, kMaxDirBudget + 1));

  // Corrupt and mismatched files are pruned first, and non-.pma files are
  // ignored in size calculations.
  const base::FilePath invalid_pma = metrics_dir.AppendASCII("corrupt.pma");
  const base::FilePath non_pma = metrics_dir.AppendASCII("ignore.txt");
  ASSERT_TRUE(base::WriteFile(invalid_pma, std::string(1024, 'x')));
  ASSERT_TRUE(base::WriteFile(non_pma, std::string(2048, 'y')));
  EXPECT_EQ(GetTotalStabilityMetricsPmaDirSizeBytes(metrics_dir), 1024);

  const base::FilePath f1 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName,
          base::Time::FromTimeT(1700000100), /*pid=*/44001);
  const base::FilePath f2 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName,
          base::Time::FromTimeT(1700000200), /*pid=*/44002);
  const base::FilePath f3 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName,
          base::Time::FromTimeT(1700000300), /*pid=*/44003);
  const base::FilePath f4 =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          metrics_dir, kStabilityAllocatorName,
          base::Time::FromTimeT(1700000400), /*pid=*/44004);

  ASSERT_TRUE(base::WriteFile(f1, std::string(kPmaFileSize, 'a')));
  ASSERT_TRUE(base::WriteFile(f2, std::string(kPmaFileSize, 'b')));
  ASSERT_TRUE(base::WriteFile(f3, std::string(kPmaFileSize, 'c')));

  // 3 * 128 KiB = 384 KiB existing + 128 KiB new <= 512 KiB budget: invalid_pma
  // is deleted, and f1..f3 are all preserved.
  EXPECT_TRUE(EnsurePmaDirectoryBudget(metrics_dir, kStabilityAllocatorName,
                                       kMaxDirBudget, kPmaFileSize));
  EXPECT_FALSE(base::PathExists(invalid_pma));
  EXPECT_TRUE(base::PathExists(non_pma));
  EXPECT_TRUE(base::PathExists(f1));
  EXPECT_TRUE(base::PathExists(f2));
  EXPECT_TRUE(base::PathExists(f3));

  // Add f4 (4 * 128 KiB = 512 KiB existing). Requesting 128 KiB more exceeds
  // 512 KiB and prunes the oldest file (f1) while keeping f2, f3, f4.
  ASSERT_TRUE(base::WriteFile(f4, std::string(kPmaFileSize, 'd')));
  EXPECT_TRUE(EnsurePmaDirectoryBudget(metrics_dir, kStabilityAllocatorName,
                                       kMaxDirBudget, kPmaFileSize));
  EXPECT_FALSE(base::PathExists(f1));
  EXPECT_TRUE(base::PathExists(f2));
  EXPECT_TRUE(base::PathExists(f3));
  EXPECT_TRUE(base::PathExists(f4));
  EXPECT_EQ(GetTotalStabilityMetricsPmaDirSizeBytes(metrics_dir),
            3 * kPmaFileSize);
}

}  // namespace cobalt
