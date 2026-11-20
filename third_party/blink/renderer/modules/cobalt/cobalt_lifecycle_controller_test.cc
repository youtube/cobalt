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

#include "third_party/blink/renderer/modules/cobalt/cobalt_lifecycle_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "cobalt/browser/lifecycle/public/mojom/cobalt_lifecycle.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace {

using ::testing::ElementsAre;

// Test-only Mojo receiver recording browser-bound CobaltLifecycleObserver
// notifications in arrival order.
//
// Lifetime and ownership: Stack-allocated per test case.
// Threading: Used exclusively on the Blink main test thread.
class FakeCobaltLifecycleObserver
    : public cobalt::mojom::blink::CobaltLifecycleObserver {
 public:
  FakeCobaltLifecycleObserver() = default;
  ~FakeCobaltLifecycleObserver() override = default;

  mojo::PendingRemote<cobalt::mojom::blink::CobaltLifecycleObserver>
  BindNewPipeAndPassRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() {
    if (receiver_.is_bound()) {
      receiver_.FlushForTesting();
    }
  }

  void ClearEvents() { events_.clear(); }
  const std::vector<std::string>& events() const { return events_; }

  // cobalt::mojom::blink::CobaltLifecycleObserver implementation:
  void OnPageVisibilityChanged(bool visible) override {
    events_.push_back(visible ? "visibility:true" : "visibility:false");
  }
  void OnPageFocused() override { events_.push_back("focused"); }
  void OnPageBlurred() override { events_.push_back("blurred"); }
  void OnPageFrozen() override { events_.push_back("frozen"); }
  void OnPageResumed() override { events_.push_back("resumed"); }
  void OnFrameReady() override { events_.push_back("ready"); }

 private:
  std::vector<std::string> events_;
  mojo::Receiver<cobalt::mojom::blink::CobaltLifecycleObserver> receiver_{this};
};

// Unit test fixture verifying CobaltLifecycleController Mojo notifications,
// context-destruction guards, and Oilpan WeakPersistent null-clearing on
// collection.
//
// Lifetime and ownership: Instantiated and owned by GTest per test case.
// Threading: Runs exclusively on the Blink main test thread.
class CobaltLifecycleControllerTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size(1920, 1080));
    // Move off the initial empty about:blank document so subsequent
    // cross-origin navigations replace LocalDOMWindow rather than reusing it
    // via ShouldReuseDOMWindow.
    NavigateTo(KURL("https://a.example/initial"));
    GetPage().GetFocusController().SetActive(true);
    GetPage().GetFocusController().SetFocusedFrame(&GetFrame());
  }

  // Marked NOINLINE so no raw Oilpan pointers remain in caller registers or
  // stack slots when ClearOutOldGarbage runs.
  NOINLINE WeakPersistent<CobaltLifecycleController> BindControllerAndObserver(
      mojo::Remote<cobalt::mojom::blink::CobaltLifecycleController>&
          controller_remote,
      FakeCobaltLifecycleObserver& observer) {
    CobaltLifecycleController::BindReceiver(
        &GetFrame(), controller_remote.BindNewPipeAndPassReceiver());
    controller_remote->SetObserver(observer.BindNewPipeAndPassRemote());
    controller_remote.FlushForTesting();
    observer.FlushForTesting();
    return WrapWeakPersistent(
        CobaltLifecycleController::From(*GetFrame().DomWindow()));
  }

  NOINLINE void QueueAllAsyncLifecycleNotifications() {
    GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                                 /*is_initial_state=*/false);
    GetPage().GetFocusController().SetFocused(true);
    GetPage().GetFocusController().SetFocused(false);
    CobaltLifecycleController* controller =
        CobaltLifecycleController::From(*GetFrame().DomWindow());
    ASSERT_NE(controller, nullptr);
    controller->ContextLifecycleStateChanged(
        mojom::blink::FrameLifecycleState::kRunning);
  }

  // Commits a cross-document navigation synchronously without pumping pending
  // tasks so queued TaskType::kDOMManipulation closures remain pending while
  // the previous LocalDOMWindow becomes detached and eligible for Oilpan GC.
  NOINLINE void CommitCrossDocumentNavigationWithoutRunningTasks(
      const KURL& url) {
    auto params = WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
    MockPolicyContainerHost mock_policy_container_host;
    params->policy_container = std::make_unique<WebPolicyContainer>(
        WebPolicyContainerPolicies(),
        mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
    GetFrame().Loader().CommitNavigation(std::move(params),
                                         /*extra_data=*/nullptr);
  }
};

