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

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/task/single_thread_task_runner.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "base/time/time.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/functional/bind.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace cobalt {

class MemoryMetricsBrowserTest : public content::ContentBrowserTest {
 public:
  MemoryMetricsBrowserTest() = default;
  ~MemoryMetricsBrowserTest() override = default;

  void OnMemoryDumpReceived(
      base::OnceClosure quit,
      const std::string& scenario_name,
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
    ASSERT_TRUE(success);
    ASSERT_TRUE(dump);
    
    uint64_t private_footprint_kb = 0;
    uint64_t peak_resident_set_kb = 0;
    uint64_t v8_heap_size_kb = 0;

    for (const auto& process_dump : dump->process_dumps()) {
      // os memory dump
      private_footprint_kb += process_dump.os_dump().private_footprint_kb;
      peak_resident_set_kb += process_dump.os_dump().peak_resident_set_kb;
      
      // allocator dump
      std::optional<uint64_t> v8_size = process_dump.GetMetric("v8", "size");
      if (v8_size.has_value()) {
        v8_heap_size_kb += v8_size.value();
      }
    }
    
    LOG(INFO) << ">>> Cobalt Private Memory Footprint (" << scenario_name << "): "
              << private_footprint_kb << " KB";
    LOG(INFO) << ">>> Cobalt Peak Resident Set (" << scenario_name << "): "
              << peak_resident_set_kb << " KB";
    LOG(INFO) << ">>> Cobalt V8 Heap Size (" << scenario_name << "): "
              << v8_heap_size_kb / 1024 << " KB";
        
    std::move(quit).Run();
  }
};

IN_PROC_BROWSER_TEST_F(MemoryMetricsBrowserTest, BlankPageMemoryFootprint) {
  base::HistogramTester histogram_tester;

  // Navigate to a blank page
  EXPECT_TRUE(content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  // Gather metrics from memory dump
  base::RunLoop run_loop;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(
          {"v8"}, base::BindOnce(&MemoryMetricsBrowserTest::OnMemoryDumpReceived,
                             base::Unretained(this), run_loop.QuitClosure(),
                             "Blank Page"));
  run_loop.Run();

  // Verify and log GPU Peak Memory
  histogram_tester.ExpectTotalCount("Memory.GPU.PeakMemoryUsage2.PageLoad", 1);
  std::vector<base::Bucket> buckets = histogram_tester.GetAllSamples("Memory.GPU.PeakMemoryUsage2.PageLoad");
  if (!buckets.empty()) {
    LOG(INFO) << ">>> Cobalt GPU Peak Memory (Blank Page): " << buckets[0].min << " MB";
    LOG(INFO) << ">>> Cobalt GPU Peak Memory Bucket (Blank Page): [" << buckets[0].min << ", " << buckets[0].count << "]";
  }
}

IN_PROC_BROWSER_TEST_F(MemoryMetricsBrowserTest, YouTubeHomepageMemoryFootprint) {
  base::HistogramTester histogram_tester;

  // Start the embedded test server
  embedded_test_server()->ServeFilesFromSourceDirectory("cobalt/testing/browser_tests/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to the mock YouTube homepage
  GURL mock_homepage_url = embedded_test_server()->GetURL("/mock_homepage_tv.html");

  EXPECT_TRUE(content::NavigateToURL(shell()->web_contents(), mock_homepage_url));

  // Wait for a few seconds for the app to load and stabilize
  base::RunLoop run_loop_delay;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop_delay.QuitClosure(), base::Seconds(5));
  run_loop_delay.Run();

  // Gather metrics from memory dump
  base::RunLoop run_loop;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(
          {"v8"}, base::BindOnce(&MemoryMetricsBrowserTest::OnMemoryDumpReceived,
                             base::Unretained(this), run_loop.QuitClosure(),
                             "YouTube Homepage"));
  run_loop.Run();

  // Verify and log GPU Peak Memory
  histogram_tester.ExpectTotalCount("Memory.GPU.PeakMemoryUsage2.PageLoad", 1);
  std::vector<base::Bucket> buckets = histogram_tester.GetAllSamples("Memory.GPU.PeakMemoryUsage2.PageLoad");
  if (!buckets.empty()) {
    LOG(INFO) << ">>> Cobalt GPU Peak Memory (YouTube Homepage): " << buckets[0].min << " MB";
    LOG(INFO) << ">>> Cobalt GPU Peak Memory Bucket (YouTube Homepage): [" << buckets[0].min << ", " << buckets[0].count << "]";
  }
}

IN_PROC_BROWSER_TEST_F(MemoryMetricsBrowserTest, Video1080pPlaybackMemoryFootprint) {
  base::HistogramTester histogram_tester;

  // Start the embedded test server
  embedded_test_server()->ServeFilesFromSourceDirectory("cobalt/testing/browser_tests/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to the 1080p playback test page
  GURL play_1080p_url = embedded_test_server()->GetURL("/play_1080p.html");
  EXPECT_TRUE(content::NavigateToURL(shell()->web_contents(), play_1080p_url));

  // Wait for a few seconds for the video to decode and play
  base::RunLoop run_loop_delay;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop_delay.QuitClosure(), base::Seconds(7));
  run_loop_delay.Run();

  // Gather metrics from memory dump
  base::RunLoop run_loop;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(
          {"v8"}, base::BindOnce(&MemoryMetricsBrowserTest::OnMemoryDumpReceived,
                             base::Unretained(this), run_loop.QuitClosure(),
                             "1080p Video Playback"));
  run_loop.Run();

  // Verify and log GPU Peak Memory
  histogram_tester.ExpectTotalCount("Memory.GPU.PeakMemoryUsage2.PageLoad", 1);
  std::vector<base::Bucket> buckets = histogram_tester.GetAllSamples("Memory.GPU.PeakMemoryUsage2.PageLoad");
  if (!buckets.empty()) {
    LOG(INFO) << ">>> Cobalt GPU Peak Memory (1080p Playback): " << buckets[0].min << " MB";
    LOG(INFO) << ">>> Cobalt GPU Peak Memory Bucket (1080p Playback): [" << buckets[0].min << ", " << buckets[0].count << "]";
  }
}


}  // namespace cobalt
