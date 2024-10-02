// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_child_process_backgrounded_bridge.h"

#include "base/process/process.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "gpu/config/gpu_finch_features.h"

namespace content {

bool IsProcessBackgrounded(base::ProcessId pid) {
  base::Process process = base::Process::Open(pid);
  if (process.is_current()) {
    base::SelfPortProvider self_port_provider;
    return process.IsProcessBackgrounded(&self_port_provider);
  }

  return process.IsProcessBackgrounded(
      content::BrowserChildProcessHost::GetPortProvider());
}

void SetProcessBackgrounded(base::ProcessId pid, bool backgrounded) {
  base::Process process = base::Process::Open(pid);
  if (process.is_current()) {
    base::SelfPortProvider self_port_provider;
    process.SetProcessBackgrounded(&self_port_provider, backgrounded);
    return;
  }

  process.SetProcessBackgrounded(
      content::BrowserChildProcessHost::GetPortProvider(), backgrounded);
}

class BrowserChildProcessBackgroundedBridgeTest
    : public content::ContentBrowserTest,
      public base::PortProvider::Observer,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAdjustGpuProcessPriority);
    content::BrowserChildProcessBackgroundedBridge::
        SetOSNotificationsEnabledForTesting(false);
    content::ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    content::ContentBrowserTest::TearDown();
    content::BrowserChildProcessBackgroundedBridge::
        SetOSNotificationsEnabledForTesting(true);
  }

  // Waits until the port for the GPU process is available.
  void WaitForPort() {
    auto* port_provider = content::BrowserChildProcessHost::GetPortProvider();
    DCHECK(port_provider->TaskForPid(
               content::GpuProcessHost::Get()->process_id()) == MACH_PORT_NULL);
    port_provider->AddObserver(this);
    base::RunLoop run_loop;

    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void EnsureBackgroundedStateChange() {
    // Do a round-trip to the process launcher task to ensure any queued task is
    // run.
    base::RunLoop run_loop;
    GetProcessLauncherTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  void OnReceivedTaskPort(base::ProcessHandle process_handle) override {
    if (process_handle != content::GpuProcessHost::Get()->process_id()) {
      return;
    }

    content::BrowserChildProcessHost::GetPortProvider()->RemoveObserver(this);
    std::move(quit_closure_).Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(BrowserChildProcessBackgroundedBridgeTest,
                       InitiallyForegrounded) {
  // Set the browser process as foregrounded.
  SetProcessBackgrounded(base::Process::Current().Pid(), false);

  // Wait until we receive the port for the GPU process.
  WaitForPort();

  // Ensure that the initial backgrounded state changed.
  EnsureBackgroundedStateChange();

  auto* gpu_process_host = content::GpuProcessHost::Get();
  EXPECT_TRUE(gpu_process_host);
  EXPECT_FALSE(IsProcessBackgrounded(gpu_process_host->process_id()));
}

// TODO(crbug.com/1426160): Disabled because this test is flaky.
IN_PROC_BROWSER_TEST_F(BrowserChildProcessBackgroundedBridgeTest,
                       DISABLED_InitiallyBackgrounded) {
  // Set the browser process as backgrounded.
  SetProcessBackgrounded(base::Process::Current().Pid(), true);

  // Wait until we receive the port for the GPU process.
  WaitForPort();

  // Ensure that the initial backgrounded state changed.
  EnsureBackgroundedStateChange();

  auto* gpu_process_host = content::GpuProcessHost::Get();
  EXPECT_TRUE(gpu_process_host);
  EXPECT_TRUE(IsProcessBackgrounded(gpu_process_host->process_id()));
}

IN_PROC_BROWSER_TEST_F(BrowserChildProcessBackgroundedBridgeTest,
                       OnBackgroundedStateChanged) {
  // Wait until we receive the port for the GPU process.
  WaitForPort();

  auto* gpu_process_host = content::GpuProcessHost::Get();
  EXPECT_TRUE(gpu_process_host);

  auto* bridge =
      gpu_process_host->browser_child_process_backgrounded_bridge_for_testing();
  ASSERT_TRUE(bridge);

  bridge->SimulateBrowserProcessForegroundedForTesting();
  EnsureBackgroundedStateChange();

  EXPECT_FALSE(IsProcessBackgrounded(gpu_process_host->process_id()));

  bridge->SimulateBrowserProcessBackgroundedForTesting();
  EnsureBackgroundedStateChange();

  EXPECT_TRUE(IsProcessBackgrounded(gpu_process_host->process_id()));

  bridge->SimulateBrowserProcessForegroundedForTesting();
  EnsureBackgroundedStateChange();

  EXPECT_FALSE(IsProcessBackgrounded(gpu_process_host->process_id()));
}

}  // namespace content
