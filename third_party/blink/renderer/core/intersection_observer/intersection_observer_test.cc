// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_element.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_delegate.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/intersection_observer_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class IntersectionObserverTest : public SimTest {};

class IntersectionObserverV2Test : public IntersectionObserverTest {
 public:
  IntersectionObserverV2Test() {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
  }

  ~IntersectionObserverV2Test() override {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
  }
};

TEST_F(IntersectionObserverTest, ObserveSchedulesFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div id='target'></div>");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_TRUE(observer->takeRecords(exception_state).empty());
  EXPECT_EQ(observer_delegate->CallCount(), 0);

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
}

TEST_F(IntersectionObserverTest, NotificationSentWhenRootRemoved) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #target {
      width: 100px;
      height: 100px;
    }
    </style>
    <div id='root'>
      <div id='target'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* root = GetDocument().getElementById("root");
  ASSERT_TRUE(root);
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());

  root->remove();
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
}

TEST_F(IntersectionObserverTest, DocumentRootClips) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <iframe src="iframe.html" style="width:200px; height:100px"></iframe>
  )HTML");
  iframe_resource.Complete(R"HTML(
    <div id='target'>Hello, world!</div>
    <div id='spacer' style='height:2000px'></div>
  )HTML");
  Compositor().BeginFrame();

  Document* iframe_document = To<WebLocalFrameImpl>(MainFrame().FirstChild())
                                  ->GetFrame()
                                  ->GetDocument();
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(
      MakeGarbageCollected<V8UnionDocumentOrElement>(iframe_document));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = iframe_document->getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());

  iframe_document->View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 1000), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
}

TEST_F(IntersectionObserverTest, ReportsFractionOfTargetOrRoot) {
  // Place a 100x100 target element in the middle of a 200x200 main frame.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #target {
      position: absolute;
      top: 50px; left: 50px; width: 100px; height: 100px;
    }
    </style>
    <div id='target'></div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  // 100% of the target element's area intersects with the frame.
  constexpr float kExpectedFractionOfTarget = 1.0f;

  // 25% of the frame's area is covered by the target element.
  constexpr float kExpectedFractionOfRoot = 0.25f;

  TestIntersectionObserverDelegate* target_observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* target_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_observer_delegate, nullptr, Vector<Length>(),
          Vector<float>{kExpectedFractionOfTarget / 2},
          IntersectionObserver::kFractionOfTarget, 0, false, false,
          IntersectionObserver::kApplyMarginToRoot,
          /* use_overflow_clip_edge */ false);
  DummyExceptionStateForTesting exception_state;
  target_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  TestIntersectionObserverDelegate* root_observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* root_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *root_observer_delegate, nullptr, Vector<Length>(),
          Vector<float>{kExpectedFractionOfRoot / 2},
          IntersectionObserver::kFractionOfRoot, 0, false, false,
          IntersectionObserver::kApplyMarginToRoot,
          /* use_overflow_clip_edge */ false);
  root_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 1);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(target_observer_delegate->LastEntry()->isIntersecting());
  EXPECT_NEAR(kExpectedFractionOfTarget,
              target_observer_delegate->LastEntry()->intersectionRatio(), 1e-6);

  EXPECT_EQ(root_observer_delegate->CallCount(), 1);
  EXPECT_EQ(root_observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(root_observer_delegate->LastEntry()->isIntersecting());
  EXPECT_NEAR(kExpectedFractionOfRoot,
              root_observer_delegate->LastEntry()->intersectionRatio(), 1e-6);
}

TEST_F(IntersectionObserverTest, TargetRectIsEmptyAfterMapping) {
  // Place a 100x100 target element in the middle of a 200x200 main frame.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    .clipper {
      transform: rotatey(90deg);
    }
    .container {
      overflow: hidden;
    }
    #target {
      width: 10px;
      height: 10px;
    }
    </style>
    <div class=clipper>
      <div class=container>
        <div id=target></div>
      </div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  TestIntersectionObserverDelegate* target_observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* target_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_observer_delegate, nullptr, Vector<Length>(),
          Vector<float>{std::numeric_limits<float>::min()},
          IntersectionObserver::kFractionOfTarget, 0, false, false,
          IntersectionObserver::kApplyMarginToRoot,
          /* use_overflow_clip_edge */ false);
  DummyExceptionStateForTesting exception_state;
  target_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 1);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(target_observer_delegate->LastEntry()->isIntersecting());
}

