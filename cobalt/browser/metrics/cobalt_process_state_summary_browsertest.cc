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

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"
#include "cobalt/browser/metrics/cobalt_stability_metrics_helper.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

class CobaltProcessStateSummaryBrowserTest
    : public content::ContentBrowserTest {
 public:
  CobaltProcessStateSummaryBrowserTest() = default;
  ~CobaltProcessStateSummaryBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(CobaltProcessStateSummaryBrowserTest,
                       RecordsPriorSessionEarlyStartupLowMemoryExit) {
  base::HistogramTester histogram_tester;

  ProcessStateSnapshot snapshot;
  snapshot.pid = 1234;
  snapshot.peak_rss_kb = 450 * 1024;     // 450 MB
  snapshot.peak_pmf_kb = 380 * 1024;     // 380 MB
  snapshot.peak_v8_code_kb = 65 * 1024;  // 65 MB
  snapshot.uptime_sec = 25;              // 25 seconds
  snapshot.last_trim_level = 80;         // TRIM_MEMORY_COMPLETE
  snapshot.flags = kFlagForeground | kFlagStartupGuardArmed;
  snapshot.startup_milestones = (1ULL << 0) | (1ULL << 5);
  snapshot.highest_milestone = 5;

  // Test LowMemory exit (enum value 7)
  EmitPriorSessionExitSummaryHistograms(7, snapshot);

  // Verify suffix-specific histograms
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.LowMemory", 450, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.LowMemory", 380, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.LowMemory", 65, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeMinutes.LowMemory", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.LastTrimLevel.LowMemory", 80, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardArmed.LowMemory", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.HighestMilestone.LowMemory", 5, 1);

  // EarlyStartup specific breakdown
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.EarlyStartup.LowMemory", 450, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.EarlyStartup.LowMemory", 380, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.EarlyStartup.LowMemory", 65, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeSeconds.EarlyStartup.LowMemory", 25, 1);

  // AllExits aggregate baseline
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.AllExits", 450, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.AllExits", 380, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.AllExits", 65, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeMinutes.AllExits", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.LastTrimLevel.AllExits", 80, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardArmed.AllExits", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.HighestMilestone.AllExits", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.EarlyStartup.AllExits", 450, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.EarlyStartup.AllExits", 380, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.EarlyStartup.AllExits", 65, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeSeconds.EarlyStartup.AllExits", 25, 1);
}

IN_PROC_BROWSER_TEST_F(CobaltProcessStateSummaryBrowserTest,
                       RecordsStartupGuardWatchdogKilledExits) {
  base::HistogramTester histogram_tester;

  ProcessStateSnapshot snapshot;
  snapshot.pid = 5678;
  snapshot.peak_rss_kb = 320 * 1024;     // 320 MB
  snapshot.peak_pmf_kb = 290 * 1024;     // 290 MB
  snapshot.peak_v8_code_kb = 80 * 1024;  // 80 MB
  snapshot.uptime_sec = 120;             // 120 seconds
  snapshot.last_trim_level = 0;
  snapshot.flags = kFlagStartupGuardArmed | kFlagStartupGuardTriggeredKill;
  snapshot.startup_milestones = (1ULL << 0) | (1ULL << 1) | (1ULL << 2);
  snapshot.highest_milestone = 2;

  // OS reports exit_reason = Crash (1) because Java runtime threw
  // RuntimeException
  EmitPriorSessionExitSummaryHistograms(1, snapshot);

  // Verify watchdog killed histograms
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardWatchdogKilled.HighestMilestone", 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardWatchdogKilled.PeakV8CodeMB", 80,
      1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardWatchdogKilled.PeakRssMB", 320, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardWatchdogKilled.PeakPmfMB", 290, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardWatchdogKilled.UptimeSeconds", 120,
      1);

  // Must NOT emit organic Crash or AllExits histograms
  histogram_tester.ExpectTotalCount("Cobalt.Stability.Android.PeakRssMB.Crash",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Stability.Android.PeakRssMB.AllExits", 0);
}

IN_PROC_BROWSER_TEST_F(CobaltProcessStateSummaryBrowserTest,
                       RecordsPostStartupNormalExit) {
  base::HistogramTester histogram_tester;

  ProcessStateSnapshot snapshot;
  snapshot.pid = 9999;
  snapshot.peak_rss_kb = 500 * 1024;     // 500 MB
  snapshot.peak_pmf_kb = 420 * 1024;     // 420 MB
  snapshot.peak_v8_code_kb = 40 * 1024;  // 40 MB
  snapshot.uptime_sec = 7200;            // 120 minutes
  snapshot.last_trim_level = 15;         // TRIM_MEMORY_RUNNING_CRITICAL
  snapshot.flags =
      kFlagForeground | kFlagMediaPlaying;  // StartupGuard disarmed
  snapshot.startup_milestones = (1ULL << 35) - 1;
  snapshot.highest_milestone = 34;

  // UserRequested / normal exit (enum value 12)
  EmitPriorSessionExitSummaryHistograms(12, snapshot);

  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.UserRequested", 500, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.UserRequested", 420, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.UserRequested", 40, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeMinutes.UserRequested", 120, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardArmed.UserRequested", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.HighestMilestone.UserRequested", 34, 1);

  // EarlyStartup histograms must NOT be emitted
  histogram_tester.ExpectTotalCount(
      "Cobalt.Stability.Android.PeakRssMB.EarlyStartup.UserRequested", 0);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Stability.Android.PeakRssMB.EarlyStartup.AllExits", 0);
}

IN_PROC_BROWSER_TEST_F(CobaltProcessStateSummaryBrowserTest,
                       RecordsPriorSessionOrganicNativeCrashExit) {
  base::HistogramTester histogram_tester;

  ProcessStateSnapshot snapshot;
  snapshot.pid = 4321;
  snapshot.peak_rss_kb = 350 * 1024;     // 350 MB
  snapshot.peak_pmf_kb = 300 * 1024;     // 300 MB
  snapshot.peak_v8_code_kb = 50 * 1024;  // 50 MB
  snapshot.uptime_sec = 18;              // 18 seconds
  snapshot.last_trim_level = 0;
  snapshot.flags =
      kFlagForeground | kFlagStartupGuardArmed;  // Early startup organic crash
  snapshot.startup_milestones = (1ULL << 0) | (1ULL << 1);
  snapshot.highest_milestone = 1;

  // Native crash (exit reason 2: kCrashNative)
  EmitPriorSessionExitSummaryHistograms(2, snapshot);

  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.CrashNative", 350, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.CrashNative", 300, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.CrashNative", 50, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeMinutes.CrashNative", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.StartupGuardArmed.CrashNative", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.HighestMilestone.CrashNative", 1, 1);

  // EarlyStartup specific breakdown
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakRssMB.EarlyStartup.CrashNative", 350, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakPmfMB.EarlyStartup.CrashNative", 300, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.PeakV8CodeMB.EarlyStartup.CrashNative", 50, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Stability.Android.UptimeSeconds.EarlyStartup.CrashNative", 18, 1);

  // Watchdog histograms must NOT be emitted for organic native crash
  histogram_tester.ExpectTotalCount(
      "Cobalt.Stability.Android.StartupGuardWatchdogKilled.PeakRssMB", 0);
}

}  // namespace
}  // namespace cobalt
