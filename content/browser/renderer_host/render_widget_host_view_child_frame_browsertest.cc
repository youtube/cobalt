// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/mock_display_feature.h"
#include "content/test/portal/portal_created_observer.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

class RenderWidgetHostViewChildFrameBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewChildFrameBrowserTest() = default;

  RenderWidgetHostViewChildFrameBrowserTest(
      const RenderWidgetHostViewChildFrameBrowserTest&) = delete;
  RenderWidgetHostViewChildFrameBrowserTest& operator=(
      const RenderWidgetHostViewChildFrameBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Tests that the FrameSinkId of each child frame has been updated by the
  // `blink::RemoteFrame`.
  void CheckFrameSinkId(RenderFrameHost* render_frame_host) {
    RenderWidgetHostViewBase* child_view =
        static_cast<RenderFrameHostImpl*>(render_frame_host)
            ->GetRenderWidgetHost()
            ->GetView();
    // Only interested in updated FrameSinkIds on child frames.
    if (!child_view || !child_view->IsRenderWidgetHostViewChildFrame())
      return;

    // Ensure that the received viz::FrameSinkId was correctly set on the child
    // frame.
    viz::FrameSinkId actual_frame_sink_id_ = child_view->GetFrameSinkId();
    EXPECT_EQ(expected_frame_sink_id_, actual_frame_sink_id_);

    // The viz::FrameSinkID will be replaced while the test blocks for
    // navigation. It should differ from the information stored in the child's
    // RenderWidgetHost.
    EXPECT_NE(base::checked_cast<uint32_t>(
                  child_view->GetRenderWidgetHost()->GetProcess()->GetID()),
              actual_frame_sink_id_.client_id());
    EXPECT_NE(base::checked_cast<uint32_t>(
                  child_view->GetRenderWidgetHost()->GetRoutingID()),
              actual_frame_sink_id_.sink_id());
  }

  Portal* CreatePortalToUrl(WebContentsImpl* host_contents,
                            GURL portal_url,
                            int number_of_navigations) {
    EXPECT_GE(number_of_navigations, 1);
    RenderFrameHostImpl* main_frame = host_contents->GetPrimaryMainFrame();

    // Create portal and wait for navigation.
    PortalCreatedObserver portal_created_observer(main_frame);
    TestNavigationObserver navigation_observer(nullptr, number_of_navigations);
    navigation_observer.set_wait_event(
        TestNavigationObserver::WaitEvent::kNavigationFinished);
    navigation_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("{"
                              "  let portal = document.createElement('portal');"
                              "  portal.src = $1;"
                              "  portal.setAttribute('id', 'portal');"
                              "  document.body.appendChild(portal);"
                              "}",
                              portal_url)));
    Portal* portal = portal_created_observer.WaitUntilPortalCreated();
    navigation_observer.StopWatchingNewWebContents();

    WebContentsImpl* portal_contents = portal->GetPortalContents();
    EXPECT_TRUE(portal_contents);

    navigation_observer.WaitForNavigationFinished();
    EXPECT_TRUE(WaitForLoadStop(portal_contents));

    return portal;
  }

  void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  void set_expected_frame_sink_id(viz::FrameSinkId frame_sink_id) {
    expected_frame_sink_id_ = frame_sink_id;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{blink::features::kPortals};
  viz::FrameSinkId expected_frame_sink_id_;
};

// Tests that the screen is properly reflected for RWHVChildFrame.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest, Screen) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), cross_site_url));

  int main_frame_screen_width =
      ExecuteScriptAndGetValue(shell()->web_contents()->GetPrimaryMainFrame(),
                               "window.screen.width")
          .GetInt();
  EXPECT_NE(main_frame_screen_width, 0);

  shell()->web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](RenderFrameHost* frame_host) {
        int width = ExecuteScriptAndGetValue(frame_host, "window.screen.width")
                        .GetInt();
        EXPECT_EQ(width, main_frame_screen_width);
      });
}