TEST_F(IntersectionObserverTest, ResumePostsTask) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='leading-space' style='height: 700px;'></div>
    <div id='target'></div>
    <div id='trailing-space' style='height: 700px;'></div>
  )HTML");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  // When document is not suspended, beginFrame() will generate notifications
  // and post a task to deliver them.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);

  // When a document is suspended, beginFrame() will generate a notification,
  // but it will not be delivered.  The notification will, however, be
  // available via takeRecords();
  WebView().GetPage()->SetPaused(true);
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 0), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_FALSE(observer->takeRecords(exception_state).empty());

  // Generate a notification while document is suspended; then resume document.
  // Notification should happen in a post task.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  WebView().GetPage()->SetPaused(false);
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
}

TEST_F(IntersectionObserverTest, HitTestAfterMutation) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='leading-space' style='height: 700px;'></div>
    <div id='target'></div>
    <div id='trailing-space' style='height: 700px;'></div>
  )HTML");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  GetDocument().View()->ScheduleAnimation();

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);

  HitTestLocation location{PhysicalOffset()};
  HitTestResult result(
      HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                     HitTestRequest::kAllowChildFrameContent),
      location);
  GetDocument().View()->GetLayoutView()->HitTest(location, result);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
}

TEST_F(IntersectionObserverTest, DisconnectClearsNotifications) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='leading-space' style='height: 700px;'></div>
    <div id='target'></div>
    <div id='trailing-space' style='height: 700px;'></div>
  )HTML");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  IntersectionObserverController& controller =
      GetDocument().EnsureIntersectionObserverController();
  observer->observe(target, exception_state);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 1u);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  // If disconnect() is called while an observer has unsent notifications,
  // those notifications should be discarded.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  observer->disconnect();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
}

TEST_F(IntersectionObserverTest, RootIntersectionWithForceZeroLayoutHeight) {
  WebView().GetSettings()->SetForceZeroLayoutHeight(true);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        margin: 0;
        height: 2000px;
      }

      #target {
        width: 100px;
        height: 100px;
        position: absolute;
        top: 1000px;
        left: 200px;
      }
    </style>
    <div id='target'></div>
  )HTML");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_TRUE(observer_delegate->LastIntersectionRect().IsEmpty());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 600), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_FALSE(observer_delegate->LastIntersectionRect().IsEmpty());
  EXPECT_EQ(PhysicalRect(200, 400, 100, 100),
            observer_delegate->LastIntersectionRect());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 1200), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_TRUE(observer_delegate->LastIntersectionRect().IsEmpty());
}

TEST_F(IntersectionObserverTest, TrackedTargetBookkeeping) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    </style>
    <div id='target'></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer1 =
      IntersectionObserver::Create(observer_init, *observer_delegate);
  observer1->observe(target);
  IntersectionObserver* observer2 =
      IntersectionObserver::Create(observer_init, *observer_delegate);
  observer2->observe(target);

  ElementIntersectionObserverData* target_data =
      target->IntersectionObserverData();
  ASSERT_TRUE(target_data);
  IntersectionObserverController& controller =
      GetDocument().EnsureIntersectionObserverController();
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 2u);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);

  target->remove();
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);
  GetDocument().body()->AppendChild(target);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 2u);

  observer1->unobserve(target);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 1u);

  observer2->unobserve(target);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);
}

