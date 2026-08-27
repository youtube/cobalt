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

#include "third_party/blink/renderer/controller/highest_pmf_reporter.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/renderer/render_thread_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/controller/memory_usage_monitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// Needs to be precisely in blink namespace to match friend declaration
class MockHighestPmfReporter : public HighestPmfReporter {
 public:
  MockHighestPmfReporter(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
      const base::TickClock* clock)
      : HighestPmfReporter(task_runner, clock) {}

  void ForceFirstNavigationStarted() { first_navigation_started_ = true; }

 protected:
  bool FirstNavigationStarted() override {
    if (first_navigation_started_) {
      if (already_returned_true_) {
        return false;
      }
      already_returned_true_ = true;
      return true;
    }
    return false;
  }

 private:
  bool first_navigation_started_ = false;
  bool already_returned_true_ = false;
};

// Minimal mock to fake MemoryUsageMonitor that reporter attaches to
class MockMemoryUsageMonitor : public MemoryUsageMonitor {
 public:
  MockMemoryUsageMonitor(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
      const base::TickClock* clock)
      : MemoryUsageMonitor(task_runner, clock) {}

  MemoryUsage GetCurrentMemoryUsage() override { return usage_; }

  MemoryUsage usage_;
};

}  // namespace blink

namespace cobalt {
namespace metrics {

class HighestPmfReporterBrowserTest : public content::ContentBrowserTest {
 public:
  HighestPmfReporterBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kHighestPmfReporterConfigurable,
        {{"intervals", "1,2"}, {"metric_suffixes", "1minTest,2minTest"}});
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

    // Tests run in the browser process; WTF partitions are not automatically
    // initialized.
    WTF::Partitions::Initialize();
    WTF::Initialize();

    memory_usage_monitor_ = std::make_unique<blink::MockMemoryUsageMonitor>(
        test_task_runner_, test_task_runner_->GetMockTickClock());
    blink::MemoryUsageMonitor::SetInstanceForTesting(
        memory_usage_monitor_.get());
    reporter_ = std::make_unique<blink::MockHighestPmfReporter>(
        test_task_runner_, test_task_runner_->GetMockTickClock());
  }

  void TearDownOnMainThread() override {
    reporter_.reset();
    blink::MemoryUsageMonitor::SetInstanceForTesting(nullptr);
    memory_usage_monitor_.reset();
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  std::unique_ptr<blink::MockMemoryUsageMonitor> memory_usage_monitor_;
  std::unique_ptr<blink::MockHighestPmfReporter> reporter_;
};

#if (BUILDFLAG(IS_STARBOARD) || BUILDFLAG(IS_APPLE)) && \
    !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
#define MAYBE_ReportMetric DISABLED_ReportMetric
#define MAYBE_ReportMetricForeground DISABLED_ReportMetricForeground
#define MAYBE_ReportMetricForegroundWithLowerOrFlatMemory \
  DISABLED_ReportMetricForegroundWithLowerOrFlatMemory
#define MAYBE_BackgroundCancelsInFlightForegroundReporting \
  DISABLED_BackgroundCancelsInFlightForegroundReporting
#define MAYBE_NoForegroundMetricWithoutForegroundTransition \
  DISABLED_NoForegroundMetricWithoutForegroundTransition
#define MAYBE_NoForegroundMetricWhenOnlyBackgrounded \
  DISABLED_NoForegroundMetricWhenOnlyBackgrounded
#define MAYBE_RenderThreadStateTransitionForeground \
  DISABLED_RenderThreadStateTransitionForeground
#else
#define MAYBE_ReportMetric ReportMetric
#define MAYBE_ReportMetricForeground ReportMetricForeground
#define MAYBE_ReportMetricForegroundWithLowerOrFlatMemory \
  ReportMetricForegroundWithLowerOrFlatMemory
#define MAYBE_BackgroundCancelsInFlightForegroundReporting \
  BackgroundCancelsInFlightForegroundReporting
#define MAYBE_NoForegroundMetricWithoutForegroundTransition \
  NoForegroundMetricWithoutForegroundTransition
#define MAYBE_NoForegroundMetricWhenOnlyBackgrounded \
  NoForegroundMetricWhenOnlyBackgrounded
#define MAYBE_RenderThreadStateTransitionForeground \
  RenderThreadStateTransitionForeground
#endif

IN_PROC_BROWSER_TEST_F(HighestPmfReporterBrowserTest, MAYBE_ReportMetric) {
  base::HistogramTester histogram_tester;

  reporter_->ForceFirstNavigationStarted();
  memory_usage_monitor_->usage_.private_footprint_bytes =
      1000.0 * 1024.0 * 1024.0;
  memory_usage_monitor_->usage_.peak_resident_bytes = 1000.0 * 1024.0 * 1024.0;

  // Fast forward by 1 second to let MemoryUsageMonitor's internal timer fire
  // OnMemoryPing
  test_task_runner_->FastForwardBy(base::Seconds(1));

  // Fast forward by 2 minutes and 2 seconds.
  // If parameter override succeeds, both 1minTest and 2minTest buckets trigger.
  // If the override fails (e.g. single-process early caching), the 0to2min
  // baseline bucket triggers.
  test_task_runner_->FastForwardBy(base::Minutes(2) + base::Seconds(2));

  auto samples_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.1minTest");
  auto samples_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min");

  // At least one of the config models must successfully register an
  // initialization ping.
  EXPECT_FALSE(samples_override.empty() && samples_baseline.empty());
  if (!samples_override.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.1minTest",
        1000, 1);
  }
  if (!samples_baseline.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min",
        1000, 1);
  }

  auto rss_samples_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "PeakResidentSet.AtHighestPrivateMemoryFootprint.1minTest");
  auto rss_samples_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "PeakResidentSet.AtHighestPrivateMemoryFootprint.0to2min");

  EXPECT_FALSE(rss_samples_override.empty() && rss_samples_baseline.empty());
  if (!rss_samples_override.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprint.1minTest",
        1000, 1);
  }
  if (!rss_samples_baseline.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprint.0to2min",
        1000, 1);
  }

  // Verify negative: foreground metrics MUST NOT fire during standard startup
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "HighestPrivateMemoryFootprintWhenForegrounded.1minTest")
                  .empty());
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "HighestPrivateMemoryFootprintWhenForegrounded.0to2min")
                  .empty());
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples(
              "Memory.Experimental.Renderer."
              "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
              "1minTest")
          .empty());
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "PeakResidentSet."
                      "AtHighestPrivateMemoryFootprintWhenForegrounded.0to2min")
                  .empty());
}

