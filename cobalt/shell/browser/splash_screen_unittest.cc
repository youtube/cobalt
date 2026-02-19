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

#include "cobalt/shell/browser/shell_test_support.h"
#include "content/test/test_web_contents.h"

using testing::_;

namespace content {

class SplashScreenTest : public ShellTestBase {
 public:
  SplashScreenTest() = default;

  void SetUp() override {
    ShellTestBase::SetUp();
    InitializeShell(true /* is_visible */);
  }

 protected:
  void CallLoadProgressChanged(Shell* shell, double progress) {
    shell->LoadProgressChanged(progress);
  }

  void CallLoadSplashScreenWebContents(Shell* shell) {
    shell->LoadSplashScreenWebContents();
  }

  void CallClosingSplashScreenWebContents(Shell* shell) {
    shell->ClosingSplashScreenWebContents();
  }

  void CallDidStopLoading(Shell* shell) { shell->DidStopLoading(); }

  Shell::State GetSplashState(Shell* shell) { return shell->splash_state_; }

  bool IsMainFrameLoaded(Shell* shell) { return shell->is_main_frame_loaded_; }

  bool HasSwitchedToMainFrame(Shell* shell) {
    return shell->has_switched_to_main_frame_;
  }

  WebContents* GetSplashScreenWebContents(Shell* shell) {
    return shell->splash_screen_web_contents_.get();
  }

  Shell* CreateShell(std::unique_ptr<WebContents> web_contents,
                     std::unique_ptr<WebContents> splash_contents) {
    Shell* shell =
        new Shell(std::move(web_contents), std::move(splash_contents),
                  /*should_set_delegate=*/false,
                  /*topic*/ "",
                  /*skip_for_testing=*/true);
    platform_->CreatePlatformWindow(shell, gfx::Size());
    Shell::FinishShellInitialization(shell);
    return shell;
  }

  void ExpectStateUninitialized(Shell* shell) {
    EXPECT_EQ(GetSplashState(shell), Shell::STATE_SPLASH_SCREEN_UNINITIALIZED);
  }

  void ExpectStateInitialized(Shell* shell) {
    EXPECT_EQ(GetSplashState(shell), Shell::STATE_SPLASH_SCREEN_INITIALIZED);
  }

  void ExpectStateStarted(Shell* shell) {
    EXPECT_EQ(GetSplashState(shell), Shell::STATE_SPLASH_SCREEN_STARTED);
  }

  void ExpectStateEnded(Shell* shell) {
    EXPECT_EQ(GetSplashState(shell), Shell::STATE_SPLASH_SCREEN_ENDED);
  }
};

TEST_F(SplashScreenTest, ParallelLoading) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  ExpectStateInitialized(shell);
  EXPECT_CALL(*platform_, LoadSplashScreenContents(shell));
  CallLoadSplashScreenWebContents(shell);
  ExpectStateStarted(shell);
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(0);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(IsMainFrameLoaded(shell));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_EQ(GetSplashScreenWebContents(shell), nullptr);
  EXPECT_CALL(*platform_, DestroyShell(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, EarlyMainContentLoad) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  EXPECT_CALL(*platform_, LoadSplashScreenContents(shell));
  CallLoadSplashScreenWebContents(shell);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, SplashClosesBeforeMainLoad) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallClosingSplashScreenWebContents(shell);
  ExpectStateEnded(shell);
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, MainContentLoadAfterSplashEnd) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallClosingSplashScreenWebContents(shell);
  ExpectStateEnded(shell);
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, DestroyShellWhileSwitching) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallLoadProgressChanged(shell, 1.0);
  // Shell should be waiting for the splash timeout.
  // Destroy the shell before the timeout expires.
  EXPECT_CALL(*platform_, DestroyShell(shell));
  shell->Close();
  // Fast forward time to trigger any pending tasks.
  task_environment_.FastForwardBy(base::Milliseconds(1600));
}

TEST_F(SplashScreenTest, MainContentLoadFailure) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallLoadProgressChanged(shell, 0.5);
  task_environment_.FastForwardBy(base::Milliseconds(1600));
  EXPECT_FALSE(IsMainFrameLoaded(shell));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_NE(GetSplashScreenWebContents(shell), nullptr);
  shell->Close();
}

TEST_F(SplashScreenTest, NoSplashScreen) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  Shell* shell = CreateShell(std::move(web_contents), nullptr);
  ExpectStateUninitialized(shell);
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(0);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_FALSE(
      IsMainFrameLoaded(shell));  // This flag is only set in splash flow
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  task_environment_.FastForwardBy(base::Milliseconds(1600));
  ExpectStateUninitialized(shell);
  shell->Close();
}

TEST_F(SplashScreenTest, SplashClosesDuringDelay) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(IsMainFrameLoaded(shell));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  CallClosingSplashScreenWebContents(shell);
  ExpectStateEnded(shell);
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  task_environment_.FastForwardBy(base::Milliseconds(1600));
  shell->Close();
}

TEST_F(SplashScreenTest, MultipleLoadProgressEvents) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallLoadProgressChanged(shell, 0.5);
  EXPECT_FALSE(IsMainFrameLoaded(shell));
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(IsMainFrameLoaded(shell));
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, SplashTimeoutExceededBeforeMainLoad) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  task_environment_.FastForwardBy(base::Milliseconds(2000));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  CallLoadProgressChanged(shell, 1.0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, MainContentLoadBeforeSplashStart) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_CALL(*platform_, LoadSplashScreenContents(shell));
  CallLoadSplashScreenWebContents(shell);
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(2000));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, MainContentDidStopLoadingFallback) {
  WebContents::CreateParams create_params(browser_context_.get());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallLoadProgressChanged(shell, 0.9);
  EXPECT_FALSE(IsMainFrameLoaded(shell));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  CallDidStopLoading(shell);
  EXPECT_TRUE(IsMainFrameLoaded(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, PreloadSkipsSplashScreen) {
  // Re-initialize Shell in a hidden (preloaded) state for this test.
  Shell::Shutdown();
  InitializeShell(false /* is_visible */);

  // Attempt to create a window with a splash screen requested.
  Shell* shell = Shell::CreateNewWindow(
      browser_context_.get(), GURL("about:blank"), nullptr, gfx::Size(),
      true /* create_splash_screen_web_contents */);

  // Verify that the main contents exist but the splash screen was skipped.
  ASSERT_NE(shell->web_contents(), nullptr);
  EXPECT_EQ(shell->splash_screen_web_contents(), nullptr);
  ExpectStateUninitialized(shell);

  // Verify that the main contents are initially hidden.
  EXPECT_EQ(shell->web_contents()->GetVisibility(), Visibility::HIDDEN);

  shell->Close();
}

}  // namespace content