TEST_F(IntersectionObserverTest, TrackedRootBookkeeping) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root'>
      <div id='target1'></div>
      <div id='target2'></div>
    </div>
  )HTML");

  IntersectionObserverController& controller =
      GetDocument().EnsureIntersectionObserverController();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  Persistent<Element> root = GetDocument().getElementById("root");
  Persistent<Element> target = GetDocument().getElementById("target1");
  Persistent<IntersectionObserverInit> observer_init =
      IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  Persistent<TestIntersectionObserverDelegate> observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  Persistent<IntersectionObserver> observer =
      IntersectionObserver::Create(observer_init, *observer_delegate);

  // For an explicit-root observer, the root element is tracked only when it
  // has observations and is connected. Target elements are not tracked.
  ElementIntersectionObserverData* root_data = root->IntersectionObserverData();
  ASSERT_TRUE(root_data);
  EXPECT_FALSE(root_data->IsEmpty());
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  observer->observe(target);
  ElementIntersectionObserverData* target_data =
      target->IntersectionObserverData();
  ASSERT_TRUE(target_data);
  EXPECT_FALSE(target_data->IsEmpty());
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  // Root should not be tracked if it's not connected.
  root->remove();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  GetDocument().body()->AppendChild(root);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);

  // Root should not be tracked if it has no observations.
  observer->disconnect();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  observer->observe(target);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);
  observer->unobserve(target);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  observer->observe(target);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);

  // The existing observation should keep the observer alive and active.
  // Flush any pending notifications, which hold a hard reference to the
  // observer and can prevent it from being gc'ed. The observation will be the
  // only thing keeping the observer alive.
  test::RunPendingTasks();
  observer_delegate->Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(root_data->IsEmpty());
  EXPECT_FALSE(target_data->IsEmpty());
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  // When the last observation is disconnected, as a result of the target being
  // gc'ed, the root element should no longer be tracked after the next
  // lifecycle update.
  target->remove();
  target = nullptr;
  target_data = nullptr;
  // Removing the target from the DOM tree forces a notification to be
  // queued, so flush it out.
  test::RunPendingTasks();
  observer_delegate->Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  Compositor().BeginFrame();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  // Removing the last reference to the observer should allow it to be dropeed
  // from the root's ElementIntersectionObserverData.
  observer = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(root_data->IsEmpty());

  target = GetDocument().getElementById("target2");
  observer = IntersectionObserver::Create(observer_init, *observer_delegate);
  observer->observe(target);
  target_data = target->IntersectionObserverData();
  ASSERT_TRUE(target_data);

  // If the explicit root of an observer goes away, any existing observations
  // should be disconnected.
  target->remove();
  root->remove();
  root = nullptr;
  test::RunPendingTasks();
  observer_delegate->Clear();
  observer_delegate = nullptr;
  observer_init = nullptr;
  // Removing the target from the tree is not enough to disconnect the
  // observation.
  EXPECT_FALSE(target_data->IsEmpty());
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(target_data->IsEmpty());
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);
}

TEST_F(IntersectionObserverTest, InaccessibleTarget) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id=target></div>
  )HTML");

  Persistent<TestIntersectionObserverDelegate> observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  Persistent<IntersectionObserver> observer = IntersectionObserver::Create(
      IntersectionObserverInit::Create(), *observer_delegate);

  Persistent<Element> target = GetDocument().getElementById("target");
  ASSERT_EQ(observer_delegate->CallCount(), 0);
  ASSERT_FALSE(observer->HasPendingActivity());

  // When we start observing a target, we should queue up a task to deliver the
  // observation. The observer should have pending activity.
  observer->observe(target);
  Compositor().BeginFrame();
  ASSERT_EQ(observer_delegate->CallCount(), 0);
  EXPECT_TRUE(observer->HasPendingActivity());

  // After the observation is delivered, the observer no longer has activity
  // pending.
  test::RunPendingTasks();
  ASSERT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_FALSE(observer->HasPendingActivity());

  WeakPersistent<TestIntersectionObserverDelegate> observer_delegate_weak =
      observer_delegate.Get();
  WeakPersistent<IntersectionObserver> observer_weak = observer.Get();
  WeakPersistent<Element> target_weak = target.Get();
  ASSERT_TRUE(target_weak);
  ASSERT_TRUE(observer_weak);
  ASSERT_TRUE(observer_delegate_weak);

  // When |target| is no longer live, and |observer| has no more pending tasks,
  // both should be garbage-collected.
  target->remove();
  target = nullptr;
  observer = nullptr;
  observer_delegate = nullptr;
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(target_weak);
  EXPECT_FALSE(observer_weak);
  EXPECT_FALSE(observer_delegate_weak);
}

TEST_F(IntersectionObserverTest, InaccessibleTargetBeforeDelivery) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id=target></div>
  )HTML");

  Persistent<TestIntersectionObserverDelegate> observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  Persistent<IntersectionObserver> observer = IntersectionObserver::Create(
      IntersectionObserverInit::Create(), *observer_delegate);

  Persistent<Element> target = GetDocument().getElementById("target");
  ASSERT_EQ(observer_delegate->CallCount(), 0);
  ASSERT_FALSE(observer->HasPendingActivity());

  WeakPersistent<TestIntersectionObserverDelegate> observer_delegate_weak =
      observer_delegate.Get();
  WeakPersistent<IntersectionObserver> observer_weak = observer.Get();
  WeakPersistent<Element> target_weak = target.Get();
  ASSERT_TRUE(target_weak);
  ASSERT_TRUE(observer_weak);
  ASSERT_TRUE(observer_delegate_weak);

  // When we start observing |target|, a task should be queued to call the
  // callback with |target| and other information. So even if we remove
  // |target| in the same tick, |observer| would be kept alive.
  observer->observe(target);
  target->remove();
  target = nullptr;
  observer = nullptr;
  observer_delegate = nullptr;
  Compositor().BeginFrame();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(target_weak);
  EXPECT_TRUE(observer_weak);
  EXPECT_TRUE(observer_delegate_weak);

  // Once we run the callback, the observer has no more pending tasks, and so
  // it should be garbage-collected along with the target.
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(target_weak);
  EXPECT_FALSE(observer_weak);
  EXPECT_FALSE(observer_delegate_weak);
}

