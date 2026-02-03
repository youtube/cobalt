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
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "cobalt/browser/switches.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class CobaltMetricsBrowserTest : public content::ContentBrowserTest {
 public:
  CobaltMetricsBrowserTest() = default;
  ~CobaltMetricsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    // Set a short interval for memory metrics to verify periodic recording.
    command_line->AppendSwitchASCII(switches::kMemoryMetricsInterval, "1");
  }
};

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest, RecordsMemoryMetrics) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  // Ensure metrics recording is started.
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = manager_client->metrics_service_client();
  ASSERT_TRUE(client);

  // Trigger a memory dump manually for testing.
  base::RunLoop run_loop;
  static_cast<CobaltMetricsServiceClient*>(client)->ScheduleRecordForTesting(
      run_loop.QuitClosure());
  run_loop.Run();

  // We expect at least one sample in the memory histograms.
  // The exact value depends on the environment, but it should be > 0.
  // Note: on some environments it might take a bit longer for the dump to
  // be processed by the service, but RunUntilIdle should cover it.
  base::RunLoop().RunUntilIdle();

  EXPECT_GE(
      histogram_tester.GetAllSamples("Memory.Total.PrivateMemoryFootprint")
          .size(),
      1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Memory.Total.Resident").size(), 1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Memory.Browser.Resident").size(),
            1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Cobalt.Memory.JavaScript").size(),
            1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Cobalt.Memory.DOM").size(), 1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Cobalt.Memory.Native").size(), 1u);
  // Layout, Graphics and Media may not be present depending on the page content
  // and platform.
}

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest, PeriodicRecordsMemoryMetrics) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  // Ensure metrics recording is started.
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  // Wait for the periodic dump (interval is 1s).
  // We wait 3 seconds to be safe and account for any scheduling delays.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(3));
  run_loop.Run();

  // We expect at least one sample from the periodic collection.
  base::RunLoop().RunUntilIdle();
  // We don't care about the exact value, just that it's been recorded.
  EXPECT_GE(
      histogram_tester.GetAllSamples("Memory.Total.PrivateMemoryFootprint")
          .size(),
      1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Memory.Total.Resident").size(), 1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Memory.Browser.Resident").size(),
            1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Cobalt.Memory.JavaScript").size(),
            1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Cobalt.Memory.DOM").size(), 1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Cobalt.Memory.Native").size(), 1u);
  // Layout, Graphics and Media may not be present depending on the page content
  // and platform.
}

}  // namespace cobalt
