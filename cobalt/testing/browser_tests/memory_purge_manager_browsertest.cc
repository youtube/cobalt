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

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "gpu/ipc/common/memory_stats.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_cache/web_cache_resource_type_stats.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "ui/android/window_android.h"
#endif

namespace cobalt {
namespace {

constexpr base::TimeDelta kPurgeTimeout = base::Seconds(5);

// Counts `base::MemoryPressureListener` notifications on the Browser UI thread
// and allows tests to wait deterministically without polling sleeps.
class MemoryPressureCounter {
 public:
  MemoryPressureCounter()
      : listener_(FROM_HERE,
                  base::BindRepeating(&MemoryPressureCounter::OnMemoryPressure,
                                      base::Unretained(this))) {}
  ~MemoryPressureCounter() = default;

  MemoryPressureCounter(const MemoryPressureCounter&) = delete;
  MemoryPressureCounter& operator=(const MemoryPressureCounter&) = delete;

  int critical_count() const { return critical_count_; }
  int moderate_count() const { return moderate_count_; }

  bool WaitForCriticalCount(int target_count,
                            base::TimeDelta timeout = kPurgeTimeout) {
    if (critical_count_ >= target_count) {
      return true;
    }
    target_critical_count_ = target_count;
    base::RunLoop run_loop;
    base::OneShotTimer timeout_timer;
    bool timed_out = false;
    quit_closure_ = run_loop.QuitClosure();
    timeout_timer.Start(FROM_HERE, timeout, base::BindLambdaForTesting([&]() {
                          timed_out = true;
                          quit_closure_.Reset();
                          run_loop.Quit();
                        }));
    run_loop.Run();
    timeout_timer.Stop();
    target_critical_count_ = 0;
    quit_closure_.Reset();
    return !timed_out && critical_count_ >= target_count;
  }

  void RunForDuration(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, duration, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
      ++critical_count_;
      if (target_critical_count_ > 0 &&
          critical_count_ >= target_critical_count_ && quit_closure_) {
        std::move(quit_closure_).Run();
      }
    } else if (level ==
               base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
      ++moderate_count_;
    }
  }

  int critical_count_ = 0;
  int moderate_count_ = 0;
  int target_critical_count_ = 0;
  base::OnceClosure quit_closure_;
  base::MemoryPressureListener listener_;
};

std::unique_ptr<net::test_server::HttpResponse> HandlePurgeTestRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (request.relative_url == "/purge_test.html") {
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content(
        "<!DOCTYPE html><html><head><title>MemoryPurgeTest</title>"
        "<script src=\"/cached_script.js\"></script></head>"
        "<body><div id=\"content\">MemoryPurgeManager "
        "Test</div></body></html>");
    return response;
  }
  if (request.relative_url == "/cached_script.js") {
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/javascript");
    response->AddCustomHeader("Cache-Control", "max-age=3600");
    // Pad the cached resource so stats.scripts.size is comfortably non-zero.
    std::string payload = "window.__cobaltCachedScriptLoaded = true;\n// ";
    payload.append(2048, 'a');
    payload.push_back('\n');
    response->set_content(payload);
    return response;
  }
  return nullptr;
}

class MemoryPurgeManagerBrowserTestBase : public content::ContentBrowserTest {
 public:
  MemoryPurgeManagerBrowserTestBase() = default;
  ~MemoryPurgeManagerBrowserTestBase() override = default;

