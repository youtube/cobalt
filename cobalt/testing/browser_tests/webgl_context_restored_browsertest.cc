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

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "gpu/ipc/common/memory_stats.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_APPLE)
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_utils.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "ui/android/window_android.h"
#endif

namespace cobalt {
namespace {

constexpr char kWebGLTestPath[] = "/webgl_restore";

// Minimal WebGL page that handles `webglcontextlost` (calling preventDefault)
// and `webglcontextrestored`, drawing a distinct clear color after initial
// creation and after each restoration and verifying rendered pixels via
// readPixels.
constexpr char kWebGLTestHtml[] = R"html(
<!DOCTYPE html>
<html>
<body>
<canvas id="c" width="64" height="64"></canvas>
<script>
const canvas = document.getElementById('c');
window.__lostCount = 0;
window.__restoredCount = 0;
window.__freezeCount = 0;
window.__lastPixel = '';
window.__lostWaiters = [];
window.__restoredWaiters = [];

document.addEventListener('freeze', () => {
  window.__freezeCount += 1;
});

function notifyWaiters(waiters, count) {
  for (let i = waiters.length - 1; i >= 0; --i) {
    if (count >= waiters[i].target) {
      const resolve = waiters[i].resolve;
      waiters.splice(i, 1);
      resolve(count);
    }
  }
}

function drawAndRead(gl, r, g, b) {
  gl.viewport(0, 0, 64, 64);
  gl.clearColor(r, g, b, 1.0);
  gl.clear(gl.COLOR_BUFFER_BIT);
  const px = new Uint8Array(4);
  gl.readPixels(32, 32, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px);
  window.__lastPixel = `${px[0]},${px[1]},${px[2]},${px[3]}`;
  return window.__lastPixel;
}

const gl = canvas.getContext('webgl');
window.__gl = gl;

canvas.addEventListener('webglcontextlost', (e) => {
  e.preventDefault();
  window.__lostCount += 1;
  notifyWaiters(window.__lostWaiters, window.__lostCount);
});

canvas.addEventListener('webglcontextrestored', () => {
  window.__restoredCount += 1;
  if (window.__restoredCount === 1) {
    drawAndRead(window.__gl, 0.0, 0.0, 1.0);
  } else {
    drawAndRead(window.__gl, 1.0, 0.0, 0.0);
  }
  notifyWaiters(window.__restoredWaiters, window.__restoredCount);
});

window.__initResult = gl ? drawAndRead(gl, 0.0, 1.0, 0.0) : 'NO_WEBGL';

window.waitForLost = (target, timeoutMs) => new Promise((resolve) => {
  if (window.__lostCount >= target) {
    resolve(window.__lostCount);
    return;
  }
  const entry = {target, resolve};
  window.__lostWaiters.push(entry);
  setTimeout(() => {
    const idx = window.__lostWaiters.indexOf(entry);
    if (idx !== -1) {
      window.__lostWaiters.splice(idx, 1);
      resolve(window.__lostCount);
    }
  }, timeoutMs);
});

window.waitForRestored = (target, timeoutMs) => new Promise((resolve) => {
  if (window.__restoredCount >= target) {
    resolve(window.__restoredCount);
    return;
  }
  const entry = {target, resolve};
  window.__restoredWaiters.push(entry);
  setTimeout(() => {
    const idx = window.__restoredWaiters.indexOf(entry);
    if (idx !== -1) {
      window.__restoredWaiters.splice(idx, 1);
      resolve(window.__restoredCount);
    }
  }, timeoutMs);
});
</script>
</body>
</html>
)html";

std::unique_ptr<net::test_server::HttpResponse> HandleWebGLTestRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kWebGLTestPath) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(kWebGLTestHtml);
  return response;
}

}  // namespace

// Browser test fixture exercising Cobalt's single-process conceal/reveal GPU
// lifecycle with an active WebGL page. Runs on the Browser UI thread and uses
// in-memory HTTP responses from `embedded_test_server()` so it works across
// desktop Linux and Evergreen devices without relying on disk test data.
class WebGLContextRestoredBrowserTest : public content::ContentBrowserTest {
 public:
  WebGLContextRestoredBrowserTest() = default;
  ~WebGLContextRestoredBrowserTest() override = default;

