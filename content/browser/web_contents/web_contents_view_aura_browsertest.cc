// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include <stddef.h>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"

namespace content {

namespace {

// TODO(tdresser): Find a way to avoid sleeping like this. See crbug.com/405282
// for details.
void GiveItSomeTime() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(10));
  run_loop.Run();
}

// A test delegate used in drag and drop tests to simulate either good or bad
// deep scans of data.
class TestWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  TestWebContentsViewDelegate(bool allow_drop) : allow_drop_(allow_drop) {}

  void OnPerformingDrop(const DropData& drop_data,
                        DropCompletionCallback callback) override {
    if (allow_drop_) {
      drop_callback_ = base::BindOnce(std::move(callback), drop_data);
    } else {
      drop_callback_ = base::BindOnce(std::move(callback), absl::nullopt);
    }

    renderer_told_to_force_default_action_ =
        !drop_data.document_is_handling_drag;
  }

  void FinishOnPerformingDrop() {
    if (drop_callback_) {
      std::move(drop_callback_).Run();
    }
  }

  bool IsRendererToldToForceDefaultAction() {
    return renderer_told_to_force_default_action_;
  }

 private:
  bool allow_drop_;
  bool renderer_told_to_force_default_action_ = false;
  base::OnceClosure drop_callback_;
};

}  // namespace

class WebContentsViewAuraTest : public ContentBrowserTest {
 public:
  WebContentsViewAuraTest() = default;

  WebContentsViewAuraTest(const WebContentsViewAuraTest&) = delete;
  WebContentsViewAuraTest& operator=(const WebContentsViewAuraTest&) = delete;

  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL test_url;
    if (url == "about:blank")
      test_url = GURL(url);
    else
      test_url = GURL(embedded_test_server()->GetURL(url));
    EXPECT_TRUE(NavigateToURL(shell(), test_url));

