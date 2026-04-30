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
    for (const auto& process_dump : dump->process_dumps()) {
     private_footprint_kb += process_dump.os_dump().private_footprint_kb;
    }
    
    LOG(INFO) << ">>> Cobalt Private Memory Footprint (" << scenario_name << "): "
              << private_footprint_kb << " KB";
    
    EXPECT_GT(private_footprint_kb, 0u);
    
    std::move(quit).Run();
  }
};

IN_PROC_BROWSER_TEST_F(MemoryMetricsBrowserTest, BlankPagePrivateFootprint) {
  // Navigate to a blank page
  EXPECT_TRUE(content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  // Gather the metric
  base::RunLoop run_loop;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(
          {}, base::BindOnce(&MemoryMetricsBrowserTest::OnMemoryDumpReceived,
                             base::Unretained(this), run_loop.QuitClosure(), "Blank Page"));
  
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(MemoryMetricsBrowserTest, YouTubeTVPrivateFootprint) {
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

  // Gather the metric
  base::RunLoop run_loop;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(
          {}, base::BindOnce(&MemoryMetricsBrowserTest::OnMemoryDumpReceived,
                             base::Unretained(this), run_loop.QuitClosure(), "YouTube TV"));
  
  run_loop.Run();
}

}  // namespace cobalt
