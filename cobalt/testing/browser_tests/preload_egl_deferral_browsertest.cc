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

#include "base/base64.h"
#include "base/command_line.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"
#include "components/version_info/version_info.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "gpu/ipc/common/memory_stats.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

namespace cobalt {

class PreloadEglDeferralBrowserTestBase : public content::ContentBrowserTest {
 public:
  PreloadEglDeferralBrowserTestBase() = default;
  ~PreloadEglDeferralBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    if (!command_line->HasSwitch(switches::kSingleProcess)) {
      command_line->AppendSwitch(switches::kSingleProcess);
    }
    command_line->AppendSwitch(
        content::ContentBrowserTestContentBrowserClient::kPreloadSwitch);
    command_line->RemoveSwitch(switches::kDisableGpuShaderDiskCache);
    command_line->AppendSwitch(switches::kGpuShaderDiskCacheGrOnly);
  }

 protected:
  void FlushRendererProcessState() {
    base::RunLoop loop;
    auto* rph = static_cast<content::RenderProcessHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
    rph->GetRendererInterface()->PurgeResourceCache(loop.QuitClosure());
    loop.Run();
  }

  void FlushGpuMainThread() {
    base::RunLoop loop;
    content::GpuProcessHost::CallOnUI(
        FROM_HERE, content::GPU_PROCESS_KIND_SANDBOXED,
        /*force_create=*/false,
        base::BindLambdaForTesting([&](content::GpuProcessHost* gpu_host) {
          ASSERT_NE(gpu_host, nullptr);
          gpu_host->gpu_service()->GetVideoMemoryUsageStats(
              base::BindLambdaForTesting(
                  [&](const gpu::VideoMemoryUsageStats&) { loop.Quit(); }));
        }));
    loop.Run();
  }

  void RevealAndRestoreGpu() {
    class GpuInfoWaiter : public content::GpuDataManagerObserver {
     public:
      explicit GpuInfoWaiter(base::OnceClosure quit_closure)
          : quit_closure_(std::move(quit_closure)) {
        content::GpuDataManagerImpl::GetInstance()->AddObserver(this);
      }
      ~GpuInfoWaiter() override {
        content::GpuDataManagerImpl::GetInstance()->RemoveObserver(this);
      }
      void OnGpuInfoUpdate() override {
        if (!content::GpuDataManagerImpl::GetInstance()
                 ->GetGPUInfo()
                 .gl_version.empty() &&
            quit_closure_) {
          std::move(quit_closure_).Run();
        }
      }

     private:
      base::OnceClosure quit_closure_;
    };

    const bool needs_cold_gpu_info = content::GpuDataManagerImpl::GetInstance()
                                         ->GetGPUInfo()
                                         .gl_version.empty();
    base::RunLoop gpu_info_loop;
    std::unique_ptr<GpuInfoWaiter> waiter;
    if (needs_cold_gpu_info) {
      waiter = std::make_unique<GpuInfoWaiter>(gpu_info_loop.QuitClosure());
    }

    if (content::Shell::GetPlatform()->IsVisible()) {
      content::RestoreGpuProcessOnUI();
      content::Shell::GetPlatform()->RevealShell(shell());
    } else {
      content::Shell::GetPlatform()->OnReveal();
    }
    content::Shell::GetPlatform()->MapWindowShell(shell());
    content::Shell::GetPlatform()->OnPageVisibilityVisible(shell());
    FlushGpuMainThread();
    if (needs_cold_gpu_info && content::GpuDataManagerImpl::GetInstance()
                                   ->GetGPUInfo()
                                   .gl_version.empty()) {
      gpu_info_loop.Run();
    }
    waiter.reset();
    FlushRendererProcessState();
    base::RunLoop().RunUntilIdle();
  }

  void ConcealAndCleanupGpu() {
    shell()->web_contents()->WasHidden();
    FlushRendererProcessState();
    content::Shell::GetPlatform()->ConcealShell(shell());
    viz::FrameEvictionManager::GetInstance()->PurgeAllUnlockedFrames();

    base::RunLoop cleanup_loop;
    content::CleanupGpuProcessOnUI(cleanup_loop.QuitClosure());
    cleanup_loop.Run();
  }

  static std::string BuildShaderPrefix(const gpu::GPUInfo& gpu_info) {
    const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
    return std::string(version_info::GetProductNameAndVersionForUserAgent()) +
           "-" + gpu_info.gl_vendor + "-" + gpu_info.gl_renderer + "-" +
           active_gpu.driver_version + "-" + active_gpu.driver_vendor + "-" +
           base::SysInfo::ProcessCPUArchitecture();
  }
};