IN_PROC_BROWSER_TEST_F(HighestPmfReporterBrowserTest,
                       MAYBE_ReportMetricForeground) {
  base::HistogramTester histogram_tester;

  blink::OnProcessBackgrounded();
  base::RunLoop().RunUntilIdle();

  blink::OnProcessForegrounded();
  base::RunLoop().RunUntilIdle();

  memory_usage_monitor_->usage_.private_footprint_bytes =
      2000.0 * 1024.0 * 1024.0;
  memory_usage_monitor_->usage_.peak_resident_bytes = 2000.0 * 1024.0 * 1024.0;

  test_task_runner_->FastForwardBy(base::Seconds(1));
  test_task_runner_->FastForwardBy(base::Minutes(2) + base::Seconds(2));

  auto samples_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.1minTest");
  auto samples_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.0to2min");

  EXPECT_FALSE(samples_override.empty() && samples_baseline.empty());
  if (!samples_override.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "HighestPrivateMemoryFootprintWhenForegrounded.1minTest",
        2000, 1);
  }
  if (!samples_baseline.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "HighestPrivateMemoryFootprintWhenForegrounded.0to2min",
        2000, 1);
  }

  auto rss_samples_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
      "1minTest");
  auto rss_samples_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
      "0to2min");

  EXPECT_FALSE(rss_samples_override.empty() && rss_samples_baseline.empty());
  if (!rss_samples_override.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
        "1minTest",
        2000, 1);
  }
  if (!rss_samples_baseline.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
        "0to2min",
        2000, 1);
  }

  // Verify negative: standard startup baseline metrics MUST NOT fire when
  // foreground-measuring
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples("Memory.Experimental.Renderer."
                                 "HighestPrivateMemoryFootprint.1minTest")
                  .empty());
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples("Memory.Experimental.Renderer."
                                 "HighestPrivateMemoryFootprint.0to2min")
                  .empty());
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples(
              "Memory.Experimental.Renderer."
              "PeakResidentSet.AtHighestPrivateMemoryFootprint.1minTest")
          .empty());
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "PeakResidentSet.AtHighestPrivateMemoryFootprint.0to2min")
                  .empty());
}