  void SetUp() override {
    EnablePixelOutput();
    content::ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    if (!command_line->HasSwitch(switches::kSingleProcess)) {
      command_line->AppendSwitch(switches::kSingleProcess);
    }
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleWebGLTestRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  // Performs a synchronous Mojo round-trip to the renderer so prior async
  // visibility updates have reached Blink before proceeding. Returns false if
  // the renderer main thread did not respond within `timeout` (for example,
  // if blocked on a queued sync GPU channel request while concealed).
  bool FlushRendererProcessStateWithTimeout(base::TimeDelta timeout) {
    base::RunLoop loop;
    bool responded = false;
    base::OneShotTimer watchdog;
    watchdog.Start(FROM_HERE, timeout,
                   base::BindLambdaForTesting([&]() { loop.Quit(); }));
    auto* rph = static_cast<content::RenderProcessHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
    rph->GetRendererInterface()->PurgeResourceCache(
        base::BindLambdaForTesting([&]() {
          responded = true;
          watchdog.Stop();
          loop.Quit();
        }));
    loop.Run();
    return responded;
  }

  // Dispatches a GPU main-thread barrier through `GpuProcessHost::CallOnUI` so
  // it is strictly ordered after `content::RestoreGpuProcessOnUI()`.
  void FlushGpuMainThread() {
    base::RunLoop loop;
    content::GpuProcessHost::CallOnUI(
        FROM_HERE, content::GPU_PROCESS_KIND_SANDBOXED,
        /*force_create=*/false,
        base::BindLambdaForTesting([&](content::GpuProcessHost* host) {
          ASSERT_NE(host, nullptr);
          host->gpu_service()->GetVideoMemoryUsageStats(
              base::BindLambdaForTesting(
                  [&](const gpu::VideoMemoryUsageStats&) { loop.Quit(); }));
        }));
    loop.Run();
  }

  // Simulates Cobalt's full conceal GPU teardown, releasing all GPU channels,
  // shared raster state, surfaces, and terminating the EGL display.
  void ConcealAndCleanupGpu() {
    shell()->web_contents()->WasHidden();
    ASSERT_TRUE(FlushRendererProcessStateWithTimeout(base::Seconds(5)));

#if BUILDFLAG(IS_ANDROID)
    if (auto* window = shell()->web_contents()->GetTopLevelNativeWindow()) {
      if (auto* comp =
              static_cast<content::CompositorImpl*>(window->GetCompositor())) {
        comp->SetVisibleForTesting(false);
      }
    }
#else
    content::Shell::GetPlatform()->ConcealShell(shell());
#endif

    viz::FrameEvictionManager::GetInstance()->PurgeAllUnlockedFrames();

    base::RunLoop cleanup_loop;
    content::CleanupGpuProcessOnUI(cleanup_loop.QuitClosure());
    cleanup_loop.Run();
  }

  // Simulates Cobalt's foreground reveal sequence: restores the EGL display and
  // offscreen surface, maps the window, and marks the page visible again.
  void RevealAndRestoreGpu() {
    content::RestoreGpuProcessOnUI();
    FlushGpuMainThread();

#if BUILDFLAG(IS_ANDROID)
    if (auto* window = shell()->web_contents()->GetTopLevelNativeWindow()) {
      if (auto* comp =
              static_cast<content::CompositorImpl*>(window->GetCompositor())) {
        comp->SetVisibleForTesting(true);
      }
    }
    shell()->web_contents()->WasShown();
#else
    content::Shell::GetPlatform()->RevealShell(shell());
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
    content::Shell::GetPlatform()->MapWindowShell(shell());
#endif
    content::Shell::GetPlatform()->OnPageVisibilityVisible(shell());
#endif

    ASSERT_TRUE(FlushRendererProcessStateWithTimeout(base::Seconds(5)));
  }
};

// User journey: A user opens a WebGL page that handles `webglcontextlost` and
// `webglcontextrestored`, presses Home to conceal Cobalt (triggering full GPU
// and EGL teardown), waits while concealed past Blink's 1-second WebGL restore
// timer, freezes and unfreezes, returns to the app, and then repeats Home ->
// return a second time within the 2-minute 3D-API domain-blocking window. The
// page must receive `webglcontextrestored` and render fresh frames after each
// reveal without stalling the renderer while concealed or blocking the domain.
IN_PROC_BROWSER_TEST_F(WebGLContextRestoredBrowserTest,
                       RestoresWebGLContextAcrossRepeatedConcealAndReveal) {
  const GURL url = embedded_test_server()->GetURL(kWebGLTestPath);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  const std::string init_result =
      content::EvalJs(shell()->web_contents(), "window.__initResult")
          .ExtractString();
  if (init_result == "NO_WEBGL") {
    const gpu::GPUInfo gpu_info =
        content::GpuDataManagerImpl::GetInstance()->GetGPUInfo();
    if (gpu_info.gl_renderer.find("ANGLE") == std::string::npos) {
      GTEST_SKIP() << "WebGL unavailable on non-ANGLE GL renderer: "
                   << gpu_info.gl_renderer;
    }
  }
  ASSERT_EQ(init_result, "0,255,0,255");

  // Cycle 1: Conceal -> full GPU teardown -> dwell >1s while hidden -> Freeze
  // -> Unfreeze -> Reveal.
  ConcealAndCleanupGpu();

#if !BUILDFLAG(IS_APPLE)
  if (gl::GLDisplayEGL* display = gl::GetDefaultDisplayEGL()) {
    EXPECT_FALSE(display->IsInitialized());
  }
#endif

  // Confirm `webglcontextlost` fired while concealed (arming Blink's 1-second
  // `restore_timer_`) before dwelling past the timer interval.
  EXPECT_EQ(
      1, content::EvalJs(shell()->web_contents(), "window.waitForLost(1, 5000)")
             .ExtractInt());

  // Dwell past Blink's 1-second `kDurationBetweenRestoreAttempts` timer to
  // verify restore attempts are deferred while concealed instead of stalling
  // the renderer thread on EstablishGpuChannelSync().
  {
    base::RunLoop dwell_loop;
    base::OneShotTimer dwell_timer;
    dwell_timer.Start(FROM_HERE, base::Milliseconds(1300),
                      dwell_loop.QuitClosure());
    dwell_loop.Run();
  }

  // Renderer must remain responsive while concealed, restoration must stay
  // deferred until reveal, and the domain must not be blocked from 3D APIs.
  EXPECT_TRUE(FlushRendererProcessStateWithTimeout(base::Seconds(2)));
  EXPECT_EQ(0,
            content::EvalJs(shell()->web_contents(), "window.__restoredCount")
                .ExtractInt());
  EXPECT_FALSE(content::GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
      url, content::THREE_D_API_TYPE_WEBGL));