  // Forces single-process mode to match Cobalt's production architecture and
  // allow querying in-process renderer caches on
  // `GetInProcessRendererThreadTaskRunnerForTesting()`.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    if (!command_line->HasSwitch(switches::kSingleProcess)) {
      command_line->AppendSwitch(switches::kSingleProcess);
    }
    // Cobalt production (`CobaltContentBrowserClient`) never loads
    // `testing/variations/fieldtrial_testing_config.json`, whereas
    // `ShellContentBrowserTestClient` does—and Chromium's testing config maps
    // `MemoryPurgeInBackground` to `"DisableAll"`. Disable the field trial
    // testing config so tests run with Cobalt's real production FeatureList.
    if (!command_line->HasSwitch("disable-field-trial-config")) {
      command_line->AppendSwitch("disable-field-trial-config");
    }
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandlePurgeTestRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Executes `closure` on the in-process renderer thread without mutating any
  // Blink or Skia caches (unlike `PurgeResourceCache`, which calls
  // `blink::WebCache::Clear()`).
  void RunOnRendererThread(base::OnceClosure closure) {
    scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner = content::
        RenderProcessHostImpl::GetInProcessRendererThreadTaskRunnerForTesting();
    ASSERT_NE(renderer_task_runner, nullptr);
    base::RunLoop run_loop;
    renderer_task_runner->PostTaskAndReply(FROM_HERE, std::move(closure),
                                           run_loop.QuitClosure());
    run_loop.Run();
  }

  void FlushRendererTasks() { RunOnRendererThread(base::DoNothing()); }

  // Waits for `document.visibilityState` to reflect `expected_visible` and
  // drains the renderer task runner. Because `SetProcessState` and
  // `PageBroadcast::SetPageVisibilityState` share the channel-associated IPC
  // pipe, this guarantees `MainThreadSchedulerImpl::SetRendererHidden()` has
  // executed before returning.
  bool WaitForPageVisibility(bool expected_visible) {
    const std::string expected_state = expected_visible ? "visible" : "hidden";
    const base::TimeTicks deadline = base::TimeTicks::Now() + kPurgeTimeout;
    while (base::TimeTicks::Now() < deadline) {
      content::EvalJsResult result =
          content::EvalJs(shell()->web_contents(), "document.visibilityState",
                          content::EXECUTE_SCRIPT_NO_USER_GESTURE);
      if (result.error.empty() && result.ExtractString() == expected_state) {
        FlushRendererTasks();
        return true;
      }
      base::RunLoop loop;
      base::OneShotTimer retry_timer;
      retry_timer.Start(FROM_HERE, base::Milliseconds(10), loop.QuitClosure());
      loop.Run();
    }
    return false;
  }

  // Forces CPU glyph rasterization into Skia's font strike cache via an
  // offscreen software 2D canvas and returns `SkGraphics::GetFontCacheUsed()`.
  size_t SeedAndGetRendererFontCacheBytes(
      std::string_view text = "CobaltPurgeTest 0123456789") {
    const std::string script = base::StringPrintf(
        "(() => {"
        "  const c = document.createElement('canvas');"
        "  c.width = 256; c.height = 64;"
        "  const ctx = c.getContext('2d', { willReadFrequently: true });"
        "  ctx.font = '24px sans-serif';"
        "  ctx.fillText('%.*s', 8, 32);"
        "  return ctx.getImageData(0, 0, 1, 1).data.length;"
        "})()",
        static_cast<int>(text.size()), text.data());
    EXPECT_EQ(4, content::EvalJs(shell()->web_contents(), script,
                                 content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                     .ExtractInt());
    return GetRendererFontCacheBytes();
  }

  size_t GetRendererFontCacheBytes() {
    size_t used_bytes = 0;
    RunOnRendererThread(base::BindLambdaForTesting(
        [&]() { used_bytes = SkGraphics::GetFontCacheUsed(); }));
    return used_bytes;
  }

  // Rasterizes text directly into a CPU SkBitmap on the in-process renderer
  // thread so Skia's font strike cache is populated without touching Blink
  // `<canvas>` / `SharedGpuContext` while the GPU service is torn down.
  size_t SeedRendererFontCacheOnCpu(
      std::string_view text = "CobaltPurgeCpuTest 0123456789") {
    size_t used_bytes = 0;
    RunOnRendererThread(base::BindLambdaForTesting([&]() {
      SkBitmap bitmap;
      bitmap.allocN32Pixels(256, 64);
      SkCanvas canvas(bitmap);
      SkFont font = skia::DefaultFont();
      font.setSize(24);
      SkPaint paint;
      canvas.drawSimpleText(text.data(), text.size(), SkTextEncoding::kUTF8, 8,
                            32, font, paint);
      used_bytes = SkGraphics::GetFontCacheUsed();
    }));
    return used_bytes;
  }

  blink::WebCacheResourceTypeStat GetBlinkMemoryCacheScriptStats() {
    blink::WebCacheResourceTypeStats stats = {};
    RunOnRendererThread(base::BindLambdaForTesting(
        [&]() { blink::WebCache::GetResourceTypeStats(&stats); }));
    return stats.scripts;
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
};

class MemoryPurgeManagerEnabledBrowserTest
    : public MemoryPurgeManagerBrowserTestBase {
 public:
  MemoryPurgeManagerEnabledBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kCobaltEnableBlinkMemoryPurgeManager);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// S2 (85% non-Shield OS 9-13): Conceal alone triggers a 1s delayed CRITICAL
// purge that clears renderer caches without suppressing listeners.
IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerEnabledBrowserTest,
                       ConcealOnlyPurgesRendererCachesAfterOneSecond) {
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));
  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);
  ASSERT_GE(GetBlinkMemoryCacheScriptStats().count, 1u);

  MemoryPressureCounter counter;
  const base::TimeTicks conceal_start = base::TimeTicks::Now();
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  EXPECT_EQ(counter.critical_count(), 0);

  ASSERT_TRUE(counter.WaitForCriticalCount(1));
  const base::TimeDelta elapsed = base::TimeTicks::Now() - conceal_start;
  EXPECT_GE(elapsed, base::Milliseconds(700));

  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);
  // EvictResources() in Blink MemoryCache stays off unless its own study flag
  // is enabled.
  EXPECT_GE(GetBlinkMemoryCacheScriptStats().count, 1u);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());
}