IN_PROC_BROWSER_TEST_F(HighestPmfReporterBrowserTest,
                       MAYBE_ReportMetricForegroundWithLowerOrFlatMemory) {
  base::HistogramTester histogram_tester;

  // 1. Initial startup with high memory usage
  reporter_->ForceFirstNavigationStarted();
  memory_usage_monitor_->usage_.private_footprint_bytes =
      3000.0 * 1024.0 * 1024.0;
  memory_usage_monitor_->usage_.peak_resident_bytes = 3500.0 * 1024.0 * 1024.0;
  test_task_runner_->FastForwardBy(base::Seconds(1));

  // 2. Transition from FG -> BG -> FG
  blink::OnProcessBackgrounded();
  base::RunLoop().RunUntilIdle();

  blink::OnProcessForegrounded();
  base::RunLoop().RunUntilIdle();

  // 3. In the new foreground session, memory usage is LOWER than the previous
  // peak.
  memory_usage_monitor_->usage_.private_footprint_bytes =
      500.0 * 1024.0 * 1024.0;
  memory_usage_monitor_->usage_.peak_resident_bytes = 800.0 * 1024.0 * 1024.0;

  // Fast-forward 1 sec for ping, then through the reporting window.
  test_task_runner_->FastForwardBy(base::Seconds(1));
  test_task_runner_->FastForwardBy(base::Minutes(2) + base::Seconds(2));

  // Verify that the foreground metrics fire with the NEW session's exact values
  // (500 MB PMF, 800 MB RSS) and NOT the old startup values (3000 MB PMF, 3500
  // MB RSS).
  auto fg_pmf_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.1minTest");
  auto fg_pmf_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.0to2min");

  EXPECT_FALSE(fg_pmf_override.empty() && fg_pmf_baseline.empty());
  if (!fg_pmf_override.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "HighestPrivateMemoryFootprintWhenForegrounded.1minTest",
        500, 1);
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "HighestPrivateMemoryFootprintWhenForegrounded.1minTest",
        3000, 0);
  }
  if (!fg_pmf_baseline.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "HighestPrivateMemoryFootprintWhenForegrounded.0to2min",
        500, 1);
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "HighestPrivateMemoryFootprintWhenForegrounded.0to2min",
        3000, 0);
  }

  auto fg_rss_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
      "1minTest");
  auto fg_rss_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
      "0to2min");

  EXPECT_FALSE(fg_rss_override.empty() && fg_rss_baseline.empty());
  if (!fg_rss_override.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
        "1minTest",
        800, 1);
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
        "1minTest",
        3500, 0);
  }
  if (!fg_rss_baseline.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
        "0to2min",
        800, 1);
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
        "0to2min",
        3500, 0);
  }
}

IN_PROC_BROWSER_TEST_F(HighestPmfReporterBrowserTest,
                       MAYBE_BackgroundCancelsInFlightForegroundReporting) {
  base::HistogramTester histogram_tester;

  // 1. Transition to foreground
  blink::OnProcessForegrounded();
  base::RunLoop().RunUntilIdle();

  memory_usage_monitor_->usage_.private_footprint_bytes =
      1200.0 * 1024.0 * 1024.0;
  memory_usage_monitor_->usage_.peak_resident_bytes = 1500.0 * 1024.0 * 1024.0;
  test_task_runner_->FastForwardBy(base::Seconds(1));

  // 2. Advance 30 seconds (before the 1min/2min reporting interval fires)
  test_task_runner_->FastForwardBy(base::Seconds(30));

  // 3. App goes to background -> cancels in-flight timer and stops monitor
  // observation
  blink::OnProcessBackgrounded();
  base::RunLoop().RunUntilIdle();

  // 4. Advance time past original interval while in background
  test_task_runner_->FastForwardBy(base::Minutes(3));

  // 5. Verify NO foreground samples were reported during background
  auto fg_samples_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.1minTest");
  auto fg_samples_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.0to2min");
  EXPECT_TRUE(fg_samples_override.empty() && fg_samples_baseline.empty());

  // 6. Resuming foreground restarts the timer cleanly
  blink::OnProcessForegrounded();
  base::RunLoop().RunUntilIdle();

  test_task_runner_->FastForwardBy(base::Seconds(1));
  test_task_runner_->FastForwardBy(base::Minutes(2) + base::Seconds(2));

  fg_samples_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.1minTest");
  fg_samples_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.0to2min");
  EXPECT_FALSE(fg_samples_override.empty() && fg_samples_baseline.empty());
}

IN_PROC_BROWSER_TEST_F(HighestPmfReporterBrowserTest,
                       MAYBE_NoForegroundMetricWithoutForegroundTransition) {
  base::HistogramTester histogram_tester;

  // Standard startup: First navigation started, process never receives
  // OnProcessForegrounded
  reporter_->ForceFirstNavigationStarted();
  memory_usage_monitor_->usage_.private_footprint_bytes =
      1000.0 * 1024.0 * 1024.0;
  memory_usage_monitor_->usage_.peak_resident_bytes = 1200.0 * 1024.0 * 1024.0;

  test_task_runner_->FastForwardBy(base::Seconds(1));
  test_task_runner_->FastForwardBy(base::Minutes(5));

  // Verify baseline metrics DID fire
  auto baseline_pmf_1min = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.1minTest");
  auto baseline_pmf_0to2min = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min");
  EXPECT_FALSE(baseline_pmf_1min.empty() && baseline_pmf_0to2min.empty());

  // Verify NEGATIVE: WhenForegrounded metrics NEVER fired
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "HighestPrivateMemoryFootprintWhenForegrounded.1minTest")
                  .empty());
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "HighestPrivateMemoryFootprintWhenForegrounded.0to2min")
                  .empty());
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples(
              "Memory.Experimental.Renderer."
              "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
              "1minTest")
          .empty());
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "PeakResidentSet."
                      "AtHighestPrivateMemoryFootprintWhenForegrounded.0to2min")
                  .empty());
}