    frame_observer_ = std::make_unique<RenderFrameSubmissionObserver>(
        shell()->web_contents());
  }

  void SetUpOnMainThread() override {
    // Setup the server to allow serving separate sites, so we can perform
    // cross-process navigation.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(switches::kTouchEventFeatureDetection,
                           switches::kTouchEventFeatureDetectionEnabled);
  }

  WebContentsImpl* GetWebContentsImpl() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  WebContentsViewAura* GetWebContentsViewAura() {
    WebContentsImpl* contents = GetWebContentsImpl();
    return static_cast<WebContentsViewAura*>(contents->GetView());
  }

  TestWebContentsViewDelegate* PrepareWebContentsViewForDropTest(
      bool delegate_allows_drop) {
    WebContentsViewAura* view = GetWebContentsViewAura();

    drag_dest_delegate_.Reset();
    view->SetDragDestDelegateForTesting(&drag_dest_delegate_);

    auto delegate = std::make_unique<TestWebContentsViewDelegate>(
        /*allow_drop=*/delegate_allows_drop);
    TestWebContentsViewDelegate* delegate_ptr = delegate.get();
    view->SetDelegateForTesting(std::move(delegate));
    view->RegisterDropCallbackForTesting(base::BindOnce(
        &WebContentsViewAuraTest::OnDropComplete, base::Unretained(this)));

    return delegate_ptr;
  }

  void SimulateDragEnterAndDrop(bool document_is_handling_drag) {
    WebContentsViewAura* view = GetWebContentsViewAura();
    std::unique_ptr<ui::OSExchangeData> data =
        std::make_unique<ui::OSExchangeData>();
    gfx::PointF point = {10, 10};
    ui::DropTargetEvent event(*data.get(), point, point,
                              ui::DragDropTypes::DRAG_COPY);
    view->OnDragEntered(event);
    view->UpdateDragOperation(ui::mojom::DragOperation::kCopy,
                              document_is_handling_drag);
    EXPECT_TRUE(drag_dest_delegate_.GetDragInitializeCalled());
    auto drop_cb = view->GetDropCallback(event);
    ASSERT_TRUE(drop_cb);
    ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(std::move(data), output_drag_op,
                           /*drag_image_layer_owner=*/nullptr);
  }

  void OnDropComplete(RenderWidgetHostImpl* target_rwh,
                      const DropData& drop_data,
                      const gfx::PointF& client_pt,
                      const gfx::PointF& screen_pt,
                      int key_modifiers,
                      bool drop_allowed) {
    // Cache the data for verification.
    drop_target_widget_ = target_rwh;
    std::move(async_drop_closure_).Run();
  }

  void TestOverscrollNavigation(bool touch_handler) {
    ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));
    WebContentsImpl* web_contents = GetWebContentsImpl();
    NavigationController& controller = web_contents->GetController();
    RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

    EXPECT_FALSE(controller.CanGoBack());
    EXPECT_FALSE(controller.CanGoForward());
    EXPECT_EQ(0, EvalJs(main_frame, "get_current()"));

    if (touch_handler)
      ASSERT_TRUE(content::ExecJs(main_frame, "install_touch_handler()"));

    ASSERT_TRUE(content::ExecJs(main_frame, "navigate_next()"));
    ASSERT_TRUE(content::ExecJs(main_frame, "navigate_next()"));
    EXPECT_EQ(2, EvalJs(main_frame, "get_current()"));
    EXPECT_TRUE(controller.CanGoBack());
    EXPECT_FALSE(controller.CanGoForward());

    aura::Window* content = web_contents->GetContentNativeView();
    gfx::Rect bounds = content->GetBoundsInRootWindow();
    ui::test::EventGenerator generator(content->GetRootWindow(), content);
    const int kScrollDurationMs = 20;
    const int kScrollSteps = 10;

    {
      // Do a swipe-right now. That should navigate backwards.
      std::u16string expected_title = u"Title: #1";
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          base::Milliseconds(kScrollDurationMs), kScrollSteps);
      std::u16string actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      EXPECT_EQ(1, EvalJs(main_frame, "get_current()"));
      EXPECT_TRUE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }

    {
      // Do a fling-right now. That should navigate backwards.
      std::u16string expected_title = u"Title:";
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          base::Milliseconds(kScrollDurationMs), kScrollSteps);
      std::u16string actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      EXPECT_EQ(0, EvalJs(main_frame, "get_current()"));
      EXPECT_FALSE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }

    {
      // Do a swipe-left now. That should navigate forward.
      std::u16string expected_title = u"Title: #1";
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          base::Milliseconds(kScrollDurationMs), kScrollSteps);
      std::u16string actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      EXPECT_EQ(1, EvalJs(main_frame, "get_current()"));
      EXPECT_TRUE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }
  }

  int GetCurrentIndex() {
    WebContentsImpl* web_contents = GetWebContentsImpl();
    RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
    return EvalJs(main_frame, "get_current()").ExtractInt();
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh =
        shell()->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() const {
    RenderWidgetHostImpl* const rwh =
        RenderWidgetHostImpl::From(shell()
                                       ->web_contents()
                                       ->GetRenderWidgetHostView()
                                       ->GetRenderWidgetHost());
    CHECK(rwh);
    return rwh;
  }

  RenderWidgetHostViewBase* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewBase*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

  void WaitAFrame() {
    while (!GetRenderWidgetHost()->RequestRepaintForTesting())
      GiveItSomeTime();
    frame_observer_->WaitForAnyFrameSubmission();
  }

  void StopObserveringFrames() { frame_observer_.reset(); }

 protected:
  class MockWebDragDestDelegate : public WebDragDestDelegate {
   public:
    void DragInitialize(WebContents* contents) override {
      drag_initialize_called_ = true;
    }
    void OnDragOver() override {}
    void OnDragEnter() override {}
    void OnDrop() override { ++on_drop_called_count_; }
    void OnDragLeave() override { on_drag_leave_called_ = true; }
    void OnReceiveDragData(const ui::OSExchangeData& data) override {}

    void Reset() { drag_initialize_called_ = false; }
    bool GetDragInitializeCalled() { return drag_initialize_called_; }
    int GetOnDropCalledCount() { return on_drop_called_count_; }
    bool GetOnDragLeaveCalled() { return on_drag_leave_called_; }

   private:
    bool drag_initialize_called_ = false;
    int on_drop_called_count_ = 0;
    bool on_drag_leave_called_ = false;
  };

  // ContentBrowserTest:
  void PostRunTestOnMainThread() override {
    // Delete this before the WebContents is destroyed.
    StopObserveringFrames();
    ContentBrowserTest::PostRunTestOnMainThread();
  }

  raw_ptr<RenderWidgetHostImpl, DanglingUntriaged> drop_target_widget_;

  // A closure indicating that async drop operation has completed.
  base::OnceClosure async_drop_closure_;

  MockWebDragDestDelegate drag_dest_delegate_;

 private:
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer_;
};