// S1 (Control Baseline): With kCobaltEnableBlinkMemoryPurgeManager disabled,
// WasHidden() must not run the 1s delayed purge.
class MemoryPurgeManagerDisabledBrowserTest
    : public MemoryPurgeManagerBrowserTestBase {
 public:
  MemoryPurgeManagerDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kCobaltEnableBlinkMemoryPurgeManager);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerDisabledBrowserTest,
                       FeatureDisabledDoesNotTriggerShortDelayConcealPurge) {
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));
  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);

  MemoryPressureCounter counter;
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));

  // Exceed kFreezePurgeDelay (1s); Android compiles purging off and desktop
  // uses a 1-4 minute delay.
  counter.RunForDuration(base::Milliseconds(1400));
  EXPECT_EQ(counter.critical_count(), 0);
  EXPECT_GT(GetRendererFontCacheBytes(), 0u);
}

// S3 & S4 (Warm-Resume Guardrail + Idempotency): Quick reveal cancels the 1s
// pending timer cleanly, and redundant WasHidden() calls emit one purge.
IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerEnabledBrowserTest,
                       TransientConcealAndQuickRevealCancelsPurgeAndDedups) {
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));
  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);

  MemoryPressureCounter counter;
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  counter.RunForDuration(base::Milliseconds(50));
  EXPECT_EQ(counter.critical_count(), 0);

  shell()->web_contents()->WasShown();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/true));

  // Wait past the original 1s timer deadline; no purge should fire and warm
  // caches remain populated.
  counter.RunForDuration(base::Milliseconds(1150));
  EXPECT_EQ(counter.critical_count(), 0);
  EXPECT_GT(GetRendererFontCacheBytes(), 0u);

  // Redundant back-to-back WasHidden() calls must trigger a single purge.
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  shell()->web_contents()->WasHidden();
  FlushRendererTasks();
  ASSERT_TRUE(counter.WaitForCriticalCount(1));
  counter.RunForDuration(base::Milliseconds(350));
  EXPECT_EQ(counter.critical_count(), 1);
}