class PreloadEglDeferralBrowserTest : public PreloadEglDeferralBrowserTestBase {
 public:
  PreloadEglDeferralBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kCobaltDeferEglInitUntilVisible);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PreloadEglDeferralDisabledBrowserTest
    : public PreloadEglDeferralBrowserTestBase {
 public:
  PreloadEglDeferralDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        features::kCobaltDeferEglInitUntilVisible);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Covers the user journey where the TV platform preloads Cobalt in the
// background (Concealed), optionally freezes and unfreezes it while still
// hidden, opens the app to the foreground (Reveal), and later backgrounds and
// foregrounds it again without ever holding an initialized EGLDisplay while
// non-visible.
IN_PROC_BROWSER_TEST_F(PreloadEglDeferralBrowserTest,
                       DefersEglInitDuringPreloadAndUnfreezeUntilReveal) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));
  FlushGpuMainThread();
  FlushRendererProcessState();

  // 1. Cold Preload state: application is concealed and GLDisplayEGL has not
  // been initialized.
  EXPECT_FALSE(content::Shell::GetPlatform()->IsVisible());
  gl::GLDisplayEGL* display = gl::GetDefaultDisplayEGL();
  ASSERT_NE(display, nullptr);
  EXPECT_FALSE(display->IsInitialized());
  EXPECT_TRUE(content::GpuDataManagerImpl::GetInstance()
                  ->GetGPUInfo()
                  .gl_version.empty());

  scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner = content::
      RenderProcessHostImpl::GetInProcessRendererThreadTaskRunnerForTesting();
  ASSERT_NE(renderer_task_runner, nullptr);

  // Hang-breaker only: if EstablishGpuChannelSync regresses and blocks while
  // preloaded, restoring the GPU unblocks the test so it fails
  // deterministically instead of timing out the suite.
  base::OneShotTimer watchdog;
  bool blocked_while_preloaded = false;
  watchdog.Start(FROM_HERE, base::Seconds(2), base::BindLambdaForTesting([&]() {
                   blocked_while_preloaded = true;
                   content::RestoreGpuProcessOnUI();
                 }));

  scoped_refptr<gpu::GpuChannelHost> preload_channel;
  base::RunLoop preload_sync_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* current = content::RenderThreadImpl::current();
        ASSERT_NE(current, nullptr);
        preload_channel = current->EstablishGpuChannelSync();
      }),
      base::BindLambdaForTesting([&]() {
        watchdog.Stop();
        preload_sync_loop.Quit();
      }));
  preload_sync_loop.Run();

  EXPECT_FALSE(blocked_while_preloaded);
  EXPECT_EQ(preload_channel, nullptr);
  EXPECT_FALSE(display->IsInitialized());

  // 2. Freeze -> Unfreeze cycle without Reveal: GLDisplayEGL stays
  // uninitialized while background script execution proceeds without hanging.
  content::Shell::GetPlatform()->OnFreeze();
  content::Shell::GetPlatform()->OnUnfreeze();
  FlushGpuMainThread();
  EXPECT_FALSE(content::Shell::GetPlatform()->IsVisible());
  EXPECT_FALSE(display->IsInitialized());
  EXPECT_EQ(2, content::EvalJs(shell()->web_contents(), "1 + 1"));
  EXPECT_EQ(false,
            content::EvalJs(
                shell()->web_contents(),
                "!!document.createElement('canvas').getContext('webgl')"));
  EXPECT_FALSE(display->IsInitialized());

  // 3. First Reveal: cold GL init runs once, populates GPUInfo, and allows
  // GPU channel and context establishment.
  RevealAndRestoreGpu();

  EXPECT_TRUE(content::Shell::GetPlatform()->IsVisible());
  EXPECT_TRUE(display->IsInitialized());
  const gpu::GPUInfo revealed_gpu_info =
      content::GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  EXPECT_FALSE(revealed_gpu_info.gl_version.empty());
  EXPECT_FALSE(revealed_gpu_info.gl_renderer.empty());
  histogram_tester.ExpectTotalCount("Cobalt.GPU.DeferEglInit.ColdInitDuration",
                                    1);

  scoped_refptr<gpu::GpuChannelHost> revealed_channel;
  scoped_refptr<viz::ContextProviderCommandBuffer> revealed_context_provider;
  gpu::ContextResult context_bind_result = gpu::ContextResult::kFatalFailure;
  base::RunLoop reveal_sync_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* current = content::RenderThreadImpl::current();
        ASSERT_NE(current, nullptr);
        revealed_channel = current->EstablishGpuChannelSync();
        revealed_context_provider = current->SharedMainThreadContextProvider();
        if (revealed_context_provider) {
          context_bind_result =
              revealed_context_provider->BindToCurrentSequence();
        }
      }),
      reveal_sync_loop.QuitClosure());
  reveal_sync_loop.Run();

  ASSERT_NE(revealed_channel, nullptr);
  EXPECT_FALSE(revealed_channel->IsLost());
  EXPECT_FALSE(revealed_channel->gpu_info().gl_renderer.empty());
  ASSERT_NE(revealed_context_provider, nullptr);
  EXPECT_EQ(context_bind_result, gpu::ContextResult::kSuccess);
  // Starboard ARM Evergreen binds native OS libEGL/libGLESv2 directly without
  // ANGLE (lacking GL_ANGLE_robust_client_memory required by passthrough
  // WebGL), so only assert WebGL creation when ANGLE and WebGL are both active.
  if (revealed_gpu_info.gl_renderer.find("ANGLE") != std::string::npos &&
      revealed_channel->gpu_feature_info()
              .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL] ==
          gpu::kGpuFeatureStatusEnabled) {
    EXPECT_EQ(true,
              content::EvalJs(
                  shell()->web_contents(),
                  "!!document.createElement('canvas').getContext('webgl')"));
  }

  // 4. Subsequent Conceal -> Reveal cycle tears down GLDisplayEGL and restores
  // via the fast path without re-running cold GPUInfo collection.
  ConcealAndCleanupGpu();
  EXPECT_FALSE(display->IsInitialized());

  RevealAndRestoreGpu();
  EXPECT_TRUE(display->IsInitialized());
  histogram_tester.ExpectTotalCount("Cobalt.GPU.DeferEglInit.ColdInitDuration",
                                    1);
}

