// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/memory/cobalt_memory_context.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "cobalt/memory/cobalt_memory_attribution_manager.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace cobalt {
namespace memory {

class CobaltMemoryAttributionBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    manager_ = CobaltMemoryAttributionManager::Get();
  }

  void ForceGarbageCollection() {
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  }

 protected:
  CobaltMemoryAttributionManager* manager_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(CobaltMemoryAttributionBrowserTest,
                       ActionBasedAuditingAndZeroUnattributed) {
  ASSERT_TRUE(manager_);

  // E2E Flow: App Start and Navigate to a blank page as baseline.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(), embedded_test_server()->GetURL("/empty.html")));

  // Wait for things to settle
  ForceGarbageCollection();

  // The 'Unattributed' component is represented by MemoryContext::kUnknown.
  // We want to ensure it remains strictly bounded. Let's assert it's 0 or
  // at least very small (e.g., < 100KB) if some unavoidable OS allocations
  // occur.
  uint64_t unattributed_baseline =
      manager_->GetResidentBytes(base::memory::MemoryContext::kUnknown);
  EXPECT_LE(unattributed_baseline, 100ULL * 1024);  // Strictly bounded.

  // Simulate navigating to a heavy local page.
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(), embedded_test_server()->GetURL("/title1.html")));

  // Evaluate memory while on page. Unattributed should still be bounded.
  uint64_t unattributed_during =
      manager_->GetResidentBytes(base::memory::MemoryContext::kUnknown);
  EXPECT_LE(unattributed_during, 100ULL * 1024);

  // Leak Detection: Navigate back to baseline.
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(), embedded_test_server()->GetURL("/empty.html")));
  ForceGarbageCollection();

  // Validate per-component resident memory drops back to near baseline.
  // We will check kDOM as a primary example. We use a polling loop as GC and
  // memory reclaiming might not be synchronous.
  bool memory_dropped = false;
  for (int i = 0; i < 50; ++i) {
    if (manager_->GetResidentBytes(base::memory::MemoryContext::kDOM) <=
        1 * 1024 * 1024) {
      memory_dropped = true;
      break;
    }
    base::RunLoop().RunUntilIdle();
    base::PlatformThread::Sleep(base::Milliseconds(100));
    ForceGarbageCollection();  // Retrigger if needed.
  }
  EXPECT_TRUE(memory_dropped) << "DOM memory did not drop below 1MB after GC";
}

}  // namespace memory
}  // namespace cobalt