// S5 & S8 (Shield Freeze + Single-Process Unsuppress Safety + Multi-Cycle):
// Freezing suppresses notifications after purging, resuming immediately clears
// process-wide suppression so Browser UI listeners receive pressure again, and
// a second Conceal -> Freeze cycle re-triggers cleanly.
IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerEnabledBrowserTest,
                       ConcealThenFreezeUnsuppressesAndResetsAcrossCycles) {
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));

  MemoryPressureCounter counter;

  // Cycle 1: Conceal -> purge -> Freeze -> purge + suppress.
  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  ASSERT_TRUE(counter.WaitForCriticalCount(1));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  ASSERT_GT(SeedAndGetRendererFontCacheBytes("Cycle1Freeze 123456"), 0u);
  shell()->web_contents()->SetPageFrozen(true);
  ASSERT_TRUE(counter.WaitForCriticalCount(2));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  // Unfreeze + Reveal must unsuppress process-wide listeners.
  shell()->web_contents()->SetPageFrozen(false);
  shell()->web_contents()->WasShown();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/true));
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(counter.moderate_count(), 1);

  // Cycle 2: Verify did_purge_with_page_frozen_since_backgrounded_ reset.
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  ASSERT_TRUE(counter.WaitForCriticalCount(3));
  shell()->web_contents()->SetPageFrozen(true);
  ASSERT_TRUE(counter.WaitForCriticalCount(4));
  FlushRendererTasks();
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  shell()->web_contents()->SetPageFrozen(false);
  shell()->web_contents()->WasShown();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/true));
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());
}

// S6 & S7 (Real RenderProcessHostImpl Priority Holds): Both pre-existing and
// mid-conceal priority boosts via OnMediaStreamAdded() keep RPHI priority at
// kUserBlocking, yet hidden purges and frozen purges still fire.
IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerEnabledBrowserTest,
                       BrowserPriorityHoldBeforeAndDuringConcealStillPurges) {
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));

  auto* rph = static_cast<content::RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  ASSERT_NE(rph, nullptr);

  bool media_hold_active = false;
  base::ScopedClosureRunner cleanup_hold(base::BindLambdaForTesting([&]() {
    if (media_hold_active) {
      rph->OnMediaStreamRemoved();
      media_hold_active = false;
    }
  }));

  MemoryPressureCounter counter;

  // Part A (S6): Pre-existing priority hold before WasHidden().
  rph->OnMediaStreamAdded();
  media_hold_active = true;
  EXPECT_EQ(rph->GetPriority(), base::Process::Priority::kUserBlocking);

  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  EXPECT_EQ(rph->GetPriority(), base::Process::Priority::kUserBlocking);
  ASSERT_TRUE(counter.WaitForCriticalCount(1));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);

  // Also verify Freeze while media stream hold keeps
  // renderer_backgrounded_=false (or priority held) still purges via CanPurge()
  // AreAllPagesFrozen().
  shell()->web_contents()->SetPageFrozen(true);
  ASSERT_TRUE(counter.WaitForCriticalCount(2));
  FlushRendererTasks();
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  shell()->web_contents()->SetPageFrozen(false);
  shell()->web_contents()->WasShown();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/true));
  rph->OnMediaStreamRemoved();
  media_hold_active = false;

  // Part B (S7): Priority boost arriving mid-conceal must not cancel purge.
  ASSERT_GT(SeedAndGetRendererFontCacheBytes("MidBoost 987654"), 0u);
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  counter.RunForDuration(base::Milliseconds(50));
  EXPECT_EQ(counter.critical_count(), 2);

  rph->OnMediaStreamAdded();
  media_hold_active = true;
  EXPECT_EQ(rph->GetPriority(), base::Process::Priority::kUserBlocking);
  FlushRendererTasks();

  ASSERT_TRUE(counter.WaitForCriticalCount(3));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);

  rph->OnMediaStreamRemoved();
  media_hold_active = false;
  shell()->web_contents()->WasShown();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/true));
}

