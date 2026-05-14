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

#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/browser/features.h"
#include "cobalt/memory/cobalt_memory_attribution_manager.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

class MemoryAttributionBrowserTest : public content::ContentBrowserTest {
 public:
  MemoryAttributionBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kCobaltMemoryAttributionManager);
  }
  ~MemoryAttributionBrowserTest() override = default;

 protected:
  void TriggerReportUma() {
    base::RunLoop run_loop;
    memory::CobaltMemoryAttributionManager::Get()->RequestReportUmaForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MemoryAttributionBrowserTest, RecordsAttributedMemory) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Execute JavaScript to trigger significant DOM/V8 memory allocation.
  EXPECT_TRUE(content::EvalJs(
                  shell()->web_contents(),
                  "var a = new Array(100000); "
                  "a.fill('cobalt_memory_test_string_to_take_up_space'); true;")
                  .ExtractBool());

  // Force reporting UMA.
  TriggerReportUma();

  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  auto check_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    if (!histogram) {
      return false;
    }
    auto samples = histogram->SnapshotSamples();
    if (samples->TotalCount() == 0) {
      return false;
    }
    EXPECT_LT(samples->GetCount(0), samples->TotalCount());
    return true;
  };

  EXPECT_TRUE(check_histogram("Memory.Cobalt.AllocationVolume.DOM"));
  EXPECT_TRUE(check_histogram("Memory.Cobalt.AllocationVolume.Layout"));
  EXPECT_TRUE(check_histogram("Memory.Cobalt.AllocationVolume.Script"));
  EXPECT_TRUE(check_histogram("Memory.Cobalt.AllocationVolume.Network"));
  EXPECT_TRUE(check_histogram("Memory.Cobalt.AllocationVolume.Graphics"));
}

}  // namespace cobalt