  shell()->web_contents()->SetPageFrozen(true);
  ASSERT_TRUE(FlushRendererProcessStateWithTimeout(base::Seconds(5)));
  shell()->web_contents()->SetPageFrozen(false);
  EXPECT_EQ(1, content::EvalJs(shell()->web_contents(), "window.__freezeCount")
                   .ExtractInt());

  RevealAndRestoreGpu();

  EXPECT_EQ(1, content::EvalJs(shell()->web_contents(),
                               "window.waitForRestored(1, 5000)")
                   .ExtractInt());
  EXPECT_EQ(false, content::EvalJs(shell()->web_contents(),
                                   "window.__gl.isContextLost()")
                       .ExtractBool());
  EXPECT_EQ("0,0,255,255",
            content::EvalJs(shell()->web_contents(), "window.__lastPixel")
                .ExtractString());

  // Cycle 2: Conceal -> full GPU teardown -> Reveal a second time immediately
  // within the 2-minute 3D-API domain-blocking window.
  ConcealAndCleanupGpu();

  RevealAndRestoreGpu();

  EXPECT_EQ(
      2, content::EvalJs(shell()->web_contents(), "window.waitForLost(2, 5000)")
             .ExtractInt());
  EXPECT_EQ(2, content::EvalJs(shell()->web_contents(),
                               "window.waitForRestored(2, 5000)")
                   .ExtractInt());
  EXPECT_EQ(false, content::EvalJs(shell()->web_contents(),
                                   "window.__gl.isContextLost()")
                       .ExtractBool());
  EXPECT_EQ("255,0,0,255",
            content::EvalJs(shell()->web_contents(), "window.__lastPixel")
                .ExtractString());
  EXPECT_FALSE(content::GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
      url, content::THREE_D_API_TYPE_WEBGL));
}

}  // namespace cobalt