// S9 (Cross-Study Synergy 52152263): When combined with
// kEvictMemoryCacheOnCriticalMemoryPressure, the 1s conceal purge evicts
// Blink's MemoryCache as well as Skia caches.
class MemoryPurgeManagerCrossStudyEvictCacheBrowserTest
    : public MemoryPurgeManagerBrowserTestBase {
 public:
  MemoryPurgeManagerCrossStudyEvictCacheBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kCobaltEnableBlinkMemoryPurgeManager,
         blink::features::kEvictMemoryCacheOnCriticalMemoryPressure},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerCrossStudyEvictCacheBrowserTest,
                       ConcealPurgeEvictsBlinkMemoryCacheWhenCrossEnabled) {
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));
  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);
  ASSERT_GE(GetBlinkMemoryCacheScriptStats().count, 1u);
  ASSERT_GT(GetBlinkMemoryCacheScriptStats().size, 0u);

  MemoryPressureCounter counter;
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  ASSERT_TRUE(counter.WaitForCriticalCount(1));
  FlushRendererTasks();

  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);
  const blink::WebCacheResourceTypeStat script_stats =
      GetBlinkMemoryCacheScriptStats();
  EXPECT_EQ(script_stats.count, 0u);
  EXPECT_EQ(script_stats.size, 0u);
}

// T7a: Exercises the `hidden && !was_process_hidden && renderer_backgrounded`
// branch in MainThreadSchedulerImpl::SetRendererHidden(), plus clean recovery
// on WasShown().
IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerEnabledBrowserTest,
                       InitialBackgroundedRendererStateStillPurgesOnConceal) {
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));
  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);

  MemoryPressureCounter counter;
  // Simulate kLaunchingProcessIsBackgrounded having set
  // main_thread_only().renderer_backgrounded = true before
  // renderer_hidden_metadata is populated.
  RunOnRendererThread(base::BindOnce([]() {
    blink::scheduler::WebThreadScheduler::MainThreadScheduler()
        .SetRendererBackgrounded(true);
  }));

  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  ASSERT_TRUE(counter.WaitForCriticalCount(1));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);

  // Showing must reset both hidden and backgrounded state so a second conceal
  // cycle purges normally.
  shell()->web_contents()->WasShown();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/true));
  ASSERT_GT(SeedAndGetRendererFontCacheBytes("ColdStartRecover 42"), 0u);

  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  ASSERT_TRUE(counter.WaitForCriticalCount(2));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);
}

// T7b: True cold-start hidden launch where the initial Shell transitions to
// hidden right before PreRunTestOnMainThread() pumps startup IPCs.
class MemoryPurgeManagerColdStartHiddenBrowserTest
    : public MemoryPurgeManagerEnabledBrowserTest {
 public:
  MemoryPurgeManagerColdStartHiddenBrowserTest() = default;
  ~MemoryPurgeManagerColdStartHiddenBrowserTest() override = default;

  void PreRunTestOnMainThread() override {
    ASSERT_FALSE(content::Shell::windows().empty());
    cold_start_counter_ = std::make_unique<MemoryPressureCounter>();
    content::Shell::windows()[0]->web_contents()->WasHidden();
    cold_start_was_hidden_ = true;
    MemoryPurgeManagerEnabledBrowserTest::PreRunTestOnMainThread();
  }

  void TearDownOnMainThread() override {
    cold_start_counter_.reset();
    MemoryPurgeManagerEnabledBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<MemoryPressureCounter> cold_start_counter_;
  bool cold_start_was_hidden_ = false;
};

IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerColdStartHiddenBrowserTest,
                       ColdStartHiddenShellPurgesAndRecoversOnShow) {
  ASSERT_TRUE(cold_start_was_hidden_);
  ASSERT_NE(cold_start_counter_, nullptr);
  EXPECT_EQ(shell()->web_contents()->GetVisibility(),
            content::Visibility::HIDDEN);

  // Navigate while still hidden (matching Cobalt `--preload` / Android TV
  // background launch where LoadURL runs on an already-hidden WebContents).
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));
  EXPECT_EQ(shell()->web_contents()->GetVisibility(),
            content::Visibility::HIDDEN);

  ASSERT_TRUE(cold_start_counter_->WaitForCriticalCount(1));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);

  // Revealing the cold-started hidden shell recovers foreground visibility,
  // unsuppresses notifications, and allows normal caching plus subsequent
  // conceal purges.
  shell()->web_contents()->WasShown();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/true));
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());
  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);

  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
  ASSERT_TRUE(cold_start_counter_->WaitForCriticalCount(2));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);
}

