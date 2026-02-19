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

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance {

class PerformanceBrowserTest : public content::ContentBrowserTest {
 public:
  PerformanceBrowserTest() = default;
  ~PerformanceBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PerformanceBrowserTest, RequestGlobalMemoryDump) {
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(),
                                     GURL("h5vcc-embedded://splash")));

  // Wait a bit for the system to settle.
  base::RunLoop run_loop_wait;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop_wait.QuitClosure(), base::Seconds(3));
  run_loop_wait.Run();

  base::RunLoop run_loop;
  cobalt::CobaltMetricsServiceClient* client = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto* features = cobalt::GlobalFeatures::GetInstance();
    // Ensure metrics recording is started.
    features->metrics_services_manager()->UpdateUploadPermissions(true);

    auto* manager_client = features->metrics_services_manager_client();
    ASSERT_TRUE(manager_client);
    client = manager_client->metrics_service_client();
  }
  ASSERT_TRUE(client);
  client->ScheduleRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  content::EvalJsResult eval_result =
      content::EvalJs(shell()->web_contents(),
                      "performance.requestGlobalMemoryDump().catch(e => { "
                      "  return 'REJECTED: ' + e; "
                      "})",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                      /*world_id=*/content::ISOLATED_WORLD_ID_GLOBAL,
                      /*after_script_invoke=*/base::DoNothing());

  ASSERT_TRUE(eval_result.value.is_string())
      << "Result is not a string: " << eval_result.value;
  std::string result_str = eval_result.value.GetString();
  ASSERT_FALSE(base::StartsWith(result_str, "REJECTED:")) << result_str;
  EXPECT_FALSE(result_str.empty());

  // Verify it contains process_dumps
  EXPECT_NE(result_str.find("process_dumps"), std::string::npos);
  EXPECT_NE(result_str.find("resident_set_kb"), std::string::npos);
  EXPECT_NE(result_str.find("gpu_memory_kb"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PerformanceBrowserTest, H5vccPerformance) {
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(),
                                     GURL("h5vcc-embedded://splash")));

  // Wait for h5vcc to be available if needed
  base::Value h5vcc_exists =
      content::EvalJs(shell()->web_contents(), "typeof h5vcc !== 'undefined'")
          .value.Clone();
  if (!h5vcc_exists.GetBool()) {
    GTEST_SKIP()
        << "h5vcc not defined on this page, skipping H5vccPerformance test.";
  }

  // Wait a bit for the system to settle.
  base::RunLoop run_loop_wait;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop_wait.QuitClosure(), base::Seconds(3));
  run_loop_wait.Run();

  base::RunLoop run_loop;
  cobalt::CobaltMetricsServiceClient* client = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto* features = cobalt::GlobalFeatures::GetInstance();
    // Ensure metrics recording is started.
    features->metrics_services_manager()->UpdateUploadPermissions(true);

    auto* manager_client = features->metrics_services_manager_client();
    ASSERT_TRUE(manager_client);
    client = manager_client->metrics_service_client();
  }
  ASSERT_TRUE(client);
  client->ScheduleRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  content::EvalJsResult eval_result = content::EvalJs(
      shell()->web_contents(),
      "h5vcc.performance.requestGlobalMemoryDump().catch(e => { "
      "  return 'REJECTED: ' + e; "
      "})",
      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
      /*world_id=*/content::ISOLATED_WORLD_ID_GLOBAL,
      /*after_script_invoke=*/base::DoNothing());

  ASSERT_TRUE(eval_result.value.is_string())
      << "Result is not a string: " << eval_result.value;
  std::string result_str = eval_result.value.GetString();
  ASSERT_FALSE(base::StartsWith(result_str, "REJECTED:")) << result_str;
  EXPECT_FALSE(result_str.empty());
}

}  // namespace performance
