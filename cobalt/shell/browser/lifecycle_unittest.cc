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

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/threading_features.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

class LifecycleTest : public ShellTestBase {
 public:
  LifecycleTest() = default;

  void SetUp() override {
    ShellTestBase::SetUp();
    // Mirror CobaltMainDelegate + ContentMainRunnerImpl startup: initialize and
    // start HangWatcher in the enabled state before Shell::Initialize() runs.
    // Disable ThreadPool registration (matching CobaltHangWatcherDelegate) so
    // ShellTestBase's longer-lived ThreadPool workers do not outlive
    // hang_watcher_ and UAF in UnregisterThread() during ~ShellTestBase().
    hang_watcher_feature_list_.InitAndEnableFeatureWithParameters(
        base::kEnableHangWatcher, {{"threadpool_log_level", "0"}});
    base::HangWatcher::InitializeOnMainThread(
        base::HangWatcher::ProcessType::kBrowserProcess,
        /*emit_crashes=*/false);
    hang_watcher_ = std::make_unique<base::HangWatcher>();
    hang_watcher_->SetMonitoringPeriodForTesting(base::Hours(10));
    hang_watcher_->Start();
  }

  void CreateTestShell(bool is_visible) {
    InitializeShell(is_visible);
    WebContents::CreateParams create_params(browser_context());
    create_params.desired_renderer_state =
        WebContents::CreateParams::kNoRendererProcess;
    create_params.initially_hidden = !is_visible;
    std::unique_ptr<WebContents> web_contents(
        TestWebContents::Create(create_params));

    if (is_visible) {
      EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
    }
    EXPECT_CALL(*platform_, SetContents(_));

    shell_ = new Shell(std::move(web_contents), nullptr /* splash_contents */,
                       /*should_set_delegate=*/true, /*topic=*/"",
                       /*skip_for_testing=*/true);

    if (is_visible) {
      platform_->CreatePlatformWindow(shell_, gfx::Size(1920, 1080));
    }

    Shell::FinishShellInitialization(shell_);
  }

  void TearDown() override {
    if (shell_) {
      EXPECT_CALL(*platform_, DestroyShell(shell_));
      EXPECT_CALL(*platform_, CleanUp(shell_));
      shell_->Close();
      task_environment()->RunUntilIdle();
      shell_ = nullptr;
    }
    hang_watcher_.reset();
    base::HangWatcher::UninitializeOnMainThreadForTesting();
    ShellTestBase::TearDown();
  }

  base::test::ScopedFeatureList hang_watcher_feature_list_;
  std::unique_ptr<base::HangWatcher> hang_watcher_;
  Shell* shell_ = nullptr;
};

TEST_F(LifecycleTest, StartupVisible) {
  ASSERT_TRUE(base::HangWatcher::IsEnabled());
  CreateTestShell(true /* is_visible */);
  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::VISIBLE);
  EXPECT_TRUE(base::HangWatcher::IsEnabled());
}

TEST_F(LifecycleTest, StartupHidden) {
  ASSERT_TRUE(base::HangWatcher::IsEnabled());
  CreateTestShell(false /* is_visible */);
  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::HIDDEN);
  EXPECT_FALSE(base::HangWatcher::IsEnabled());
}

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_STARBOARD)
TEST_F(LifecycleTest, ClosingActivityWindowPreservesPlatformForRecreation) {
  CreateTestShell(true);
  ShellPlatformDelegate* platform = Shell::GetPlatform();
  EXPECT_CALL(*platform_, DestroyShell(shell_));
  EXPECT_CALL(*platform_, CleanUp(shell_));
  EXPECT_CALL(*platform_, DidCloseLastWindow()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::DidCloseLastWindow();
  });

  shell_->Close();
  shell_ = nullptr;

  EXPECT_TRUE(Shell::windows().empty());
  EXPECT_EQ(platform, Shell::GetPlatform());
}
#endif

TEST_F(LifecycleTest, Reveal) {
  CreateTestShell(false /* is_visible */);
  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::HIDDEN);
  EXPECT_FALSE(base::HangWatcher::IsEnabled());

  // Simulate that the frame was visible before conceal, so OnReveal will
  // trigger WasShown().
  platform_->AddWebContentsToRevealForTesting(shell_->web_contents());

  // Trigger reveal.
  EXPECT_CALL(*platform_, OnReveal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnReveal();
  });
  EXPECT_CALL(*platform_, RevealShell(shell_))
      .WillOnce(
          [](content::Shell* shell) { shell->web_contents()->WasShown(); });
  Shell::OnReveal();

  EXPECT_TRUE(platform_->IsVisible());
  // Visibility should now be VISIBLE because we called WasShown() in OnReveal
  // to unblock the renderer.
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::VISIBLE);
  // HangWatcher must be resumed when revealed.
  EXPECT_TRUE(base::HangWatcher::IsEnabled());

  // Simulate Reveal ACK.
  platform_->OnPageVisibilityVisible(shell_);

  // Now visibility should be VISIBLE.
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::VISIBLE);
}

TEST_F(LifecycleTest, RedundantReveal) {
  CreateTestShell(false /* is_visible */);

  // First reveal.
  EXPECT_CALL(*platform_, OnReveal()).Times(1).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnReveal();
  });
  EXPECT_CALL(*platform_, RevealShell(shell_));
  Shell::OnReveal();

  // Second reveal (should be ignored by idempotent Shell logic).
  EXPECT_CALL(*platform_, OnReveal()).Times(0);
  Shell::OnReveal();
}

