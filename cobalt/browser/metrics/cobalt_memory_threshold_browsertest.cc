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
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
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
    ASSERT_TRUE(success);
    ASSERT_TRUE(dump);

    uint64_t private_footprint_kb = 0;
    uint64_t peak_resident_set_kb = 0;
    uint64_t v8_heap_size_kb = 0;

    for (const auto& process_dump : dump->process_dumps()) {
      // OS memory dump
      private_footprint_kb += process_dump.os_dump().private_footprint_kb;
      peak_resident_set_kb += process_dump.os_dump().peak_resident_set_kb;

      // Allocator dump
      std::optional<uint64_t> v8_size = process_dump.GetMetric("v8", "size");
      if (v8_size.has_value()) {
        v8_heap_size_kb += v8_size.value() / 1024;
      }
    }

    testing::Test::RecordProperty("private_footprint_kb", private_footprint_kb);
    testing::Test::RecordProperty("peak_resident_set_kb", peak_resident_set_kb);
    testing::Test::RecordProperty("v8_heap_size_kb", v8_heap_size_kb);
    // print to stdout
    perf_test::PrintResult("PrivateFootprint", "", scenario_name,
                           static_cast<size_t>(private_footprint_kb), "kb",
                           true);
    perf_test::PrintResult("PeakResidentSet", "", scenario_name,
                           static_cast<size_t>(peak_resident_set_kb), "kb",
                           true);
    perf_test::PrintResult("V8HeapSize", "", scenario_name, 
                           static_cast<size_t>(v8_heap_size_kb), "kb", true);
 
  }

  void CollectMemoryMetrics(base::HistogramTester& histogram_tester,
                            const std::string& scenario_name) {
    base::RunLoop run_loop;

    // Gather metrics from memory dump.
    memory_instrumentation::MemoryInstrumentation::GetInstance()
        ->RequestGlobalDump(
            {"v8"},
            base::BindOnce(
                &CobaltMemoryThresholdBrowserTest::OnMemoryDumpReceived,
                base::Unretained(this), run_loop.QuitClosure(), scenario_name));
    run_loop.Run();

    // Verify and log GPU Peak Memory from UMA.
    std::vector<base::Bucket> buckets =
        histogram_tester.GetAllSamples("Memory.GPU.PeakMemoryUsage2.PageLoad");
    if (!buckets.empty()) {
      uint64_t gpu_peak_memory_page_load = buckets[0].min;
      testing::Test::RecordProperty("gpu_peak_memory_on_page_load_mb",
                                    gpu_peak_memory_page_load);
      perf_test::PrintResult("GpuPeakMemoryOnPageLoad", "", scenario_name,
                             static_cast<size_t>(gpu_peak_memory_page_load),
                             "mb", true);
    }
  }
};

IN_PROC_BROWSER_TEST_P(CobaltMemoryThresholdBrowserTest,
                       BlankPageMemoryFootprint) {
  base::HistogramTester histogram_tester;

  // Navigate to a blank page.
  EXPECT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  // Wait for a few seconds for metrics to be recorded.
  base::RunLoop run_loop_delay;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop_delay.QuitClosure(), base::Seconds(5));
  run_loop_delay.Run();

  CollectMemoryMetrics(histogram_tester, "BlankPage");
}

IN_PROC_BROWSER_TEST_P(CobaltMemoryThresholdBrowserTest,
                       YouTubeHomepageMemoryFootprint) {
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

  // Wait for a few seconds for the page to load and stabilize.
  base::RunLoop run_loop_delay;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop_delay.QuitClosure(), base::Seconds(5));
  run_loop_delay.Run();

  CollectMemoryMetrics(histogram_tester, "YouTubeHomepage");
}

// Instantiate suite to repeat test run for five times.
INSTANTIATE_TEST_SUITE_P(All,
                         CobaltMemoryThresholdBrowserTest,
                         testing::Values(1, 2, 3, 4, 5));

}  // namespace cobalt