// Covers the user journey where Skia shader blobs from a prior session are read
// from disk during background Preload and replayed on Reveal once real GL
// vendor/renderer strings are known, preserving versioned shader prefix checks.
IN_PROC_BROWSER_TEST_F(
    PreloadEglDeferralBrowserTest,
    ReplaysPreloadedGrShaderCacheBlobsWithValidGlPrefixOnReveal) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));
  FlushGpuMainThread();

  gl::GLDisplayEGL* display = gl::GetDefaultDisplayEGL();
  ASSERT_NE(display, nullptr);
  ASSERT_FALSE(display->IsInitialized());

  auto* gpu_host = content::GpuProcessHost::Get();
  ASSERT_NE(gpu_host, nullptr);
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner =
      gpu_host->GetInProcessGpuThreadTaskRunnerForTesting();
  ASSERT_NE(gpu_task_runner, nullptr);

  // Probe the platform's real GL strings and immediately shut GLDisplayEGL back
  // down inside a single atomic task on Chrome_InProcGpuThread so EGL calls
  // stay on their owning thread and no EstablishGpuChannel task can interleave
  // while deferred_gl_init_pending_ is still set.
  gpu::GPUInfo probed_gpu_info =
      content::GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  bool probe_init_ok = false;
  bool probe_collect_ok = false;
  base::RunLoop probe_loop;
  gpu_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        gl::GLDisplay* probed_display =
            gl::init::InitializeGLNoExtensionsOneOff(
                /*init_bindings=*/false, gl::GpuPreference::kDefault);
        probe_init_ok = probed_display && probed_display->IsInitialized();
        if (probe_init_ok) {
          probe_collect_ok = gpu::CollectContextGraphicsInfo(&probed_gpu_info);
          display->Shutdown();
        }
      }),
      probe_loop.QuitClosure());
  probe_loop.Run();
  ASSERT_TRUE(probe_init_ok);
  ASSERT_TRUE(probe_collect_ok);
  ASSERT_FALSE(probed_gpu_info.gl_renderer.empty());
  ASSERT_FALSE(display->IsInitialized());
  ASSERT_TRUE(content::GpuDataManagerImpl::GetInstance()
                  ->GetGPUInfo()
                  .gl_version.empty());

  const std::string stale_key = base::Base64Encode("stale_empty_gl_shader_key");
  const std::string valid_key = base::Base64Encode("skia_test_shader_key");
  const std::string shader_payload = "compiled_shader_binary_payload";

  // Queue both an invalid-prefix blob and a valid-prefix blob while concealed.
  // With buffering active, LoadedBlob must hold both entries until Reveal
  // rather than memoizing an empty-GL shader prefix key.
  const std::string empty_gl_prefix = BuildShaderPrefix(gpu::GPUInfo());
  const std::string valid_prefix = BuildShaderPrefix(probed_gpu_info);
  gpu_host->gpu_service()->LoadedBlob(gpu::kGrShaderGpuDiskCacheHandle,
                                      empty_gl_prefix + ":" + stale_key,
                                      shader_payload);
  gpu_host->gpu_service()->LoadedBlob(gpu::kGrShaderGpuDiskCacheHandle,
                                      valid_prefix + ":" + valid_key,
                                      shader_payload);
  FlushGpuMainThread();
  histogram_tester.ExpectTotalCount("GPU.ShaderLoadPrefixOK", 0);

  // Reveal populates real GL strings and replays both buffered blobs, rejecting
  // the empty-GL prefix and accepting the valid hardware/ANGLE prefix.
  RevealAndRestoreGpu();
  ASSERT_TRUE(display->IsInitialized());
  histogram_tester.ExpectTotalCount(
      "Cobalt.GPU.DeferEglInit.ShaderBlobReplayDuration", 1);
  histogram_tester.ExpectBucketCount("GPU.ShaderLoadPrefixOK", false, 1);
  histogram_tester.ExpectBucketCount("GPU.ShaderLoadPrefixOK", true, 1);
}