// Flaky on Windows: http://crbug.com/305722
// The test frequently times out on Linux, too. See crbug.com/440043.
// For Fuchsia, see https://crbug.com/1318245.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA)
#define MAYBE_OverscrollNavigation DISABLED_OverscrollNavigation
#else
#define MAYBE_OverscrollNavigation OverscrollNavigation
#endif

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, MAYBE_OverscrollNavigation) {
  TestOverscrollNavigation(false);
}

// Flaky on Windows (might be related to the above test):
// http://crbug.com/305722
// On Linux, the test frequently times out. (See crbug.com/440043).
// For Fuchsia, see https://crbug.com/1318245.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA)
#define MAYBE_OverscrollNavigationWithTouchHandler \
  DISABLED_OverscrollNavigationWithTouchHandler
#else
#define MAYBE_OverscrollNavigationWithTouchHandler \
  OverscrollNavigationWithTouchHandler
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_OverscrollNavigationWithTouchHandler) {
  TestOverscrollNavigation(true);
}

namespace {
// This fails the test if it sees any mouse move events.
class SpuriousMouseMoveEventObserver
    : public RenderWidgetHost::InputEventObserver {
 public:
  explicit SpuriousMouseMoveEventObserver(RenderWidgetHost* host)
      : host_(host) {
    host_->AddInputEventObserver(this);
  }

  SpuriousMouseMoveEventObserver(const SpuriousMouseMoveEventObserver&) =
      delete;
  SpuriousMouseMoveEventObserver& operator=(
      const SpuriousMouseMoveEventObserver&) = delete;

  ~SpuriousMouseMoveEventObserver() override {
    host_->RemoveInputEventObserver(this);
  }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    EXPECT_NE(blink::WebInputEvent::Type::kMouseMove, event.GetType())
        << "Unexpected mouse move event.";
  }

 private:
  raw_ptr<RenderWidgetHost> host_;
};
}  // namespace

// Start an overscroll gesture and then check if the gesture is interrupted by
// a spurious mouse event. Overscrolling may trigger mouse-move events, but
// these should all be marked as synthesized and get dropped while the
// overscroll gesture is in progress.
// See crbug.com/731914
// Disabled due to flakiness: https://crbug.com/807107.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       DISABLED_OverscrollNotInterruptedBySpuriousMouseEvents) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));
  WebContentsImpl* web_contents = GetWebContentsImpl();
  NavigationController& controller = web_contents->GetController();
  RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(0, EvalJs(main_frame, "get_current()"));

  ASSERT_TRUE(content::ExecJs(main_frame, "navigate_next()"));
  EXPECT_EQ(1, EvalJs(main_frame, "get_current()"));
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // We start an overscroll gesture, but pause mid-gesture.

  // Fail the test if the following gesture produces mouse-moves that don't get
  // dropped.
  SpuriousMouseMoveEventObserver mouse_observer(GetRenderWidgetHost());

  blink::WebGestureEvent gesture_scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_scroll_begin.data.scroll_begin.delta_hint_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = 0.f;
  GetRenderWidgetHost()->ForwardGestureEvent(gesture_scroll_begin);

  blink::WebGestureEvent gesture_scroll_update(
      blink::WebGestureEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_scroll_update.data.scroll_update.delta_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  gesture_scroll_update.data.scroll_update.delta_y = 0.f;
  float start_threshold = OverscrollConfig::kStartTouchscreenThresholdDips;
  gesture_scroll_update.data.scroll_update.delta_x = start_threshold + 1;
  GetRenderWidgetHost()->ForwardGestureEvent(gesture_scroll_update);

  // Wait for the overscroll gesture to start and then allow some time for the
  // spurious mouse event. Since we're testing that an event does not happen,
  // we just have a timeout. This could potentially result in the event
  // happening after the timeout, which would cause the test to succeed
  // incorrectly. That said, the event we're worried about happens almost
  // instantly after the start of the overscroll gesture.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();

  // Check that the overscroll gesture was not reset.
  OverscrollController* overscroll_controller =
      static_cast<RenderWidgetHostViewAura*>(GetRenderWidgetHostView())
          ->overscroll_controller();
  EXPECT_NE(OVERSCROLL_NONE, overscroll_controller->overscroll_mode());
}

