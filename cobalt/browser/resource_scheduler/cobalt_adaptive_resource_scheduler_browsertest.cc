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

#include "cobalt/browser/resource_scheduler/cobalt_adaptive_resource_scheduler.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cobalt/browser/resource_scheduler/cobalt_resource_throttle.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace cobalt {

class CobaltAdaptiveResourceSchedulerBrowserTest
    : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Reset scheduler state to idle.
    CobaltAdaptiveResourceScheduler::GetInstance()->SetScrollState(false);
    content::ContentBrowserTest::TearDownOnMainThread();
  }
};

// 1. Verifies that critical document and script requests bypass throttling.
IN_PROC_BROWSER_TEST_F(CobaltAdaptiveResourceSchedulerBrowserTest,
                       CriticalResourcesBypassThrottlingDuringScroll) {
  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  scheduler->SetScrollState(true);
  EXPECT_TRUE(scheduler->IsScrolling());

  // Top-level document navigations must not be blocked by active scrolling.
  GURL url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(content::NavigateToURL(shell(), url));
  EXPECT_EQ(scheduler->GetDeferredCount(), 0u);
}

// 2. Verifies that user interaction transitions scheduler to SCROLLING state
//    and automatically drains upon settle debounce timeout.
IN_PROC_BROWSER_TEST_F(CobaltAdaptiveResourceSchedulerBrowserTest,
                       InteractionTriggersScrollingAndSettles) {
  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  scheduler->SetSettleDelayForTesting(base::Milliseconds(100));

  GURL url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(content::NavigateToURL(shell(), url));

  // Trigger D-Pad down interaction.
  scheduler->OnUserInteraction(ui::VKEY_DOWN);
  EXPECT_TRUE(scheduler->IsScrolling());

  // Simulate a deferrable thumbnail throttle while scrolling.
  network::ResourceRequest img_request;
  img_request.destination = network::mojom::RequestDestination::kImage;
  img_request.priority = net::RequestPriority::LOW;

  auto throttle = CobaltResourceThrottle::MaybeCreate(img_request);
  ASSERT_NE(throttle, nullptr);

  bool defer = false;
  throttle->WillStartRequest(&img_request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(throttle->is_deferred());
  EXPECT_EQ(scheduler->GetDeferredCount(), 1u);

  // Wait for settle debounce timer (100ms + margin).
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(150));
  run_loop.Run();

  // Verify scheduler transitioned to IDLE and queue drained.
  EXPECT_FALSE(scheduler->IsScrolling());
  EXPECT_FALSE(throttle->is_deferred());
  EXPECT_EQ(scheduler->GetDeferredCount(), 0u);
}

// 3. Verifies that client cancellation (e.g. scrolling past thumbnail) cleanly
//    destroys the throttle and unregisters from the scheduler.
IN_PROC_BROWSER_TEST_F(CobaltAdaptiveResourceSchedulerBrowserTest,
                       DeferredThrottleCleansUpOnCancellation) {
  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  scheduler->SetScrollState(true);

  network::ResourceRequest img_request;
  img_request.destination = network::mojom::RequestDestination::kImage;
  img_request.priority = net::RequestPriority::LOW;

  {
    auto throttle = CobaltResourceThrottle::MaybeCreate(img_request);
    ASSERT_NE(throttle, nullptr);

    bool defer = false;
    throttle->WillStartRequest(&img_request, &defer);
    EXPECT_TRUE(defer);
    EXPECT_EQ(scheduler->GetDeferredCount(), 1u);
    // Destroy throttle (simulating element removal / load cancellation).
  }

  // Destruction must automatically remove the throttle from scheduler queue.
  EXPECT_EQ(scheduler->GetDeferredCount(), 0u);
}

}  // namespace cobalt