TEST_F(IntersectionObserverTest, RootMarginDevicePixelRatio) {
  WebView().SetZoomFactorForDeviceScaleFactor(3.5f);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(2800, 2100));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    body {
      margin: 0;
    }
    #target {
      height: 30px;
    }
    </style>
    <div id='target'>Hello, world!</div>
  )HTML");
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRootMargin("-31px 0px 0px 0px");
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(ToPixelSnappedRect(
                observer_delegate->LastEntry()->GetGeometry().RootRect()),
            gfx::Rect(0, 31, 800, 600 - 31));
}

TEST_F(IntersectionObserverTest, CachedRectsTest) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    body { margin: 0; }
    .spacer { height: 1000px; }
    .scroller { overflow-y: scroll; height: 100px; position: relative; }
    </style>
    <div id='root' class='scroller'>
      <div id='target1-container'>
        <div id='target1'>Hello, world!</div>
      </div>
      <div class='scroller'>
        <div id='target2'>Hello, world!</div>
        <div class='spacer'></div>
      </div>
      <div class='spacer'></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById("root");
  Element* target1 = GetDocument().getElementById("target1");
  Element* target2 = GetDocument().getElementById("target2");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target1, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target2, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  IntersectionObservation* observation1 =
      target1->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(observation1->CanUseCachedRectsForTesting());
  IntersectionObservation* observation2 =
      target2->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(observation2->CanUseCachedRectsForTesting());

  // Generate initial notifications and populate cache
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(observation1->CanUseCachedRectsForTesting());
  // observation2 can't use cached rects because the observer's root is not the
  // target's enclosing scroller.
  EXPECT_FALSE(observation2->CanUseCachedRectsForTesting());

  // Scrolling the root should not invalidate.
  PaintLayerScrollableArea* root_scroller =
      root->GetLayoutBoxForScrolling()->GetScrollableArea();
  root_scroller->SetScrollOffset(ScrollOffset(0, 100),
                                 mojom::blink::ScrollType::kProgrammatic);
  EXPECT_TRUE(observation1->CanUseCachedRectsForTesting());

  // Changing layout between root and target should invalidate.
  target1->parentElement()->SetInlineStyleProperty(CSSPropertyID::kMarginLeft,
                                                   "10px");
  // Invalidation happens during compositing inputs update, so force it here.
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(observation1->CanUseCachedRectsForTesting());

  // Moving target2 out from the subscroller should allow it to cache rects.
  target2->remove();
  root->appendChild(target2);
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(observation1->CanUseCachedRectsForTesting());
  EXPECT_TRUE(observation2->CanUseCachedRectsForTesting());
}

TEST_F(IntersectionObserverV2Test, TrackVisibilityInit) {
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_FALSE(observer->trackVisibility());

  // This should fail because no delay is set.
  observer_init->setTrackVisibility(true);
  observer = IntersectionObserver::Create(observer_init, *observer_delegate,
                                          exception_state);
  EXPECT_TRUE(exception_state.HadException());

  // This should fail because the delay is < 100.
  exception_state.ClearException();
  observer_init->setDelay(99.9);
  observer = IntersectionObserver::Create(observer_init, *observer_delegate,
                                          exception_state);
  EXPECT_TRUE(exception_state.HadException());

  exception_state.ClearException();
  observer_init->setDelay(101.);
  observer = IntersectionObserver::Create(observer_init, *observer_delegate,
                                          exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_TRUE(observer->trackVisibility());
  EXPECT_EQ(observer->delay(), 101.);
}

TEST_F(IntersectionObserverV2Test, BasicOcclusion) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='target'>
      <div id='child'></div>
    </div>
    <div id='occluder'></div>
  )HTML");
  Compositor().BeginFrame();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setTrackVisibility(true);
  observer_init->setDelay(100);
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById("target");
  Element* occluder = GetDocument().getElementById("occluder");
  ASSERT_TRUE(target);
  observer->observe(target);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_TRUE(observer_delegate->LastEntry()->isVisible());

  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-10px");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_FALSE(observer_delegate->LastEntry()->isVisible());

  // Zero-opacity objects should not count as occluding.
  occluder->SetInlineStyleProperty(CSSPropertyID::kOpacity, "0");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_TRUE(observer_delegate->LastEntry()->isVisible());
}