// Disabled because the test always fails the first time it runs on the Win Aura
// bots, and usually but not always passes second-try (See crbug.com/179532).
// Flaky on CrOS and Linux as well: https://crbug.com/856079
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_LINUX)
#define MAYBE_QuickOverscrollDirectionChange \
  DISABLED_QuickOverscrollDirectionChange
#else
#define MAYBE_QuickOverscrollDirectionChange QuickOverscrollDirectionChange
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_QuickOverscrollDirectionChange) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));
  WebContentsImpl* web_contents = GetWebContentsImpl();
  RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  // This test triggers a large number of animations. Speed them up to ensure
  // the test completes within its time limit.
  ui::ScopedAnimationDurationScaleMode fast_duration_mode(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  // Make sure the page has both back/forward history.
  ASSERT_TRUE(content::ExecJs(main_frame, "navigate_next()"));
  EXPECT_EQ(1, GetCurrentIndex());
  ASSERT_TRUE(content::ExecJs(main_frame, "navigate_next()"));
  EXPECT_EQ(2, GetCurrentIndex());
  web_contents->GetController().GoToOffset(-1);
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetContentNativeView();
  ui::EventSink* sink = content->GetHost()->GetEventSink();
  gfx::Rect bounds = content->GetBoundsInRootWindow();

  // Spurious mouse moves interfere with the overscroll gesture which causes
  // this test to fail. This observer will let us know if this is happening.
  SpuriousMouseMoveEventObserver mouse_observer(GetRenderWidgetHost());

  // TODO(crbug.com/1322921): Use a mock timer to generate timestamps for
  // events. This would need injecting the mock timer into
  // `cc::CompositorFrameReportingController`.
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED,
      gfx::Point(bounds.x() + bounds.width() / 2, bounds.y() + 5),
      ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::EventDispatchDetails details = sink->OnEventFromSource(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, GetCurrentIndex());

  ui::TouchEvent move1(ui::ET_TOUCH_MOVED,
                       gfx::Point(bounds.right() - 10, bounds.y() + 5),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  details = sink->OnEventFromSource(&move1);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, GetCurrentIndex());

  // Swipe back from the right edge, back to the left edge, back to the right
  // edge.

  for (int x = bounds.right() - 10; x >= bounds.x() + 10; x -= 10) {
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED, gfx::Point(x, bounds.y() + 5),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    details = sink->OnEventFromSource(&inc);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  for (int x = bounds.x() + 10; x <= bounds.width() - 10; x += 10) {
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED, gfx::Point(x, bounds.y() + 5),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    details = sink->OnEventFromSource(&inc);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  for (int x = bounds.width() - 10; x >= bounds.x() + 10; x -= 10) {
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED, gfx::Point(x, bounds.y() + 5),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    details = sink->OnEventFromSource(&inc);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  // Do not end the overscroll sequence.
}

// TODO(sadrul): This test is disabled because it reparents in a way the
//               FocusController does not support. This code would crash in
//               a production build. It only passed prior to this revision
//               because testing used the old FocusManager which did some
//               different (osbolete) processing. TODO(sadrul) to figure out
//               how this test should work that mimics production code a bit
//               better.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       DISABLED_ContentWindowReparent) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->Init(ui::LAYER_NOT_DRAWN);

  WebContentsImpl* web_contents = GetWebContentsImpl();
  ASSERT_TRUE(
      content::ExecJs(web_contents->GetPrimaryMainFrame(), "navigate_next()"));
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  ui::test::EventGenerator generator(content->GetRootWindow(), content);
  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10), base::Milliseconds(20),
      1);

  window->AddChild(shell()->web_contents()->GetContentNativeView());
}

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, DragDropOnOopif) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/overlapping_cross_site_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* contents = GetWebContentsImpl();
  WebContentsViewAura* view = GetWebContentsViewAura();

  view->SetDragDestDelegateForTesting(&drag_dest_delegate_);

  // Drop on the root frame.
  {
    drag_dest_delegate_.Reset();
    std::unique_ptr<ui::OSExchangeData> data =
        std::make_unique<ui::OSExchangeData>();
    view->RegisterDropCallbackForTesting(base::BindOnce(
        &WebContentsViewAuraTest::OnDropComplete, base::Unretained(this)));
    base::RunLoop run_loop;
    async_drop_closure_ = run_loop.QuitClosure();

    gfx::PointF point = {10, 10};
    ui::DropTargetEvent event(*data.get(), point, point,
                              ui::DragDropTypes::DRAG_COPY);
    view->OnDragEntered(event);
    EXPECT_TRUE(drag_dest_delegate_.GetDragInitializeCalled());
    auto drop_cb = view->GetDropCallback(event);
    ASSERT_TRUE(drop_cb);
    ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(std::move(data), output_drag_op,
                           /*drag_image_layer_owner=*/nullptr);

    run_loop.Run();

    EXPECT_EQ(drop_target_widget_,
              RenderWidgetHostImpl::From(contents->GetPrimaryFrameTree()
                                             .root()
                                             ->current_frame_host()
                                             ->GetRenderWidgetHost()));
  }
  // Drop on the element in the root frame overlapping the embedded OOPIF.
  {
    drag_dest_delegate_.Reset();
    std::unique_ptr<ui::OSExchangeData> data =
        std::make_unique<ui::OSExchangeData>();
    view->RegisterDropCallbackForTesting(base::BindOnce(
        &WebContentsViewAuraTest::OnDropComplete, base::Unretained(this)));
    base::RunLoop run_loop;
    async_drop_closure_ = run_loop.QuitClosure();

    float left =
        EvalJs(contents,
               "document.getElementById('target').getBoundingClientRect().left")
            .ExtractInt();
    float top =
        EvalJs(contents,
               "document.getElementById('target').getBoundingClientRect().top")
            .ExtractInt();
    gfx::PointF point = {left + 5, top + 5};
    ui::DropTargetEvent event(*data.get(), point, point,
                              ui::DragDropTypes::DRAG_COPY);
    view->OnDragEntered(event);
    EXPECT_TRUE(drag_dest_delegate_.GetDragInitializeCalled());
    auto drop_cb = view->GetDropCallback(event);
    ASSERT_TRUE(drop_cb);
    ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(std::move(data), output_drag_op,
                           /*drag_image_layer_owner=*/nullptr);

    run_loop.Run();

    EXPECT_EQ(drop_target_widget_,
              RenderWidgetHostImpl::From(contents->GetPrimaryFrameTree()
                                             .root()
                                             ->current_frame_host()
                                             ->GetRenderWidgetHost()));
  }
}