// Auto-resize is only implemented for Ash and GuestViews. So we need to inject
// an implementation that actually resizes the top level widget.
class AutoResizeWebContentsDelegate : public WebContentsDelegate {
  void ResizeDueToAutoResize(WebContents* web_contents,
                             const gfx::Size& new_size) override {
    RenderWidgetHostView* view =
        web_contents->GetTopLevelRenderWidgetHostView();
    view->SetSize(new_size);
  }
};

// Test that the |visible_viewport_size| from the top frame is propagated to all
// local roots (aka RenderWidgets):
// a) Initially upon adding the child frame (we create this scenario by
// navigating a child on b.com to c.com, where we already have a RenderProcess
// active for c.com).
// b) When resizing the top level widget.
// c) When auto-resize is enabled for the top level main frame and the renderer
// resizes the top level widget.
// d) When auto-resize is enabled for the nested main frame and the renderer
// resizes the nested widget.
// See https://crbug.com/726743 and https://crbug.com/1050635.
// TODO(crbug.com/1315346): Flaky on Android and Linux.
// TODO(crbug.com/1341838): Flaky on Mac (Sheriff 2022-07-04)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_VisualPropertiesPropagation_VisibleViewportSize \
  DISABLED_VisualPropertiesPropagation_VisibleViewportSize
#else
#define MAYBE_VisualPropertiesPropagation_VisibleViewportSize \
  VisualPropertiesPropagation_VisibleViewportSize