// Covers the user journey where the TV OS signals critical low-memory pressure
// while Cobalt is preloaded in the background, ensuring buffered shader blobs
// are released immediately instead of being held until Reveal.
IN_PROC_BROWSER_TEST_F(
    PreloadEglDeferralBrowserTest,
    EvictsBufferedPreloadShaderBlobsOnCriticalMemoryPressure) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));
  FlushGpuMainThread();

  auto* gpu_host = content::GpuProcessHost::Get();
  ASSERT_NE(gpu_host, nullptr);
  const std::string encoded_key = base::Base64Encode("evicted_shader_key");
  gpu_host->gpu_service()->LoadedBlob(
      gpu::kGrShaderGpuDiskCacheHandle,
      BuildShaderPrefix(gpu::GPUInfo()) + ":" + encoded_key,
      "shader_payload_to_evict");
  FlushGpuMainThread();

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  FlushGpuMainThread();

  RevealAndRestoreGpu();
  histogram_tester.ExpectTotalCount(
      "Cobalt.GPU.DeferEglInit.ShaderBlobReplayDuration", 0);
  histogram_tester.ExpectTotalCount("GPU.ShaderLoadPrefixOK", 0);
}

// Covers the user journey where the user briefly switches to a preloaded Cobalt
// app and immediately switches away (Reveal followed immediately by Conceal
// before waiting for cold init to settle), then opens the app again later.
IN_PROC_BROWSER_TEST_F(PreloadEglDeferralBrowserTest,
                       HandlesRapidRevealFollowedImmediatelyByConceal) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));
  FlushGpuMainThread();

  gl::GLDisplayEGL* display = gl::GetDefaultDisplayEGL();
  ASSERT_NE(display, nullptr);
  ASSERT_FALSE(display->IsInitialized());

  // Trigger OnReveal() and immediately conceal without draining the GPU thread
  // in between; FIFO IO -> GPU main ordering must leave the display torn down.
  content::Shell::GetPlatform()->OnReveal();
  content::Shell::GetPlatform()->MapWindowShell(shell());
  content::Shell::GetPlatform()->OnPageVisibilityVisible(shell());
  ConcealAndCleanupGpu();
  EXPECT_FALSE(display->IsInitialized());
  histogram_tester.ExpectTotalCount("Cobalt.GPU.DeferEglInit.ColdInitDuration",
                                    1);

  // A subsequent Reveal must re-initialize via the fast path cleanly.
  RevealAndRestoreGpu();
  EXPECT_TRUE(display->IsInitialized());
  histogram_tester.ExpectTotalCount("Cobalt.GPU.DeferEglInit.ColdInitDuration",
                                    1);
}

// Covers the user journey where a device launches in preload mode with the
// CobaltDeferEglInitUntilVisible kill switch disabled, verifying that EGL
// initializes eagerly during startup as before deferral was introduced.
IN_PROC_BROWSER_TEST_F(PreloadEglDeferralDisabledBrowserTest,
                       InitializesEglDuringPreloadWhenDisabled) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));
  FlushGpuMainThread();

  EXPECT_FALSE(content::Shell::GetPlatform()->IsVisible());
  gl::GLDisplayEGL* display = gl::GetDefaultDisplayEGL();
  ASSERT_NE(display, nullptr);
  EXPECT_TRUE(display->IsInitialized());
  EXPECT_FALSE(content::GpuDataManagerImpl::GetInstance()
                   ->GetGPUInfo()
                   .gl_version.empty());

  RevealAndRestoreGpu();
  EXPECT_TRUE(display->IsInitialized());
  histogram_tester.ExpectTotalCount("Cobalt.GPU.DeferEglInit.ColdInitDuration",
                                    0);
}

}  // namespace cobalt