// When the user drops data onto a web page that does not handle drops and the
// web view delegate allows it, the renderer sees this as a "drop" and not a
// "drag leave".
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       Drop_NoDropZone_DelegateAllows) {
  StartTestWithPage("/simple_page.html");

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  TestWebContentsViewDelegate* delegate =
      PrepareWebContentsViewForDropTest(/*delegate_allows_drop=*/true);
  SimulateDragEnterAndDrop(/*document_is_handling_drag=*/false);
  delegate->FinishOnPerformingDrop();

  run_loop.Run();

  EXPECT_TRUE(delegate->IsRendererToldToForceDefaultAction());
  EXPECT_EQ(1, drag_dest_delegate_.GetOnDropCalledCount());
  EXPECT_FALSE(drag_dest_delegate_.GetOnDragLeaveCalled());
}

// When the user drops data onto a web page that does not handle drops and the
// web view delegate blocks it, the renderer sees this as a "drag leave" and not
// a "drop".
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       Drop_NoDropZone_DelegateBlocks) {
  StartTestWithPage("/simple_page.html");

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  TestWebContentsViewDelegate* delegate =
      PrepareWebContentsViewForDropTest(/*delegate_allows_drop=*/false);
  SimulateDragEnterAndDrop(/*document_is_handling_drag=*/false);
  delegate->FinishOnPerformingDrop();

  run_loop.Run();

  EXPECT_EQ(0, drag_dest_delegate_.GetOnDropCalledCount());
  EXPECT_TRUE(drag_dest_delegate_.GetOnDragLeaveCalled());
}