TEST_F(LifecycleTest, Conceal) {
  CreateTestShell(true /* is_visible */);
  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::VISIBLE);
  EXPECT_TRUE(base::HangWatcher::IsEnabled());

  // Trigger conceal.
  EXPECT_CALL(*platform_, OnConceal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnConceal();
  });
  EXPECT_CALL(*platform_, ConcealShell(shell_));
  Shell::OnConceal();

  // Symmetrical unit-test trigger: since there are no active Blink frames to
  // dispatch Mojo visibility ACKs, we manually invoke the observer callback
  // on the platform manager base class.
  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesConcealed(shell_->web_contents());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::HIDDEN);
  // HangWatcher must be suspended when concealed.
  EXPECT_FALSE(base::HangWatcher::IsEnabled());
}

TEST_F(LifecycleTest, FreezeUnfreeze) {
  CreateTestShell(false /* is_visible */);
  TestWebContents* test_web_contents =
      static_cast<TestWebContents*>(shell_->web_contents());
  EXPECT_FALSE(base::HangWatcher::IsEnabled());

  // Trigger freeze.
  EXPECT_CALL(*platform_, OnFreeze()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnFreeze();
  });
  Shell::OnFreeze();
  EXPECT_TRUE(test_web_contents->IsPageFrozen());
  EXPECT_FALSE(base::HangWatcher::IsEnabled());

  // Trigger unfreeze.
  EXPECT_CALL(*platform_, OnUnfreeze()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnUnfreeze();
  });
  Shell::OnUnfreeze();
  EXPECT_FALSE(test_web_contents->IsPageFrozen());
  // HangWatcher must NOT be prematurely resumed while still concealed.
  EXPECT_FALSE(base::HangWatcher::IsEnabled());
}

TEST_F(LifecycleTest, HangWatcherLifecycleSymmetry) {
  CreateTestShell(true /* is_visible */);
  EXPECT_TRUE(platform_->IsVisible());

  // 1. Initial foreground state: HangWatcher starts active.
  EXPECT_TRUE(base::HangWatcher::IsEnabled());

  // 2. Conceal (Home button pressed): HangWatcher suspends.
  EXPECT_CALL(*platform_, OnConceal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnConceal();
  });
  EXPECT_CALL(*platform_, ConcealShell(shell_));
  Shell::OnConceal();
  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesConcealed(shell_->web_contents());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_FALSE(base::HangWatcher::IsEnabled());

  // 3. Freeze: HangWatcher remains suspended.
  EXPECT_CALL(*platform_, OnFreeze()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnFreeze();
  });
  Shell::OnFreeze();
  EXPECT_FALSE(base::HangWatcher::IsEnabled());

  // 4. Unfreeze: HangWatcher remains suspended (no premature resume while
  // concealed).
  EXPECT_CALL(*platform_, OnUnfreeze()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnUnfreeze();
  });
  Shell::OnUnfreeze();
  EXPECT_FALSE(base::HangWatcher::IsEnabled());

  // 5. Reveal (App foregrounded): HangWatcher resumes.
  platform_->AddWebContentsToRevealForTesting(shell_->web_contents());
  EXPECT_CALL(*platform_, OnReveal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnReveal();
  });
  EXPECT_CALL(*platform_, RevealShell(shell_))
      .WillOnce(
          [](content::Shell* shell) { shell->web_contents()->WasShown(); });
  Shell::OnReveal();
  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesVisible(shell_->web_contents());

  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_TRUE(base::HangWatcher::IsEnabled());
}

TEST_F(LifecycleTest, PreloadFreezeUnfreezeCyclesStaySuspendedUntilReveal) {
  ASSERT_TRUE(base::HangWatcher::IsEnabled());
  CreateTestShell(false /* is_visible */);
  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_FALSE(base::HangWatcher::IsEnabled());

  for (int i = 0; i < 2; ++i) {
    Shell::OnFreeze();
    EXPECT_FALSE(base::HangWatcher::IsEnabled());
    Shell::OnUnfreeze();
    EXPECT_FALSE(base::HangWatcher::IsEnabled());
  }

  platform_->AddWebContentsToRevealForTesting(shell_->web_contents());
  EXPECT_CALL(*platform_, RevealShell(shell_))
      .WillOnce(
          [](content::Shell* shell) { shell->web_contents()->WasShown(); });
  Shell::OnReveal();
  platform_->OnPageVisibilityVisible(shell_);
  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesVisible(shell_->web_contents());
  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_TRUE(base::HangWatcher::IsEnabled());
}

TEST_F(LifecycleTest, BlurFocus) {
  CreateTestShell(true /* is_visible */);
  EXPECT_TRUE(base::HangWatcher::IsEnabled());

  // Trigger blur.
  EXPECT_CALL(*platform_, OnBlur());
  Shell::OnBlur();
  EXPECT_TRUE(base::HangWatcher::IsEnabled());

  // Trigger focus.
  EXPECT_CALL(*platform_, OnFocus());
  Shell::OnFocus();
  EXPECT_TRUE(base::HangWatcher::IsEnabled());
}

}  // namespace content