#endif
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest,
                       MAYBE_VisualPropertiesPropagation_VisibleViewportSize) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  RenderWidgetHostView* root_view =
      root->current_frame_host()->GetRenderWidgetHost()->GetView();

  // We wait for the main frame and the two subframes.
  int number_of_navigations = 3;
  Portal* portal =
      CreatePortalToUrl(web_contents, main_url, number_of_navigations);
  WebContentsImpl* nested_contents = portal->GetPortalContents();
  FrameTreeNode* nested_root = nested_contents->GetPrimaryFrameTree().root();
  RenderWidgetHostView* nested_root_view =
      nested_root->current_frame_host()->GetRenderWidgetHost()->GetView();

  // Watch processes for a.com and c.com, as we will throw away b.com when we
  // navigate it below.
  auto* root_rwh = root->current_frame_host()->GetRenderWidgetHost();
  auto* child_rwh =
      root->child_at(1)->current_frame_host()->GetRenderWidgetHost();
  ASSERT_NE(root_rwh->GetProcess(), child_rwh->GetProcess());

  auto* nested_root_rwh =
      nested_root->current_frame_host()->GetRenderWidgetHost();
  auto* nested_child_rwh =
      nested_root->child_at(1)->current_frame_host()->GetRenderWidgetHost();
  ASSERT_NE(nested_root_rwh->GetProcess(), nested_child_rwh->GetProcess());

  const gfx::Size initial_size = root_view->GetVisibleViewportSize();
  const gfx::Size nested_initial_size =
      nested_root_view->GetVisibleViewportSize();
  ASSERT_NE(initial_size, nested_initial_size);

  // We should see the top level widget's size in the visible_viewport_size
  // in both local roots. When a child local root is added in the parent
  // renderer, the value is propagated down through the browser to the child
  // renderer's RenderWidget.
  //
  // This property is not directly visible in the renderer, so we can only
  // check that the value is sent to the appropriate RenderWidget.
  {
    GURL cross_site_url(
        embedded_test_server()->GetURL("c.com", "/title2.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), cross_site_url));

    // Wait to see the size sent to the child RenderWidget.
    while (true) {
      absl::optional<blink::VisualProperties> properties =
          child_rwh->LastComputedVisualProperties();
      if (properties && properties->visible_viewport_size == initial_size)
        break;
      base::RunLoop().RunUntilIdle();
    }
  }

  // Same check as above but for a nested WebContents.
  {
    GURL cross_site_url(
        embedded_test_server()->GetURL("c.com", "/title2.html"));
    EXPECT_TRUE(
        NavigateToURLFromRenderer(nested_root->child_at(0), cross_site_url));

    // Wait to see the size sent to the child RenderWidget.
    while (true) {
      absl::optional<blink::VisualProperties> properties =
          nested_child_rwh->LastComputedVisualProperties();
      if (properties &&
          properties->visible_viewport_size == nested_initial_size)
        break;
      base::RunLoop().RunUntilIdle();
    }
  }

// This part of the test does not work well on Android, for a few reasons:
// 1. RenderWidgetHostViewAndroid can not be resized, the Java objects need to
// be resized somehow through ui::ViewAndroid.
// 2. AutoResize on Android does not size to the min/max bounds specified, it
// ends up ignoring them and sizing to the screen (I think).
// Luckily this test is verifying interactions and behaviour of
// RenderWidgetHostImpl - RenderWidget - `blink::RemoteFrame` -
// CrossProcessFrameConnector, and this isn't Android-specific code.
#if !BUILDFLAG(IS_ANDROID)

  // Resize the top level widget to cause its |visible_viewport_size| to be
  // changed. The change should propagate down to the child RenderWidget.
  {
    const gfx::Size resize_to(initial_size.width() - 10,
                              initial_size.height() - 10);

    root_view->SetSize(resize_to);

    // Wait to see both RenderWidgets receive the message.
    while (true) {
      absl::optional<blink::VisualProperties> properties =
          root_rwh->LastComputedVisualProperties();
      if (properties && properties->visible_viewport_size == resize_to)
        break;
      base::RunLoop().RunUntilIdle();
    }
    while (true) {
      absl::optional<blink::VisualProperties> properties =
          child_rwh->LastComputedVisualProperties();
      if (properties && properties->visible_viewport_size == resize_to)
        break;
      base::RunLoop().RunUntilIdle();
    }
  }

  // Same check as above but resizing the nested WebContents' main frame
  // instead.
  // Resize the top level widget to cause its |visible_viewport_size| to be
  // changed. The change should propagate down to the child RenderWidget.
  {
    const gfx::Size resize_to(nested_initial_size.width() - 10,
                              nested_initial_size.height() - 10);

    EXPECT_TRUE(ExecJs(
        root->current_frame_host(),
        JsReplace("document.getElementById('portal').style.width = '$1px';"
                  "document.getElementById('portal').style.height = '$2px';",
                  resize_to.width(), resize_to.height())));

    // Wait to see both RenderWidgets receive the message.
    while (true) {
      absl::optional<blink::VisualProperties> properties =
          nested_root_rwh->LastComputedVisualProperties();
      if (properties && properties->visible_viewport_size == resize_to)
        break;
      base::RunLoop().RunUntilIdle();
    }
    while (true) {
      absl::optional<blink::VisualProperties> properties =
          nested_child_rwh->LastComputedVisualProperties();
      if (properties && properties->visible_viewport_size == resize_to)
        break;
      base::RunLoop().RunUntilIdle();
    }
  }

  // Informs the top-level frame it can auto-resize. It will shrink down to its
  // minimum window size based on the empty content we've loaded, which is
  // 100x100.
  //
  // When the renderer resizes, thanks to our AutoResizeWebContentsDelegate
  // the top level widget will resize. That size will be reflected in the
  // visible_viewport_size which is sent back through the top level RenderWidget
  // to propagte down to child local roots.
  //
  // This property is not directly visible in the renderer, so we can only
  // check that the value is sent to both RenderWidgets.
  {
    const gfx::Size auto_resize_to(105, 100);

    // Replace the WebContentsDelegate so that we can use the auto-resize
    // changes to adjust the size of the top widget.
    WebContentsDelegate* old_delegate = shell()->web_contents()->GetDelegate();
    AutoResizeWebContentsDelegate resize_delegate;
    shell()->web_contents()->SetDelegate(&resize_delegate);

    root_view->EnableAutoResize(auto_resize_to, auto_resize_to);

    // Wait for the renderer side to resize itself and the RenderWidget
    // waterfall to pass the new |visible_viewport_size| down.
    while (true) {
      absl::optional<blink::VisualProperties> properties =
          root_rwh->LastComputedVisualProperties();
      if (properties && properties->visible_viewport_size == auto_resize_to)
        break;
      base::RunLoop().RunUntilIdle();
    }
    while (true) {
      absl::optional<blink::VisualProperties> properties =
          child_rwh->LastComputedVisualProperties();
      if (properties && properties->visible_viewport_size == auto_resize_to)
        break;
      base::RunLoop().RunUntilIdle();
    }

    shell()->web_contents()->SetDelegate(old_delegate);
  }

  // TODO(danakj): We'd like to run the same check as above but tell the main
  // frame of the nested WebContents that it can auto-resize. However this seems
  // to get through to the main frame's RenderWidget and propagate correctly but
  // no size change occurs in the renderer.
#endif
}

// Validate that OOPIFs receive presentation feedbacks.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest,
                       PresentationFeedback) {
  base::HistogramTester histogram_tester;
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), cross_site_url));

  const auto trigger_subframe_tab_switch = [&root]() -> bool {
    auto* child_rwh_impl =
        root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
    // Hide the frame and make it visible again, to force it to record the
    // tab-switch time, which is generated from presentation-feedback.
    child_rwh_impl->WasHidden();
    child_rwh_impl->WasShown(
        blink::mojom::RecordContentToVisibleTimeRequest::New(
            base::TimeTicks::Now(),
            /*destination_is_loaded=*/true,
            /*show_reason_tab_switching=*/true,
            /*show_reason_bfcache_restore=*/false));
    // Force the child to submit a new frame.
    return ExecJs(root->child_at(0)->current_frame_host(),
                  "document.write('Force a new frame.');");
  };
  ASSERT_TRUE(trigger_subframe_tab_switch());

  // Ensure the loop starts in the right state.
  ASSERT_TRUE(
      histogram_tester.GetAllSamples("Browser.Tabs.TotalSwitchDuration3")
          .empty());

  // Once the tab switch completes the PresentationFeedback should cause a
  // single TotalSwitchDuration3 histogram to be logged.
  bool got_incomplete_tab_switch = false;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (histogram_tester.GetAllSamples("Browser.Tabs.TotalSwitchDuration3")
             .empty()) {
    ASSERT_LT(base::TimeTicks::Now() - start_time,
              TestTimeouts::action_timeout())
        << "Timed out waiting for Browser.Tabs.TotalSwitchDuration3.";
    FetchHistogramsFromChildProcesses();
    GiveItSomeTime();

    // Work around a race condition while loading the cross-site iframe.
    //
    // The NavigateToURLFromRenderer call above replaces the
    // LocalFrame/LocalFrameView in renderer process A with a
    // RemoteFrame/RemoteFrameView proxy for the frame which is now hosted in
    // renderer process B. During initialization the RemoteFrameView sends a
    // series of VisibilityChanged messages to the browser process, which cause
    // CrossProcessFrameConnector to call WasHidden and then WasShown on
    // `child_rwh_impl`. Depending on the timing these might arrive during the
    // NavigateToURLFromRenderer call or the ExecJS call above, both of which
    // pump the message loop. If CrossProcessFrameConnector calls WasHidden
    // after the WasShown call above, it will cancel the simulated tab switch.
    // This causes ContentToVisibleTimeReporter to log
    // TotalIncompleteSwitchDuration3, which is not based on
    // PresentationFeedback, instead of TotalSwitchDuration3. See
    // crbug.com/1288560 for more details.
    //
    // The race condition can only cause a single incomplete tab switch, so
    // only check for this once. If the second simulated tab switch is also
    // cancelled something else is wrong, so the loop will time out and fail
    // the test.
    //
    // TODO(crbug.com/1288560): Remove this once the race condition is
    // fixed.
    if (!got_incomplete_tab_switch &&
        !histogram_tester
             .GetAllSamples("Browser.Tabs.TotalIncompleteSwitchDuration3")
             .empty()) {
      LOG(ERROR) << "Incomplete tab switch - try again.";
      got_incomplete_tab_switch = true;
      ASSERT_TRUE(trigger_subframe_tab_switch());
    }
  }
}