// When the user drops data onto a web page that handles drops and the web view
// delegate allows it, the renderer sees this as a "drop" and not a "drag
// leave".
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, Drop_DropZone_DelegateAllow) {
  StartTestWithPage("/accept-drop.html");

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  TestWebContentsViewDelegate* delegate =
      PrepareWebContentsViewForDropTest(/*delegate_allows_drop=*/true);
  SimulateDragEnterAndDrop(/*document_is_handling_drag=*/true);
  delegate->FinishOnPerformingDrop();

  run_loop.Run();

  EXPECT_FALSE(delegate->IsRendererToldToForceDefaultAction());
  EXPECT_EQ(1, drag_dest_delegate_.GetOnDropCalledCount());
  EXPECT_FALSE(drag_dest_delegate_.GetOnDragLeaveCalled());
}

// When the user drops data onto a web page that handles drops and the web view
// delegate blocks it, the renderer sees this as a "drag leave" and not a
// "drop".
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, Drop_DropZone_DelegateBlocks) {
  StartTestWithPage("/accept-drop.html");

  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  TestWebContentsViewDelegate* delegate =
      PrepareWebContentsViewForDropTest(/*delegate_allows_drop=*/false);
  SimulateDragEnterAndDrop(/*document_is_handling_drag=*/true);
  delegate->FinishOnPerformingDrop();

  run_loop.Run();

  EXPECT_EQ(0, drag_dest_delegate_.GetOnDropCalledCount());
  EXPECT_TRUE(drag_dest_delegate_.GetOnDragLeaveCalled());
}

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, ContentWindowClose) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  WebContentsImpl* web_contents = GetWebContentsImpl();
  ASSERT_TRUE(
      content::ExecJs(web_contents->GetPrimaryMainFrame(), "navigate_next()"));
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  ui::test::EventGenerator generator(content->GetRootWindow(), content);
  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10), base::Milliseconds(20),
      1);

  delete web_contents->GetContentNativeView();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA)
// This appears to be flaky in the same was as the other overscroll
// tests. Enabling for non-Windows platforms.
// See http://crbug.com/369871.
// For linux, see http://crbug.com/381294.
// For ChromeOS, see http://crbug.com/668128.
// For Fuchsia, see https://crbug.com/1318245.
#define MAYBE_RepeatedQuickOverscrollGestures DISABLED_RepeatedQuickOverscrollGestures
#else
#define MAYBE_RepeatedQuickOverscrollGestures RepeatedQuickOverscrollGestures
#endif

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_RepeatedQuickOverscrollGestures) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  WebContentsImpl* web_contents = GetWebContentsImpl();
  NavigationController& controller = web_contents->GetController();
  RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(content::ExecJs(main_frame, "install_touch_handler()"));

  // Navigate twice, then navigate back in history once.
  ASSERT_TRUE(content::ExecJs(main_frame, "navigate_next()"));
  ASSERT_TRUE(content::ExecJs(main_frame, "navigate_next()"));
  EXPECT_EQ(2, GetCurrentIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_EQ(1, GetCurrentIndex());
  EXPECT_EQ(u"Title: #1", web_contents->GetTitle());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());

  aura::Window* content = web_contents->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  ui::test::EventGenerator generator(content->GetRootWindow(), content);

  // Do a swipe left to start a forward navigation. Then quickly do a swipe
  // right.
  std::u16string expected_title = u"Title: #2";
  content::TitleWatcher title_watcher(web_contents, expected_title);
  TestNavigationManager nav_watcher(web_contents,
      embedded_test_server()->GetURL("/overscroll_navigation.html#2"));

  generator.GestureScrollSequence(
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      gfx::Point(bounds.x() + 2, bounds.y() + 10), base::Milliseconds(2000),
      10);
  ASSERT_TRUE(nav_watcher.WaitForNavigationFinished());

  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      base::Milliseconds(2000), 10);
  std::u16string actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);

  EXPECT_EQ(2, GetCurrentIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

class OverscrollWebContentsDelegate : public WebContentsDelegate {
 public:
  void SetCanOverscrollContent(bool can_overscroll_content) {
    can_overscroll_content_ = can_overscroll_content;
  }

  // WebContentsDelegate:
  bool CanOverscrollContent() override { return can_overscroll_content_; }

 private:
  bool can_overscroll_content_ = true;
};

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, RenderViewHostChanged) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  WebContentsImpl* contents = GetWebContentsImpl();
  WebContentsViewAura* view = GetWebContentsViewAura();
  OverscrollWebContentsDelegate delegate;
  contents->SetDelegate(&delegate);

  delegate.SetCanOverscrollContent(false);
  view->RenderViewHostChanged(nullptr, nullptr);
  EXPECT_FALSE(
      !!static_cast<RenderWidgetHostViewAura*>(GetRenderWidgetHostView())
            ->overscroll_controller());

  delegate.SetCanOverscrollContent(true);
  view->RenderViewHostChanged(nullptr, nullptr);
  EXPECT_TRUE(
      !!static_cast<RenderWidgetHostViewAura*>(GetRenderWidgetHostView())
            ->overscroll_controller());
}