// Covers the user journey where a viewer backgrounds or unfocuses the TV app
// while a startup redirect or cross-document navigation replaces the active
// DOM window and triggers garbage collection before queued DOM lifecycle tasks
// execute.
TEST_F(CobaltLifecycleControllerTest,
       CancelsQueuedLifecycleNotificationsAfterNavigationAndGarbageCollection) {
  mojo::Remote<cobalt::mojom::blink::CobaltLifecycleController>
      controller_remote;
  FakeCobaltLifecycleObserver observer;
  WeakPersistent<CobaltLifecycleController> weak_old_controller =
      BindControllerAndObserver(controller_remote, observer);
  ASSERT_TRUE(weak_old_controller);
  observer.ClearEvents();

  QueueAllAsyncLifecycleNotifications();

  CommitCrossDocumentNavigationWithoutRunningTasks(
      KURL("https://b.example/next"));
  controller_remote.reset();

  TestSupportingGC::ClearOutOldGarbage();
  ASSERT_FALSE(weak_old_controller);

  test::RunPendingTasks();
  observer.FlushForTesting();
  EXPECT_TRUE(observer.events().empty());
}

// Covers the user journey where visibility or focus changes immediately before
// a cross-document navigation detaches the old DOM window, and the queued task
// runs before garbage collection sweeps the detached controller.
TEST_F(
    CobaltLifecycleControllerTest,
    DropsQueuedLifecycleNotificationsAfterNavigationBeforeGarbageCollection) {
  mojo::Remote<cobalt::mojom::blink::CobaltLifecycleController>
      controller_remote;
  FakeCobaltLifecycleObserver observer;
  WeakPersistent<CobaltLifecycleController> weak_old_controller =
      BindControllerAndObserver(controller_remote, observer);
  ASSERT_TRUE(weak_old_controller);
  observer.ClearEvents();

  QueueAllAsyncLifecycleNotifications();

  CommitCrossDocumentNavigationWithoutRunningTasks(
      KURL("https://b.example/next"));
  ASSERT_TRUE(weak_old_controller);

  test::RunPendingTasks();
  observer.FlushForTesting();
  EXPECT_TRUE(observer.events().empty());
}

// Covers normal active browsing and playback where page visibility, focus,
// blur, freeze, and resume transitions must reach the browser lifecycle
// observer in FIFO order.
TEST_F(CobaltLifecycleControllerTest,
       DeliversLifecycleNotificationsInOrderWhileContextIsActive) {
  mojo::Remote<cobalt::mojom::blink::CobaltLifecycleController>
      controller_remote;
  FakeCobaltLifecycleObserver observer;
  WeakPersistent<CobaltLifecycleController> weak_controller =
      BindControllerAndObserver(controller_remote, observer);
  ASSERT_TRUE(weak_controller);
  EXPECT_THAT(observer.events(),
              ElementsAre("visibility:true", "blurred", "resumed", "ready"));
  observer.ClearEvents();

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  GetPage().GetFocusController().SetFocused(true);
  GetPage().GetFocusController().SetFocused(false);

  CobaltLifecycleController* controller =
      CobaltLifecycleController::From(*GetFrame().DomWindow());
  ASSERT_NE(controller, nullptr);
  controller->ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState::kRunning);

  test::RunPendingTasks();
  observer.FlushForTesting();
  EXPECT_THAT(observer.events(),
              ElementsAre("visibility:false", "visibility:true", "focused",
                          "blurred", "resumed"));

  observer.ClearEvents();
  controller->ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState::kFrozen);
  observer.FlushForTesting();
  EXPECT_THAT(observer.events(), ElementsAre("frozen"));
}

// Covers the user journey where visibility, focus, or lifecycle callbacks hit
// a detached DOM window after navigation has destroyed its execution context
// but before Oilpan sweeps the controller.
TEST_F(CobaltLifecycleControllerTest, IgnoresNotificationsOnDestroyedContext) {
  LocalDOMWindow* detached_window = GetFrame().DomWindow();
  mojo::Remote<cobalt::mojom::blink::CobaltLifecycleController>
      controller_remote;
  FakeCobaltLifecycleObserver observer;
  WeakPersistent<CobaltLifecycleController> weak_old_controller =
      BindControllerAndObserver(controller_remote, observer);
  ASSERT_TRUE(weak_old_controller);
  observer.ClearEvents();

  CommitCrossDocumentNavigationWithoutRunningTasks(
      KURL("https://b.example/next"));
  ASSERT_TRUE(detached_window->IsContextDestroyed());
  ASSERT_TRUE(weak_old_controller);

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  GetPage().GetFocusController().SetFocused(true);
  GetPage().GetFocusController().SetFocused(false);
  weak_old_controller->ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState::kRunning);
  weak_old_controller->ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState::kFrozen);

  test::RunPendingTasks();
  observer.FlushForTesting();
  EXPECT_TRUE(observer.events().empty());
}

}  // namespace
}  // namespace blink
