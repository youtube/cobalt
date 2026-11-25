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

#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "gpu/ipc/common/memory_stats.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

namespace {

// A well-behaved WebGL page: it prevents the default action of
// webglcontextlost (which asks the browser to restore the context) and redraws
// on webglcontextrestored.
constexpr char kSetUpRestorableWebGLScript[] = R"(
(() => {
  const canvas = document.createElement('canvas');
  canvas.width = 64;
  canvas.height = 64;
  document.body.appendChild(canvas);
  window.webglLostCount = 0;
  window.webglRestoredCount = 0;
  canvas.addEventListener('webglcontextlost', (e) => {
    e.preventDefault();
    ++window.webglLostCount;
  });
  canvas.addEventListener('webglcontextrestored', () => {
    ++window.webglRestoredCount;
  });
  const gl = canvas.getContext('webgl', {preserveDrawingBuffer: true});
  if (!gl) {
    return 'getContext returned null';
  }
  window.webglDrawAndReadBack = (r, g, b) => {
    if (gl.isContextLost()) {
      return 'context lost';
    }
    gl.clearColor(r / 255, g / 255, b / 255, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    const pixel = new Uint8Array(4);
    gl.readPixels(32, 32, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
    if (gl.getError() !== gl.NO_ERROR) {
      return 'GL error';
    }
    const expected = [r, g, b, 255];
    for (let i = 0; i < 4; ++i) {
      if (Math.abs(pixel[i] - expected[i]) > 2) {
        return 'unexpected pixel ' + Array.from(pixel).join(',');
      }
    }
    return 'ok';
  };
  window.waitForRestore = (timeoutMs) => new Promise((resolve) => {
    const start = performance.now();
    const poll = () => {
      if (window.webglRestoredCount > 0) {
        resolve('restored');
      } else if (performance.now() - start > timeoutMs) {
        resolve('timeout');
      } else {
        setTimeout(poll, 100);
      }
    };
    poll();
  });
  return window.webglDrawAndReadBack(255, 64, 128);
})()
)";

}  // namespace

// Checks that a page which handles WebGL context loss gets its context back
// after the app is concealed and revealed. With
// CobaltKeepWebGLContextsOnBackground disabled (the default), background
// cleanup releases the WebGL context, and the page must be able to restore it.
class WebGLContextRestoreBrowserTest : public content::ContentBrowserTest {
 protected:
  void SetUp() override {
    // Browser tests disable GL drawing by default; the pixel checks need it.
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void FlushGpuMainThread() {
    base::RunLoop loop;
    auto* gpu_host = content::GpuProcessHost::Get();
    ASSERT_NE(gpu_host, nullptr);
    gpu_host->gpu_service()->GetVideoMemoryUsageStats(
        base::BindLambdaForTesting(
            [&](const gpu::VideoMemoryUsageStats&) { loop.Quit(); }));
    loop.Run();
  }

  void Conceal() {
    shell()->web_contents()->WasHidden();
    content::Shell::GetPlatform()->ConcealShell(shell());
    viz::FrameEvictionManager::GetInstance()->PurgeAllUnlockedFrames();
    base::RunLoop cleanup_loop;
    content::CleanupGpuProcessOnUI(cleanup_loop.QuitClosure());
    cleanup_loop.Run();
  }

  void Reveal() {
    content::RestoreGpuProcessOnUI();
    FlushGpuMainThread();
    content::Shell::GetPlatform()->RevealShell(shell());
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
    content::Shell::GetPlatform()->MapWindowShell(shell());
#endif
    shell()->web_contents()->WasShown();
  }

  bool WebGLBlocked(const GURL& url) {
    return content::GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
        url, content::THREE_D_API_TYPE_WEBGL);
  }
};

IN_PROC_BROWSER_TEST_F(WebGLContextRestoreBrowserTest, RestoredAfterReveal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));
  ASSERT_EQ("ok", content::EvalJs(shell()->web_contents(),
                                  kSetUpRestorableWebGLScript));

  Conceal();
  // The contexts were released on purpose, so the page must not be blocked
  // from 3D APIs; a block makes Blink silently skip the restore.
  EXPECT_FALSE(WebGLBlocked(url));
  Reveal();

  EXPECT_EQ("restored", content::EvalJs(shell()->web_contents(),
                                        "window.waitForRestore(5000)"));
  EXPECT_EQ(1,
            content::EvalJs(shell()->web_contents(), "window.webglLostCount"));
  EXPECT_EQ("ok", content::EvalJs(shell()->web_contents(),
                                  "window.webglDrawAndReadBack(32, 200, 96)"));
}

}  // namespace cobalt