// Ensure that SnapToPhysicalPixelBoundary() is called on WebContentsView parent
// change. This is a regression test for http://crbug.com/388908.
// Disabled due to flakiness: https://crbug.com/807107.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       DISABLED_WebContentsViewReparent) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->Init(ui::LAYER_NOT_DRAWN);
  window->AddChild(shell()->web_contents()->GetNativeView());
}

// Flaky on some platforms, likely for the same reason as other flaky overscroll
// tests. http://crbug.com/305722
// TODO(tdresser): Re-enable this once eager GR is back on. See
// crbug.com/410280.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#define MAYBE_OverscrollNavigationTouchThrottling \
        DISABLED_OverscrollNavigationTouchThrottling
#else
#define MAYBE_OverscrollNavigationTouchThrottling \
        DISABLED_OverscrollNavigationTouchThrottling
#endif

// Tests that touch moves are not throttled when performing a scroll gesture on
// a non-scrollable area, except during gesture-nav.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_OverscrollNavigationTouchThrottling) {
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/overscroll_navigation.html"));

  WebContentsImpl* web_contents = GetWebContentsImpl();
  aura::Window* content = web_contents->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  const int dx = 20;

  ASSERT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(),
                              "install_touchmove_handler()"));

  WaitAFrame();

  for (int navigated = 0; navigated <= 1; ++navigated) {
    if (navigated) {
      ASSERT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(),
                                  "navigate_next()"));
      ASSERT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(),
                                  "reset_touchmove_count()"));
    }
    InputEventAckWaiter touch_start_waiter(
        GetRenderWidgetHost(),
        base::BindRepeating([](blink::mojom::InputEventResultSource,
                               blink::mojom::InputEventResultState state,
                               const blink::WebInputEvent& event) {
          return event.GetType() == blink::WebGestureEvent::Type::kTouchStart &&
                 state == blink::mojom::InputEventResultState::kNotConsumed;
        }));
    // Send touch press.
    blink::SyntheticWebTouchEvent touch;
    touch.PressPoint(bounds.x() + 2, bounds.y() + 10);
    GetRenderWidgetHost()->ForwardTouchEventWithLatencyInfo(touch,
                                                            ui::LatencyInfo());
    touch_start_waiter.Wait();
    WaitAFrame();

    // Send first touch move, and then a scroll begin.
    touch.MovePoint(0, bounds.x() + 20 + 1 * dx, bounds.y() + 100);
    InputEventAckWaiter touch_move_waiter(
        GetRenderWidgetHost(),
        base::BindRepeating([](blink::mojom::InputEventResultSource,
                               blink::mojom::InputEventResultState state,
                               const blink::WebInputEvent& event) {
          return event.GetType() == blink::WebGestureEvent::Type::kTouchMove &&
                 state == blink::mojom::InputEventResultState::kNotConsumed;
        }));
    GetRenderWidgetHost()->ForwardTouchEventWithLatencyInfo(touch,
                                                            ui::LatencyInfo());
    touch_move_waiter.Wait();

    blink::WebGestureEvent scroll_begin =
        blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
            1, 1, blink::WebGestureDevice::kTouchscreen);
    GetRenderWidgetHost()->ForwardGestureEventWithLatencyInfo(
        scroll_begin, ui::LatencyInfo());
    // Scroll begin ignores ack disposition, so don't wait for the ack.
    WaitAFrame();

    // First touchmove already sent, start at 2.
    for (int i = 2; i <= 10; ++i) {
      // Send a touch move, followed by a scroll update
      touch.MovePoint(0, bounds.x() + 20 + i * dx, bounds.y() + 100);
      GetRenderWidgetHost()->ForwardTouchEventWithLatencyInfo(
          touch, ui::LatencyInfo());
      WaitAFrame();

      blink::WebGestureEvent scroll_update =
          blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
              dx, 5, 0, blink::WebGestureDevice::kTouchscreen);

      GetRenderWidgetHost()->ForwardGestureEventWithLatencyInfo(
          scroll_update, ui::LatencyInfo());

      WaitAFrame();
    }

    touch.ReleasePoint(0);
    GetRenderWidgetHost()->ForwardTouchEventWithLatencyInfo(touch,
                                                            ui::LatencyInfo());
    WaitAFrame();

    blink::WebGestureEvent scroll_end(
        blink::WebInputEvent::Type::kGestureScrollEnd,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    GetRenderWidgetHost()->ForwardGestureEventWithLatencyInfo(
        scroll_end, ui::LatencyInfo());
    WaitAFrame();

    if (!navigated)
      EXPECT_EQ(10, EvalJs(shell(), "touchmoveCount"));
    else
      EXPECT_GT(10, EvalJs(shell(), "touchmoveCount"));
  }
}