IN_PROC_BROWSER_TEST_F(HighestPmfReporterBrowserTest,
                       MAYBE_NoForegroundMetricWhenOnlyBackgrounded) {
  base::HistogramTester histogram_tester;

  // Process transitions to background immediately
  blink::OnProcessBackgrounded();
  base::RunLoop().RunUntilIdle();

  memory_usage_monitor_->usage_.private_footprint_bytes =
      1000.0 * 1024.0 * 1024.0;
  memory_usage_monitor_->usage_.peak_resident_bytes = 1200.0 * 1024.0 * 1024.0;

  // Advance time extensively while remaining backgrounded
  test_task_runner_->FastForwardBy(base::Minutes(10));

  // Verify NEGATIVE: ZERO foreground metrics recorded
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "HighestPrivateMemoryFootprintWhenForegrounded.1minTest")
                  .empty());
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "HighestPrivateMemoryFootprintWhenForegrounded.0to2min")
                  .empty());
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples(
              "Memory.Experimental.Renderer."
              "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
              "1minTest")
          .empty());
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples(
                      "Memory.Experimental.Renderer."
                      "PeakResidentSet."
                      "AtHighestPrivateMemoryFootprintWhenForegrounded.0to2min")
                  .empty());
}

IN_PROC_BROWSER_TEST_F(HighestPmfReporterBrowserTest,
                       MAYBE_RenderThreadStateTransitionForeground) {
  content::RenderThreadImpl* render_thread =
      content::RenderThreadImpl::current();
  if (!render_thread) {
    return;
  }

  content::mojom::Renderer* renderer = render_thread;
  base::HistogramTester histogram_tester;

  // 1. Transition render thread to background via Mojo Renderer interface
  renderer->SetProcessState(base::Process::Priority::kBestEffort,
                            content::mojom::RenderProcessVisibleState::kHidden);
  base::RunLoop().RunUntilIdle();

  // 2. Transition render thread to foreground (calls OnRendererForegrounded ->
  // blink::OnProcessForegrounded)
  renderer->SetProcessState(
      base::Process::Priority::kUserBlocking,
      content::mojom::RenderProcessVisibleState::kVisible);
  base::RunLoop().RunUntilIdle();

  // 3. Simulate memory ping and advance clock
  memory_usage_monitor_->usage_.private_footprint_bytes =
      1500.0 * 1024.0 * 1024.0;
  memory_usage_monitor_->usage_.peak_resident_bytes = 1800.0 * 1024.0 * 1024.0;

  test_task_runner_->FastForwardBy(base::Seconds(1));
  test_task_runner_->FastForwardBy(base::Minutes(2) + base::Seconds(2));

  // 4. Verify that HighestPmfReporter captured the foreground memory peak
  // through the RenderThreadImpl transition!
  auto samples_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.1minTest");
  auto samples_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "HighestPrivateMemoryFootprintWhenForegrounded.0to2min");

  EXPECT_FALSE(samples_override.empty() && samples_baseline.empty());
  if (!samples_override.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "HighestPrivateMemoryFootprintWhenForegrounded.1minTest",
        1500, 1);
  }
  if (!samples_baseline.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "HighestPrivateMemoryFootprintWhenForegrounded.0to2min",
        1500, 1);
  }

  auto rss_samples_override = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
      "1minTest");
  auto rss_samples_baseline = histogram_tester.GetAllSamples(
      "Memory.Experimental.Renderer."
      "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
      "0to2min");

  EXPECT_FALSE(rss_samples_override.empty() && rss_samples_baseline.empty());
  if (!rss_samples_override.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
        "1minTest",
        1800, 1);
  }
  if (!rss_samples_baseline.empty()) {
    histogram_tester.ExpectBucketCount(
        "Memory.Experimental.Renderer."
        "PeakResidentSet.AtHighestPrivateMemoryFootprintWhenForegrounded."
        "0to2min",
        1800, 1);
  }
}

}  // namespace metrics
}  // namespace cobalt
