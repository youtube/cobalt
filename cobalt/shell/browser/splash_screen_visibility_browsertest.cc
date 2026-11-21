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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/browser/features.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kStartupVisibilityPath[] = "/startup_visibility.html";

std::unique_ptr<net::test_server::HttpResponse> HandleStartupVisibilityRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kStartupVisibilityPath) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(R"(<!doctype html>
<html>
<head>
<script>
  window.__initialVisibility = document.visibilityState;
  window.__visibilityChanges = [];
  document.addEventListener('visibilitychange', () => {
    window.__visibilityChanges.push(document.visibilityState);
  });
</script>
</head>
<body>Main Page</body>
</html>)");
  return response;
}

// Records every WebContents visibility transition on the UI thread for the
// observed WebContents lifetime.
class VisibilityHistoryObserver : public WebContentsObserver {
 public:
  explicit VisibilityHistoryObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void OnVisibilityChanged(Visibility visibility) override {
    seen_visibilities_.push_back(visibility);
  }

  const std::vector<Visibility>& seen_visibilities() const {
    return seen_visibilities_;
  }

 private:
  std::vector<Visibility> seen_visibilities_;
};

// Blocks on a RunLoop on the UI thread until the observed WebContents is
// destroyed (used to wait deterministically for splash screen dismissal).
class WebContentsDestroyedWaiter : public WebContentsObserver {
 public:
  explicit WebContentsDestroyedWaiter(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() {
    if (destroyed_) {
      return;
    }
    run_loop_.Run();
  }

  void WebContentsDestroyed() override {
    destroyed_ = true;
    run_loop_.Quit();
  }

 private:
  bool destroyed_ = false;
  base::RunLoop run_loop_;
};

// Returns true if |splash_window| is stacked above |main_window| inside
// |host_window|'s Aura child hierarchy (accounting for NativeViewHostAura's
// intermediate clipping windows).
bool IsStackedAboveInHost(aura::Window* host_window,
                          aura::Window* splash_window,
                          aura::Window* main_window) {
  if (!host_window || !splash_window || !main_window) {
    return false;
  }
  auto find_direct_child = [host_window](aura::Window* descendant) {
    aura::Window* current = descendant;
    while (current && current->parent() != host_window) {
      current = current->parent();
    }
    return current;
  };
  aura::Window* splash_child = find_direct_child(splash_window);
  aura::Window* main_child = find_direct_child(main_window);
  if (!splash_child || !main_child || splash_child == main_child) {
    return false;
  }
  const auto& children = host_window->children();
  auto splash_it = std::ranges::find(children, splash_child);
  auto main_it = std::ranges::find(children, main_child);
  return splash_it != children.end() && main_it != children.end() &&
         splash_it > main_it;
}

// Base browsertest fixture configuring the image splash screen timeout and an
// embedded test server page that records bootstrap visibility state.
class SplashScreenVisibilityBrowserTestBase : public ContentBrowserTest {
 public:
  explicit SplashScreenVisibilityBrowserTestBase(const char* splash_timeout) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{cobalt::features::kSplashScreenConfig,
          {{cobalt::features::kSplashScreenTimeoutParam.name,
            splash_timeout}}}},
        {cobalt::features::kDisableSplashScreen,
         cobalt::features::kForceVideoSplashScreen});
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleStartupVisibilityRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Fixture using a short splash timeout so normal startup and dismissal run
// end-to-end quickly.
class SplashScreenVisibilityBrowserTest
    : public SplashScreenVisibilityBrowserTestBase {
 public:
  SplashScreenVisibilityBrowserTest()
      : SplashScreenVisibilityBrowserTestBase("200ms") {}
};

// Fixture using a long splash timeout so conceal and re-overlay transitions
// execute deterministically before automatic splash dismissal.
class SplashScreenVisibilityLongTimeoutBrowserTest
    : public SplashScreenVisibilityBrowserTestBase {
 public:
  SplashScreenVisibilityLongTimeoutBrowserTest()
      : SplashScreenVisibilityBrowserTestBase("30s") {}
};