TEST_F(IntersectionObserverV2Test, BasicOpacity) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='transparent'>
      <div id='target'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setTrackVisibility(true);
  observer_init->setDelay(100);
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById("target");
  Element* transparent = GetDocument().getElementById("transparent");
  ASSERT_TRUE(target);
  ASSERT_TRUE(transparent);
  observer->observe(target);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_TRUE(observer_delegate->LastEntry()->isVisible());

  transparent->SetInlineStyleProperty(CSSPropertyID::kOpacity, "0.99");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_FALSE(observer_delegate->LastEntry()->isVisible());
}

TEST_F(IntersectionObserverV2Test, BasicTransform) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='transformed'>
      <div id='target'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setTrackVisibility(true);
  observer_init->setDelay(100);
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById("target");
  Element* transformed = GetDocument().getElementById("transformed");
  ASSERT_TRUE(target);
  ASSERT_TRUE(transformed);
  observer->observe(target);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_TRUE(observer_delegate->LastEntry()->isVisible());

  // 2D translations and proportional upscaling is permitted.
  transformed->SetInlineStyleProperty(
      CSSPropertyID::kTransform, "translateX(10px) translateY(20px) scale(2)");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);

  // Any other transform is not permitted.
  transformed->SetInlineStyleProperty(CSSPropertyID::kTransform,
                                      "skewX(10deg)");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_FALSE(observer_delegate->LastEntry()->isVisible());
}

TEST_F(IntersectionObserverTest, ApplyMarginToTarget) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #scroller { height: 100px; overflow: scroll; }
    #target { width: 50px; height: 50px; }
    .spacer { height: 105px; }
    </style>
    <div id=scroller>
      <div class=spacer></div>
      <div id=target></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  TestIntersectionObserverDelegate* root_margin_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* root_margin_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *root_margin_delegate, nullptr, Vector<Length>{Length::Fixed(10)},
          Vector<float>{std::numeric_limits<float>::min()},
          IntersectionObserver::kFractionOfTarget, 0, false, false,
          IntersectionObserver::kApplyMarginToRoot,
          /* use_overflow_clip_edge */ false);

  DummyExceptionStateForTesting exception_state;
  root_margin_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  TestIntersectionObserverDelegate* target_margin_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  // Same parameters as above except that margin is applied to target.
  IntersectionObserver* target_margin_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_margin_delegate, nullptr, Vector<Length>{Length::Fixed(10)},
          Vector<float>{std::numeric_limits<float>::min()},
          IntersectionObserver::kFractionOfTarget, 0, false, false,
          IntersectionObserver::kApplyMarginToTarget,
          /* use_overflow_clip_edge */ false);

  target_margin_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(root_margin_delegate->CallCount(), 1);
  EXPECT_EQ(root_margin_delegate->EntryCount(), 1);
  // Since the inner scroller clips content, the root margin has no effect and
  // target is not intersecting.
  EXPECT_FALSE(root_margin_delegate->LastEntry()->isIntersecting());

  EXPECT_EQ(target_margin_delegate->CallCount(), 1);
  EXPECT_EQ(target_margin_delegate->EntryCount(), 1);
  // Since the margin is applied to the target, the inner scroller clips an
  // expanded rect, which ends up being visible in the root. Hence, it is
  // intersecting.
  EXPECT_TRUE(target_margin_delegate->LastEntry()->isIntersecting());
}

TEST_F(IntersectionObserverTest, TargetMarginPercentResolvesAgainstRoot) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 500));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #scroller { height: 100px; overflow: scroll; }
    #target { width: 50px; height: 50px; }
    .spacer { height: 145px; }
    </style>
    <div id=scroller>
      <div class=spacer></div>
      <div id=target></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  TestIntersectionObserverDelegate* target_margin_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  // 10% margin on a target would be 5px if it resolved against target, which is
  // not enough to intersect. It would be 10px if it resolved against the
  // scroller, which is also not enough. However, it would be 50px if it
  // resolved against root, which would make it intersecting.
  IntersectionObserver* target_margin_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_margin_delegate, nullptr, Vector<Length>{Length::Percent(10)},
          Vector<float>{std::numeric_limits<float>::min()},
          IntersectionObserver::kFractionOfTarget, 0, false, false,
          IntersectionObserver::kApplyMarginToTarget,
          /* use_overflow_clip_edge */ false);

  DummyExceptionStateForTesting exception_state;
  target_margin_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_margin_delegate->CallCount(), 1);
  EXPECT_EQ(target_margin_delegate->EntryCount(), 1);
  EXPECT_TRUE(target_margin_delegate->LastEntry()->isIntersecting());
}
}  // namespace blink
