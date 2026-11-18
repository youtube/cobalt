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

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
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
#include "content/common/features.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/memory_stats.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "ui/android/window_android.h"
#endif

namespace cobalt {

class RenderThreadHiddenGpuChannelBrowserTestBase
    : public content::ContentBrowserTest {
 public:
  RenderThreadHiddenGpuChannelBrowserTestBase() = default;
  ~RenderThreadHiddenGpuChannelBrowserTestBase() override = default;

  // Forces single-process mode to match Cobalt's production architecture and
  // allow posting tasks directly onto `RenderThreadImpl::current()` via
  // `GetInProcessRendererThreadTaskRunnerForTesting()`.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    if (!command_line->HasSwitch(switches::kSingleProcess)) {
      command_line->AppendSwitch(switches::kSingleProcess);
    }
  }

 protected:
  // Performs a synchronous Mojo round-trip to the renderer so prior async
  // lifecycle IPCs (`WasHidden()` / `WasShown()`) are guaranteed to have
  // updated `RenderThreadImpl::RendererIsHidden()` before the next step runs.
  void FlushRendererProcessState() {
    base::RunLoop loop;
    auto* rph = static_cast<content::RenderProcessHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
    rph->GetRendererInterface()->PurgeResourceCache(loop.QuitClosure());
    loop.Run();
  }

  // Performs a synchronous Mojo round-trip to `GpuServiceImpl` so prior async
  // GPU lifecycle calls (`RestoreGpuProcessOnUI()`) have completed on the GPU
  // main thread before the renderer attempts sync channel establishment.
  void FlushGpuMainThread() {
    base::RunLoop loop;
    auto* gpu_host = content::GpuProcessHost::Get();
    ASSERT_NE(gpu_host, nullptr);
    gpu_host->gpu_service()->GetVideoMemoryUsageStats(
        base::BindLambdaForTesting(
            [&](const gpu::VideoMemoryUsageStats&) { loop.Quit(); }));
    loop.Run();
  }

  // Simulates Cobalt's background suspend sequence: marks the renderer hidden,
  // hides the UI compositor and purges unlocked Viz frames to release active
  // surfaces, and runs `CleanupGpuProcessOnUI()` so `GpuServiceImpl` tears
  // down `GLDisplayEGL` and defers incoming `EstablishGpuChannel` requests.
  void ConcealAndCleanupGpu() {
    shell()->web_contents()->WasHidden();
    FlushRendererProcessState();

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

  // Simulates Cobalt's foreground resume sequence: restores `GpuServiceImpl`
  // (`GLDisplayEGL` re-init and draining deferred channel requests), makes the
  // UI compositor/window visible again, and marks the renderer shown.
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
#else
    content::Shell::GetPlatform()->RevealShell(shell());
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
    content::Shell::GetPlatform()->MapWindowShell(shell());
#endif
#endif

    shell()->web_contents()->WasShown();
    FlushRendererProcessState();
  }
};

