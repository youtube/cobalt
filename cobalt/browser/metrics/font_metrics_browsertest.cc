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
#include "base/threading/thread_restrictions.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "cobalt/browser/switches.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class FontMetricsBrowserTest : public content::ContentBrowserTest {
 public:
  FontMetricsBrowserTest() = default;
  ~FontMetricsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    // Set a short interval for memory metrics to verify periodic recording.
    command_line->AppendSwitchASCII(switches::kMemoryMetricsInterval, "1");
  }
};

IN_PROC_BROWSER_TEST_F(FontMetricsBrowserTest, RecordsFontHistograms) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();

  // Ensure metrics recording is started.
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  // Load a page with various characters to exercise font caches and fallback in
  // Blink.
  std::string html_content = R"(
    <html>
    <body>
      <p>Standard Latin text.</p>
      <p>Emoji: 😀 😃 😄 😁 😆 😅 😂 🤣 🦩 🦒 🦚 🦝</p>
      <p>Rare CJK: 𠜎 𠜱 𠝹 𠱓 𠱸 𠲖 𠳏 𠳕</p>
    </body>
    </html>
  )";

  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // Trigger a memory dump manually for testing.
  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = static_cast<CobaltMetricsServiceClient*>(
      manager_client->metrics_service_client());
  ASSERT_TRUE(client);

  base::RunLoop run_loop;
  client->ScheduleRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  base::RunLoop().RunUntilIdle();

  // Check for memory histograms (Skia Glyph Cache).
  EXPECT_GE(
      histogram_tester.GetAllSamples("Memory.Browser2.PrivateMemoryFootprint")
          .size(),
      1u);

  EXPECT_GE(histogram_tester
                .GetAllSamples("Memory.Experimental.Browser2.Skia.SkGlyphCache")
                .size(),
            1u);

  EXPECT_GE(
      histogram_tester.GetAllSamples("Memory.Experimental.Browser2.FontCaches")
          .size(),
      1u);
}

}  // namespace cobalt
