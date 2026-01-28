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
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace cobalt {

class CobaltMetricsBrowserTest : public content::ContentBrowserTest {
 public:
  CobaltMetricsBrowserTest() = default;
  ~CobaltMetricsBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       RecordMemoryTotalPrivateMemoryFootprint) {
  base::HistogramTester histogram_tester;

  auto* global_features = GlobalFeatures::GetInstance();
  global_features->metrics_local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, true);

  // GetMetricsService() will trigger creation of the client if reporting is
  // enabled.
  global_features->metrics_services_manager()->GetMetricsService();

  auto* client_manager = global_features->metrics_services_manager_client();
  ASSERT_TRUE(client_manager);
  CobaltMetricsServiceClient* client = static_cast<CobaltMetricsServiceClient*>(
      client_manager->metrics_service_client());
  ASSERT_TRUE(client);

  base::RunLoop run_loop;
  client->CollectFinalMetricsForLog(run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("Memory.Total.PrivateMemoryFootprint", 1);
}

}  // namespace cobalt