// Auto-resize is only implemented for Ash and GuestViews. So we need to inject
// an implementation that actually resizes the top level widget.
class DisplayModeControllingWebContentsDelegate : public WebContentsDelegate {
 public:
  blink::mojom::DisplayMode GetDisplayMode(
      const WebContents* web_contents) override {
    return mode_;
  }

  void set_display_mode(blink::mojom::DisplayMode mode) { mode_ = mode; }

 private:
  // The is the default value throughout the browser and renderer.
  blink::mojom::DisplayMode mode_ = blink::mojom::DisplayMode::kBrowser;
};

// TODO(crbug.com/1060336): Unlike most VisualProperties, the DisplayMode does
// not propagate down the tree of RenderWidgets, but is sent independently to
// each RenderWidget.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest,
                       VisualPropertiesPropagation_DisplayMode) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  DisplayModeControllingWebContentsDelegate display_mode_delegate;
  shell()->web_contents()->SetDelegate(&display_mode_delegate);

  // Main frame.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  RenderWidgetHostImpl* root_widget =
      root->current_frame_host()->GetRenderWidgetHost();
  // Out-of-process frame.
  FrameTreeNode* oopchild = root->child_at(0);
  RenderWidgetHostImpl* oopchild_widget =
      oopchild->current_frame_host()->GetRenderWidgetHost();
  // In-process frame.
  FrameTreeNode* ipchild = oopchild->child_at(0);
  RenderWidgetHostImpl* ipchild_widget =
      ipchild->current_frame_host()->GetRenderWidgetHost();
  EXPECT_NE(root_widget, ipchild_widget);

  // Check all frames for the initial value.
  EXPECT_EQ(
      true,
      EvalJs(root, "window.matchMedia('(display-mode: browser)').matches"));
  EXPECT_EQ(
      true,
      EvalJs(oopchild, "window.matchMedia('(display-mode: browser)').matches"));
  EXPECT_EQ(
      true,
      EvalJs(ipchild, "window.matchMedia('(display-mode: browser)').matches"));

  // The display mode changes.
  display_mode_delegate.set_display_mode(
      blink::mojom::DisplayMode::kStandalone);
  // Each RenderWidgetHost would need to hear about that by having
  // SynchronizeVisualProperties() called. It's not clear what triggers that but
  // the place that changes the DisplayMode would be responsible.
  //
  // We ignore the pending ack to ensure this IPC is sent immediately.
  EXPECT_TRUE(root_widget->SynchronizeVisualPropertiesIgnoringPendingAck());
  EXPECT_TRUE(oopchild_widget->SynchronizeVisualPropertiesIgnoringPendingAck());
  EXPECT_TRUE(ipchild_widget->SynchronizeVisualPropertiesIgnoringPendingAck());

  // Check all frames for the changed value.
  EXPECT_EQ(
      true,
      EvalJsAfterLifecycleUpdate(
          root, "", "window.matchMedia('(display-mode: standalone)').matches"));
  EXPECT_EQ(true,
            EvalJsAfterLifecycleUpdate(
                oopchild, "",
                "window.matchMedia('(display-mode: standalone)').matches"));
  EXPECT_EQ(true,
            EvalJsAfterLifecycleUpdate(
                ipchild, "",
                "window.matchMedia('(display-mode: standalone)').matches"));

  // Navigate a frame to b.com, which we already have a process for.
  GURL same_site_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), same_site_url));

  // The navigated frame sees the correct (non-default) value.
  EXPECT_EQ(true,
            EvalJs(root->child_at(0),
                   "window.matchMedia('(display-mode: standalone)').matches"));

  // Navigate the frame to c.com, which we don't have a process for.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), cross_site_url));

  // The navigated frame sees the correct (non-default) value.
  EXPECT_EQ(true,
            EvalJs(root->child_at(0),
                   "window.matchMedia('(display-mode: standalone)').matches"));
}

