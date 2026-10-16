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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cobalt {

class CobaltMemoryThresholdBrowserTest
    : public content::ContentBrowserTest,
      public testing::WithParamInterface<int> {
 public:
  CobaltMemoryThresholdBrowserTest() = default;
  ~CobaltMemoryThresholdBrowserTest() override = default;

  void OnMemoryDumpReceived(
      base::OnceClosure quit,
      const std::string& scenario_name,
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
    // ScopedClosureRunner guarantees quit.Run() is called when this function
    // ends, no matter how it exits (failure, etc.)
    base::ScopedClosureRunner quit_runner(std::move(quit));
    if (!success || !dump) {
      ADD_FAILURE() << "Failed to receive memory dump.";
      return;
    }

    uint64_t private_footprint_kb = 0;
    uint64_t peak_resident_set_kb = 0;

    for (const auto& process_dump : dump->process_dumps()) {
      private_footprint_kb += process_dump.os_dump().private_footprint_kb;
      peak_resident_set_kb += process_dump.os_dump().peak_resident_set_kb;
    }

    testing::Test::RecordProperty("private_footprint_kb", private_footprint_kb);
    testing::Test::RecordProperty("peak_resident_set_kb", peak_resident_set_kb);
    // still print to stdout
    perf_test::PrintResult("PrivateFootprint", "", scenario_name,
                           static_cast<size_t>(private_footprint_kb), "kb",
                           true);
    perf_test::PrintResult("PeakResidentSet", "", scenario_name,
                           static_cast<size_t>(peak_resident_set_kb), "kb",
                           true);
  }

  void WaitForHistogram(base::HistogramTester& histogram_tester,
                        const std::string& histogram_name) {
    base::RunLoop run_loop;
    // Listen using a histogram observer to avoid flakiness in test
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](std::string_view histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample32 sample) { run_loop.Quit(); }));
    // Only block if histogram has not yet been recorded.
    if (histogram_tester.GetAllSamples(histogram_name).empty()) {
      run_loop.Run();
    }
  }

  void CollectMemoryMetrics(base::HistogramTester& histogram_tester,
                            const std::string& scenario_name) {
    base::RunLoop run_loop;

    memory_instrumentation::MemoryInstrumentation::GetInstance()
        ->RequestGlobalDump(
            {},
            base::BindOnce(
                &CobaltMemoryThresholdBrowserTest::OnMemoryDumpReceived,
                base::Unretained(this), run_loop.QuitClosure(), scenario_name));
    run_loop.Run();

    // Verify and log GPU Peak Memory from UMA.
    WaitForHistogram(histogram_tester, "Memory.GPU.PeakMemoryUsage2.PageLoad");
    std::vector<base::Bucket> buckets =
        histogram_tester.GetAllSamples("Memory.GPU.PeakMemoryUsage2.PageLoad");
    ASSERT_TRUE(!buckets.empty());
    uint64_t gpu_peak_memory_page_load = buckets[0].min;
    testing::Test::RecordProperty("gpu_peak_memory_on_page_load_mb",
                                  gpu_peak_memory_page_load);
    perf_test::PrintResult("GpuPeakMemoryOnPageLoad", "", scenario_name,
                           static_cast<size_t>(gpu_peak_memory_page_load), "mb",
                           true);
  }
};

IN_PROC_BROWSER_TEST_P(CobaltMemoryThresholdBrowserTest,
                       BlankPageMemoryFootprint) {
  base::HistogramTester histogram_tester;

  // Navigate to a blank page.
  EXPECT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  CollectMemoryMetrics(histogram_tester, "BlankPage");
}

IN_PROC_BROWSER_TEST_P(CobaltMemoryThresholdBrowserTest,
                       MockYouTubeHomepageMemoryFootprint) {
  base::HistogramTester histogram_tester;

  // Start the embedded test server.
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "cobalt/testing/browser_tests/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to the mock YouTube homepage.
  GURL mock_homepage_url =
      embedded_test_server()->GetURL("/mock_homepage_tv.html");

  EXPECT_TRUE(
      content::NavigateToURL(shell()->web_contents(), mock_homepage_url));

  CollectMemoryMetrics(histogram_tester, "YouTubeHomepage");
}

// Repeat five times to denoise metrics and reduce variation
INSTANTIATE_TEST_SUITE_P(All,
                         CobaltMemoryThresholdBrowserTest,
                         testing::Values(1, 2, 3, 4, 5));

}  // namespace cobalt