// T8 (5% UseStarboardLifeCycleStudy / DoConceal + GPU Teardown Interaction):
// Exercises Starboard conceal + CleanupGpuProcessOnUI() + immediate DoConceal
// CRITICAL notification, verifies post-visibilitychange allocations are
// reclaimed by MemoryPurgeManager's 1s timer without deadlocking on torn-down
// GPU, and verifies clean Freeze -> Unfreeze -> Reveal recovery.
IN_PROC_BROWSER_TEST_F(MemoryPurgeManagerEnabledBrowserTest,
                       StarboardConcealGpuTeardownAndDelayedPurgeInteraction) {
  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL("/purge_test.html")));
  ASSERT_GT(SeedAndGetRendererFontCacheBytes(), 0u);

  MemoryPressureCounter counter;

  // 1. Conceal + GPU teardown matching ShellPlatformDelegate / AppEventRunner.
  shell()->web_contents()->WasHidden();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/false));
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

  // 2. Immediate DoConceal CRITICAL pressure notification.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        base::MemoryPressureListener::NotifyMemoryPressure(
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
      }));
  ASSERT_TRUE(counter.WaitForCriticalCount(1));
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);

  // 3. Simulate a post-conceal CPU glyph allocation while GPU stays torn down
  // before MemoryPurgeManager's 1s delayed purge fires, guarded by a watchdog
  // in case any step stalls on the torn-down GPU service.
  base::OneShotTimer gpu_watchdog;
  bool watchdog_fired = false;
  gpu_watchdog.Start(FROM_HERE, base::Seconds(4),
                     base::BindLambdaForTesting([&]() {
                       watchdog_fired = true;
                       content::RestoreGpuProcessOnUI();
                     }));
  const int count_after_do_conceal = counter.critical_count();
  if (count_after_do_conceal == 1) {
    ASSERT_GT(SeedRendererFontCacheOnCpu("PostConcealAlloc 777"), 0u);
  }

  // 4. MemoryPurgeManager's 1s timer reclaims post-conceal caches without
  // deadlocking against CleanupGpuProcessOnUI().
  ASSERT_TRUE(counter.WaitForCriticalCount(2));
  gpu_watchdog.Stop();
  EXPECT_FALSE(watchdog_fired);
  FlushRendererTasks();
  EXPECT_EQ(GetRendererFontCacheBytes(), 0u);

  // 5. Freeze -> Freeze purge + process-wide suppression.
  const int expected_freeze_count = counter.critical_count() + 1;
  shell()->web_contents()->SetPageFrozen(true);
  ASSERT_TRUE(counter.WaitForCriticalCount(expected_freeze_count));
  FlushRendererTasks();
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  // 6. Unfreeze -> Reveal -> Restore GPU -> Verify full foreground recovery.
  content::RestoreGpuProcessOnUI();
  FlushGpuMainThread();
  shell()->web_contents()->SetPageFrozen(false);
#if BUILDFLAG(IS_ANDROID)
  if (auto* window = shell()->web_contents()->GetTopLevelNativeWindow()) {
    if (auto* comp =
            static_cast<content::CompositorImpl*>(window->GetCompositor())) {
      comp->SetVisibleForTesting(true);
    }
  }
#else
  content::Shell::GetPlatform()->RevealShell(shell());
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
  content::Shell::GetPlatform()->MapWindowShell(shell());
#endif
#endif
  shell()->web_contents()->WasShown();
  ASSERT_TRUE(WaitForPageVisibility(/*expected_visible=*/true));
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());
  EXPECT_GT(SeedAndGetRendererFontCacheBytes("RevealRecovered 888"), 0u);
}

}  // namespace
}  // namespace cobalt
