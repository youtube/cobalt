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

#include "base/values.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

  base::Value result = content::EvalJs(shell()->web_contents(),
                                       "performance.requestGlobalMemoryDump()")
                           .value.Clone();
  ASSERT_TRUE(result.is_string());
  std::string json = result.GetString();
  EXPECT_FALSE(json.empty());

  // Verify it contains process_dumps
  EXPECT_NE(json.find("process_dumps"), std::string::npos);
  EXPECT_NE(json.find("resident_set_kb"), std::string::npos);
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

  base::Value result =
      content::EvalJs(shell()->web_contents(),
                      "h5vcc.performance.requestGlobalMemoryDump()")
          .value.Clone();
  ASSERT_TRUE(result.is_string());
  std::string json = result.GetString();
  EXPECT_FALSE(json.empty());
}

}  // namespace performance
