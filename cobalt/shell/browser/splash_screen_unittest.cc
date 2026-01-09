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

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_storage_partition.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_web_contents.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace content {

class MockShellPlatformDelegate : public ShellPlatformDelegate {
 public:
  MOCK_METHOD(void,
              Initialize,
              (const gfx::Size& default_window_size),
              (override));
  MOCK_METHOD(void,
              CreatePlatformWindow,
              (Shell * shell, const gfx::Size& initial_size),
              (override));
  MOCK_METHOD(void, SetContents, (Shell * shell), (override));
  MOCK_METHOD(void, LoadSplashScreenContents, (Shell * shell), (override));
  MOCK_METHOD(void, UpdateContents, (Shell * shell), (override));
  MOCK_METHOD(void,
              ResizeWebContent,
              (Shell * shell, const gfx::Size& content_size),
              (override));
  MOCK_METHOD(bool, DestroyShell, (Shell * shell), (override));
  MOCK_METHOD(void, CleanUp, (Shell * shell), (override));
  MOCK_METHOD(void, DidCloseLastWindow, (), (override));
};

class TestBrowserAccessibilityState : public BrowserAccessibilityStateImpl {
 public:
  TestBrowserAccessibilityState() = default;
};

class SplashScreenTest : public testing::Test {
 public:
  SplashScreenTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ForceInProcessNetworkService();
    mojo::core::Init();
    ui::DeviceDataManager::CreateInstance();
#if defined(USE_AURA)
    env_ = aura::Env::CreateInstance();
#endif
    browser_accessibility_state_ =
        std::make_unique<TestBrowserAccessibilityState>();

    SetContentClient(&test_content_client_);
    SetBrowserClientForTesting(&test_content_browser_client_);

    rvh_enabler_ = std::make_unique<RenderViewHostTestEnabler>();

    auto platform = std::make_unique<NiceMock<MockShellPlatformDelegate>>();
    platform_ = platform.get();
    Shell::Initialize(std::move(platform));
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  void TearDown() override {
    platform_ = nullptr;
    Shell::Shutdown();
    browser_context_.reset();
    rvh_enabler_.reset();
    SetBrowserClientForTesting(nullptr);
    SetContentClient(nullptr);
    browser_accessibility_state_.reset();
#if defined(USE_AURA)
    env_.reset();
#endif
    ui::DeviceDataManager::DeleteInstance();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestContentClient test_content_client_;
  TestContentBrowserClient test_content_browser_client_;
#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env_;
#endif
  std::unique_ptr<BrowserAccessibilityState> browser_accessibility_state_;
  std::unique_ptr<RenderViewHostTestEnabler> rvh_enabler_;
  raw_ptr<NiceMock<MockShellPlatformDelegate>> platform_;
  std::unique_ptr<TestBrowserContext> browser_context_;

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

}  // namespace content
