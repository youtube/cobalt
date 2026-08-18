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

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/browser/performance/performance_impl.h"
#include "cobalt/build/configs/buildflags.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace cobalt {

class PerformanceExtensionsBrowserTest : public content::ContentBrowserTest {
 public:
  PerformanceExtensionsBrowserTest() = default;
  ~PerformanceExtensionsBrowserTest() override {
    performance::PerformanceImpl::SetProcStatusDataForTesting(nullptr);
  }
};

class PerformanceExtensionsBrowserTestDisabled
    : public PerformanceExtensionsBrowserTest {
 public:
  PerformanceExtensionsBrowserTestDisabled() {
    scoped_feature_list_.InitAndDisableFeature(blink::features::kCobaltPeakRss);
  }
  ~PerformanceExtensionsBrowserTestDisabled() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class PerformanceExtensionsBrowserTestNoBackoff
    : public PerformanceExtensionsBrowserTest {
 public:
  PerformanceExtensionsBrowserTestNoBackoff() {
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kCobaltPeakRssBackoff);
  }
  ~PerformanceExtensionsBrowserTestNoBackoff() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PerformanceExtensionsBrowserTestNoBackoff,
                       MeasureRssHighWaterMarkMemory) {
  double expected_kb = 120560.0;
  std::string mock_data = base::StringPrintf(
      "Name:\tcobalt\n"
      "Umask:\t0002\n"
      "State:\tS (sleeping)\n"
      "VmHWM:\t   %d kB\n"
      "VmRSS:\t   100560 kB\n",
      static_cast<int>(expected_kb));
  performance::PerformanceImpl::SetProcStatusDataForTesting(&mock_data);

  ASSERT_TRUE(content::NavigateToURL(shell(), GURL("about:blank")));

  // Verify that the JS API extracts the precise VmHWM evaluation correctly.
  std::string script = R"(
    (async function() {
      if (typeof performance.measureRssHighWaterMarkMemory === 'undefined') {
        return -1;
      }
      try {
        let result = await performance.measureRssHighWaterMarkMemory();
        if (typeof result === 'string' && result === 'API not supported') {
          return -2;
        }
        return result;
      } catch(e) {
        return -3;
      }
    })()
  )";

  EXPECT_EQ(expected_kb * 1024.0,
            content::EvalJs(shell()->web_contents(), script).ExtractDouble());
}

IN_PROC_BROWSER_TEST_F(PerformanceExtensionsBrowserTestDisabled,
                       MeasureRssHighWaterMarkMemoryDisabled) {
  std::string mock_data =
      "Name:\tcobalt\n"
      "Umask:\t0002\n"
      "State:\tS (sleeping)\n"
      "VmHWM:\t   120560 kB\n"
      "VmRSS:\t   100560 kB\n";
  performance::PerformanceImpl::SetProcStatusDataForTesting(&mock_data);

  ASSERT_TRUE(content::NavigateToURL(shell(), GURL("about:blank")));

  // Verify that when the Finch flag CobaltPeakRss is disabled, the API
  // throws/rejects immediately and does not evaluate memory.
  std::string script = R"(
    (async function() {
      if (typeof performance.measureRssHighWaterMarkMemory === 'undefined') {
        return -1;
      }
      try {
        let result = await performance.measureRssHighWaterMarkMemory();
        return result;
      } catch(e) {
        if (e.message.includes('API not supported')) {
          return -2;
        }
        return -3;
      }
    })()
  )";

  EXPECT_EQ(-2.0,
            content::EvalJs(shell()->web_contents(), script).ExtractDouble());
}

IN_PROC_BROWSER_TEST_F(PerformanceExtensionsBrowserTest,
                       MeasureRssHighWaterMarkMemoryBackoff) {
  double expected_kb = 120560.0;
  std::string mock_data = base::StringPrintf(
      "Name:\tcobalt\n"
      "Umask:\t0002\n"
      "State:\tS (sleeping)\n"
      "VmHWM:\t   %d kB\n"
      "VmRSS:\t   100560 kB\n",
      static_cast<int>(expected_kb));
  performance::PerformanceImpl::SetProcStatusDataForTesting(&mock_data);

  ASSERT_TRUE(content::NavigateToURL(shell(), GURL("about:blank")));

  // Verify that calling it twice in a row rapidly triggers the backoff block
  std::string script = R"(
    (async function() {
      await performance.measureRssHighWaterMarkMemory();
      try {
        await performance.measureRssHighWaterMarkMemory();
        return -1;
      } catch(e) {
        if (e.message.includes('API not supported - rate limited')) {
          return -2;
        }
        return -3;
      }
    })()
  )";

  EXPECT_EQ(-2.0,
            content::EvalJs(shell()->web_contents(), script).ExtractDouble());
}

}  // namespace cobalt