// Tests that running the drop callback will perform drop.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, GetDropCallback_Run) {
  StartTestWithPage("/simple_page.html");
  WebContentsImpl* contents = GetWebContentsImpl();
  WebContentsViewAura* view = GetWebContentsViewAura();

  view->SetDragDestDelegateForTesting(&drag_dest_delegate_);

  std::unique_ptr<ui::OSExchangeData> data =
      std::make_unique<ui::OSExchangeData>();
  view->RegisterDropCallbackForTesting(base::BindOnce(
      &WebContentsViewAuraTest::OnDropComplete, base::Unretained(this)));
  base::RunLoop run_loop;
  async_drop_closure_ = run_loop.QuitClosure();

  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(*data.get(), point, point,
                            ui::DragDropTypes::DRAG_COPY);
  view->OnDragEntered(event);
  EXPECT_TRUE(drag_dest_delegate_.GetDragInitializeCalled());
  auto drop_cb = view->GetDropCallback(event);
  ASSERT_TRUE(drop_cb);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(std::move(data), output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  run_loop.Run();

  EXPECT_EQ(1, drag_dest_delegate_.GetOnDropCalledCount());
  EXPECT_FALSE(drag_dest_delegate_.GetOnDragLeaveCalled());
  EXPECT_EQ(drop_target_widget_,
            RenderWidgetHostImpl::From(contents->GetPrimaryFrameTree()
                                           .root()
                                           ->current_frame_host()
                                           ->GetRenderWidgetHost()));
}

// Tests that resetting the drop callback won't complete the drop and will exit
// the drag insead.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest, GetDropCallback_Cancelled) {
  StartTestWithPage("/simple_page.html");
  WebContentsViewAura* view = GetWebContentsViewAura();

  view->SetDragDestDelegateForTesting(&drag_dest_delegate_);

  std::unique_ptr<ui::OSExchangeData> data =
      std::make_unique<ui::OSExchangeData>();
  view->RegisterDropCallbackForTesting(base::BindOnce(
      &WebContentsViewAuraTest::OnDropComplete, base::Unretained(this)));

  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(*data.get(), point, point,
                            ui::DragDropTypes::DRAG_COPY);
  view->OnDragEntered(event);
  EXPECT_TRUE(drag_dest_delegate_.GetDragInitializeCalled());
  auto drop_cb = view->GetDropCallback(event);
  ASSERT_TRUE(drop_cb);
  drop_cb.Reset();

  EXPECT_EQ(0, drag_dest_delegate_.GetOnDropCalledCount());
  EXPECT_TRUE(drag_dest_delegate_.GetOnDragLeaveCalled());
}

}  // namespace content