// Validate that the root widget's window segments are correctly propagated
// via the SynchronizeVisualProperties cascade.
// Flaky on Mac, Linux and Android (http://crbug/1089994).
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
#define MAYBE_VisualPropertiesPropagation_RootWindowSegments \
  DISABLED_VisualPropertiesPropagation_RootWindowSegments
#else
#define MAYBE_VisualPropertiesPropagation_RootWindowSegments \
  VisualPropertiesPropagation_RootWindowSegments
#endif
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest,
                       MAYBE_VisualPropertiesPropagation_RootWindowSegments) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  // Main frame/root view.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  RenderWidgetHostImpl* root_widget =
      root->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHostViewBase* root_view = root_widget->GetView();
  // Out-of-process child frame.
  FrameTreeNode* oopchild = root->child_at(0);
  // Out-of-process descendant frame (child of the first oop-iframe).
  FrameTreeNode* oopdescendant = oopchild->child_at(0);

  const gfx::Size root_view_size = root_view->GetVisibleViewportSize();
  const int kDisplayFeatureLength = 10;
  DisplayFeature emulated_display_feature{
      DisplayFeature::Orientation::kVertical,
      /* offset */ root_view_size.width() / 2 - kDisplayFeatureLength / 2,
      /* mask_length */ kDisplayFeatureLength};
  std::vector<gfx::Rect> expected_segments;
  expected_segments.emplace_back(0, 0, emulated_display_feature.offset,
                                 root_view_size.height());
  const int second_segment_offset =
      emulated_display_feature.offset + emulated_display_feature.mask_length;
  expected_segments.emplace_back(second_segment_offset, 0,
                                 root_view_size.width() - second_segment_offset,
                                 root_view_size.height());

  absl::optional<blink::VisualProperties> properties =
      oopchild->current_frame_host()
          ->GetRenderWidgetHost()
          ->LastComputedVisualProperties();
  EXPECT_TRUE(properties);
  EXPECT_TRUE(properties->local_surface_id);
  viz::LocalSurfaceId oopchild_initial_lsid =
      properties->local_surface_id.value();

  properties = oopdescendant->current_frame_host()
                   ->GetRenderWidgetHost()
                   ->LastComputedVisualProperties();
  EXPECT_TRUE(properties);
  EXPECT_TRUE(properties->local_surface_id);
  viz::LocalSurfaceId oopdescendant_initial_lsid =
      properties->local_surface_id.value();

  {
    // Watch for visual properties changes, first to the child oop-iframe, then
    // to the descendant (at which point we're done and can validate the
    // values).

    MockDisplayFeature mock_display_feature(root_view);
    mock_display_feature.SetDisplayFeature(&emulated_display_feature);
    root_widget->SynchronizeVisualProperties();

    while (true) {
      properties = oopchild->current_frame_host()
                       ->GetRenderWidgetHost()
                       ->LastComputedVisualProperties();
      if (properties && properties->local_surface_id &&
          oopchild_initial_lsid < properties->local_surface_id) {
        EXPECT_EQ(properties->root_widget_window_segments, expected_segments);
        break;
      }
      base::RunLoop().RunUntilIdle();
    }
    while (true) {
      properties = oopdescendant->current_frame_host()
                       ->GetRenderWidgetHost()
                       ->LastComputedVisualProperties();
      if (properties && properties->local_surface_id &&
          oopdescendant_initial_lsid < properties->local_surface_id) {
        EXPECT_EQ(properties->root_widget_window_segments, expected_segments);
        break;
      }
      base::RunLoop().RunUntilIdle();
    }
  }

  {
    // Creating a new local frame root (navigating a.com to c.com) should also
    // propagate the property to the new local frame root. Note that we're
    // re-using the existing RenderProcessHost from c.com (aka
    // |oopdescendant_rph|).
    GURL new_frame_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), new_frame_url));

    while (true) {
      properties = oopdescendant->current_frame_host()
                       ->GetRenderWidgetHost()
                       ->LastComputedVisualProperties();
      // This check is needed, since we'll get an IPC originating from
      // RenderWidgetHostImpl immediately after the frame is added with the
      // incorrect value (the segments are cascaded from the parent renderer
      // when the frame is added in that process). So we need to wait for
      // the outgoing VisualProperties triggered from the parent renderer
      // and comes in via the CrossProcessFrameConnector, which can happen
      // after NavigateToURLFromRenderer completes.
      if (properties &&
          properties->root_widget_window_segments == expected_segments)
        break;
      base::RunLoop().RunUntilIdle();
    }
  }
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest,
                       SetTextDirection) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  // Main frame.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  RenderWidgetHostImpl* root_widget =
      root->current_frame_host()->GetRenderWidgetHost();
  ASSERT_TRUE(ExecJs(root->current_frame_host(),
                     "var elem = document.createElement('input'); "
                     "elem.id = 'mainframe_input_id';"
                     "document.body.appendChild(elem);"
                     "document.getElementById('mainframe_input_id').focus();"));
  root_widget->UpdateTextDirection(base::i18n::RIGHT_TO_LEFT);
  root_widget->NotifyTextDirection();
  std::string mainframe_input_element_dir =
      ExecuteScriptAndGetValue(
          root->current_frame_host(),
          "document.getElementById('mainframe_input_id').dir")
          .GetString();
  EXPECT_EQ(mainframe_input_element_dir, "rtl");

  // In-process frame.
  FrameTreeNode* ipchild = root->child_at(0);
  RenderWidgetHostImpl* ipchild_widget =
      ipchild->current_frame_host()->GetRenderWidgetHost();
  ASSERT_TRUE(ExecJs(ipchild->current_frame_host(),
                     "var elem = document.createElement('input'); "
                     "elem.id = 'ipchild_input_id';"
                     "document.body.appendChild(elem);"
                     "document.getElementById('ipchild_input_id').focus();"));
  ipchild_widget->UpdateTextDirection(base::i18n::LEFT_TO_RIGHT);
  ipchild_widget->NotifyTextDirection();
  std::string ip_input_element_dir =
      ExecuteScriptAndGetValue(
          ipchild->current_frame_host(),
          "document.getElementById('ipchild_input_id').dir")
          .GetString();
  EXPECT_EQ(ip_input_element_dir, "ltr");

  // Out-of-process frame.
  FrameTreeNode* oopchild = root->child_at(1);
  RenderWidgetHostImpl* oopchild_widget =
      oopchild->current_frame_host()->GetRenderWidgetHost();
  ASSERT_TRUE(ExecJs(oopchild->current_frame_host(),
                     "var elem = document.createElement('input'); "
                     "elem.id = 'oop_input_id';"
                     "document.body.appendChild(elem);"
                     "document.getElementById('oop_input_id').focus();"));
  oopchild_widget->UpdateTextDirection(base::i18n::RIGHT_TO_LEFT);
  oopchild_widget->NotifyTextDirection();
  std::string oop_input_element_dir =
      ExecuteScriptAndGetValue(oopchild->current_frame_host(),
                               "document.getElementById('oop_input_id').dir")
          .GetString();
  EXPECT_EQ(oop_input_element_dir, "rtl");

  // In case of UNKNOWN_DIRECTION, old value of direction is maintained.
  oopchild_widget->UpdateTextDirection(base::i18n::UNKNOWN_DIRECTION);
  oopchild_widget->NotifyTextDirection();
  oop_input_element_dir =
      ExecuteScriptAndGetValue(oopchild->current_frame_host(),
                               "document.getElementById('oop_input_id').dir")
          .GetString();
  EXPECT_EQ(oop_input_element_dir, "rtl");
}

}  // namespace

}  // namespace content