class RenderThreadHiddenGpuChannelBrowserTest
    : public RenderThreadHiddenGpuChannelBrowserTestBase {
 public:
  RenderThreadHiddenGpuChannelBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kCobaltSkipSyncGpuChannelWhenHidden);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RenderThreadHiddenGpuChannelBrowserTest,
                       SkipsSyncGpuChannelWhenHiddenAndRecoversOnShow) {
  ASSERT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner = content::
      RenderProcessHostImpl::GetInProcessRendererThreadTaskRunnerForTesting();
  ASSERT_NE(renderer_task_runner, nullptr);

  // 1. Visible baseline: sync GPU channel establishment succeeds.
  scoped_refptr<gpu::GpuChannelHost> initial_channel;
  base::RunLoop initial_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ASSERT_NE(content::RenderThreadImpl::current(), nullptr);
        initial_channel =
            content::RenderThreadImpl::current()->EstablishGpuChannelSync();
      }),
      initial_loop.QuitClosure());
  initial_loop.Run();
  ASSERT_NE(initial_channel, nullptr);
  EXPECT_FALSE(initial_channel->IsLost());

  // 2. Hide renderer and perform background GPU cleanup.
  ConcealAndCleanupGpu();

  // Drop renderer's cached client channel.
  base::RunLoop drop_channel_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([]() {
        if (auto* current = content::RenderThreadImpl::current()) {
          if (auto* channel = current->GetGpuChannel()) {
            channel->DestroyChannel();
          }
        }
      }),
      drop_channel_loop.QuitClosure());
  drop_channel_loop.Run();

  // Watchdog timer on Browser UI thread:
  // If the bug exists (RenderThreadImpl blocks on event.Wait()),
  // the watchdog unblocks the deadlocked thread after 2s by foregrounding
  // the GPU service, allowing the test to fail fast rather than timing out.
  base::OneShotTimer watchdog;
  bool blocked_until_foregrounded = false;
  watchdog.Start(FROM_HERE, base::Seconds(2), base::BindLambdaForTesting([&]() {
                   blocked_until_foregrounded = true;
                   content::RestoreGpuProcessOnUI();
                 }));

  scoped_refptr<gpu::GpuChannelHost> hidden_channel;
  scoped_refptr<viz::ContextProviderCommandBuffer> hidden_context_provider;
  base::RunLoop hidden_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* current = content::RenderThreadImpl::current();
        ASSERT_NE(current, nullptr);
        hidden_channel = current->EstablishGpuChannelSync();
        hidden_context_provider = current->SharedMainThreadContextProvider();
      }),
      base::BindLambdaForTesting([&]() {
        watchdog.Stop();
        hidden_loop.Quit();
      }));
  hidden_loop.Run();

  EXPECT_FALSE(blocked_until_foregrounded);
  EXPECT_EQ(hidden_channel, nullptr);
  EXPECT_EQ(hidden_context_provider, nullptr);

  // 3. Foreground recovery: channel establishment succeeds again.
  RevealAndRestoreGpu();

  scoped_refptr<gpu::GpuChannelHost> resumed_channel;
  scoped_refptr<viz::ContextProviderCommandBuffer> resumed_context_provider;
  base::RunLoop resume_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* current = content::RenderThreadImpl::current();
        ASSERT_NE(current, nullptr);
        resumed_channel = current->EstablishGpuChannelSync();
        resumed_context_provider = current->SharedMainThreadContextProvider();
      }),
      resume_loop.QuitClosure());
  resume_loop.Run();

  EXPECT_NE(resumed_channel, nullptr);
  if (resumed_channel) {
    EXPECT_FALSE(resumed_channel->IsLost());
  }
  EXPECT_NE(resumed_context_provider, nullptr);
}

using RenderThreadHiddenGpuChannelDefaultDisabledBrowserTest =
    RenderThreadHiddenGpuChannelBrowserTestBase;

IN_PROC_BROWSER_TEST_F(RenderThreadHiddenGpuChannelDefaultDisabledBrowserTest,
                       DisabledByDefaultRestoresSyncChannelWhileHidden) {
  ASSERT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner = content::
      RenderProcessHostImpl::GetInProcessRendererThreadTaskRunnerForTesting();
  ASSERT_NE(renderer_task_runner, nullptr);

  // Mark hidden.
  shell()->web_contents()->WasHidden();
  FlushRendererProcessState();

  // Drop cached channel. GLDisplayEGL remains initialized, so the GPU service
  // can service channel establishment synchronously without stalling.
  base::RunLoop drop_channel_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([]() {
        if (auto* current = content::RenderThreadImpl::current()) {
          if (auto* channel = current->GetGpuChannel()) {
            channel->DestroyChannel();
          }
        }
      }),
      drop_channel_loop.QuitClosure());
  drop_channel_loop.Run();

  scoped_refptr<gpu::GpuChannelHost> hidden_channel;
  base::RunLoop hidden_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* current = content::RenderThreadImpl::current();
        ASSERT_NE(current, nullptr);
        hidden_channel = current->EstablishGpuChannelSync();
      }),
      hidden_loop.QuitClosure());
  hidden_loop.Run();

  EXPECT_NE(hidden_channel, nullptr);
  if (hidden_channel) {
    EXPECT_FALSE(hidden_channel->IsLost());
  }
}

}  // namespace cobalt