// User journey: A user launches Cobalt in the foreground with the native splash
// screen enabled. While the splash screen overlays the window and until it
// dismisses, the main web app must stay visible (never hidden or occluded by
// Aura occlusion tracking) so it boots as a foreground launch with
// document.visibilityState == "visible" and no spurious visibilitychange
// transitions.
IN_PROC_BROWSER_TEST_F(SplashScreenVisibilityBrowserTest,
                       MainWebContentsStaysVisibleDuringSplashScreen) {
  const GURL page_url = embedded_test_server()->GetURL(kStartupVisibilityPath);
  Shell* splash_shell = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), page_url, nullptr,
      gfx::Size(1280, 720), /*create_splash_screen_web_contents=*/true);
  ASSERT_NE(splash_shell, nullptr);
  ASSERT_NE(splash_shell->web_contents(), nullptr);
  ASSERT_NE(splash_shell->splash_screen_web_contents(), nullptr);

  VisibilityHistoryObserver visibility_observer(splash_shell->web_contents());
  WebContentsDestroyedWaiter splash_destroyed_waiter(
      splash_shell->splash_screen_web_contents());

  aura::Window* host_window =
      Shell::GetPlatform()->GetNativeWindow(splash_shell);
  aura::Window* main_window = splash_shell->web_contents()->GetNativeView();
  aura::Window* splash_window =
      splash_shell->splash_screen_web_contents()->GetNativeView();
  ASSERT_NE(host_window, nullptr);
  ASSERT_NE(main_window, nullptr);
  ASSERT_NE(splash_window, nullptr);

  // Splash must be stacked visually on top of main, yet main must remain
  // unoccluded and visible because the splash window is excluded from Aura
  // occlusion tracking.
  EXPECT_TRUE(IsStackedAboveInHost(host_window, splash_window, main_window));
  EXPECT_EQ(main_window->GetOcclusionState(),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(splash_shell->web_contents()->GetVisibility(), Visibility::VISIBLE);

  EXPECT_TRUE(WaitForLoadStop(splash_shell->web_contents()));
  splash_destroyed_waiter.Wait();

  EXPECT_EQ(splash_shell->splash_screen_web_contents(), nullptr);
  EXPECT_EQ(splash_shell->web_contents()->GetVisibility(), Visibility::VISIBLE);
  EXPECT_EQ(main_window->GetOcclusionState(),
            aura::Window::OcclusionState::VISIBLE);

  for (Visibility visibility : visibility_observer.seen_visibilities()) {
    EXPECT_EQ(visibility, Visibility::VISIBLE);
  }

  EXPECT_EQ("visible",
            EvalJs(splash_shell->web_contents(), "window.__initialVisibility"));
  EXPECT_EQ(0, EvalJs(splash_shell->web_contents(),
                      "window.__visibilityChanges.length"));

  splash_shell->Close();
}

// User journey: A user launches Cobalt in the foreground and the OS conceals
// and reveals the window while the splash screen is still displayed. Concealing
// the widget must still hide the main WebContents (excluding the splash window
// from occlusion must not suppress widget-level conceal), and re-displaying the
// splash overlay on reveal must keep the splash stacked above main without
// duplicating Aura occlusion exclusion.
IN_PROC_BROWSER_TEST_F(SplashScreenVisibilityLongTimeoutBrowserTest,
                       ConcealAndReoverlayDuringSplashScreen) {
  const GURL page_url = embedded_test_server()->GetURL(kStartupVisibilityPath);
  Shell* splash_shell = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), page_url, nullptr,
      gfx::Size(1280, 720), /*create_splash_screen_web_contents=*/true);
  ASSERT_NE(splash_shell, nullptr);
  ASSERT_NE(splash_shell->web_contents(), nullptr);
  ASSERT_NE(splash_shell->splash_screen_web_contents(), nullptr);

  EXPECT_EQ(splash_shell->web_contents()->GetVisibility(), Visibility::VISIBLE);

  // Concealing the shell widget hides both the main and splash WebContents.
  Shell::GetPlatform()->ConcealShell(splash_shell);
  EXPECT_EQ(splash_shell->web_contents()->GetVisibility(), Visibility::HIDDEN);

  // Re-displaying the splash overlay (as on reveal during splash) restores
  // visibility without hitting a duplicate ScopedExclude DCHECK and keeps
  // splash stacked above the existing main WebView.
  Shell::GetPlatform()->LoadSplashScreenContents(splash_shell);

  aura::Window* host_window =
      Shell::GetPlatform()->GetNativeWindow(splash_shell);
  aura::Window* main_window = splash_shell->web_contents()->GetNativeView();
  aura::Window* splash_window =
      splash_shell->splash_screen_web_contents()->GetNativeView();
  EXPECT_TRUE(IsStackedAboveInHost(host_window, splash_window, main_window));
  EXPECT_EQ(main_window->GetOcclusionState(),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(splash_shell->web_contents()->GetVisibility(), Visibility::VISIBLE);

  splash_shell->Close();
}

}  // namespace

}  // namespace content
