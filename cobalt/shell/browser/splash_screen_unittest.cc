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

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cobalt/browser/features.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "cobalt/shell/common/shell_switches.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace content {

// Stub required to satisfy linker dependencies when the aggregation service
// is not linked in this unit test target.
void AggregatableReport::Provider::SetDisableEncryptionForTestingTool(
    bool should_disable) {}

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

  void CallOnSplashScreenLoadComplete(Shell* shell) {
    shell->OnSplashScreenLoadComplete();
  }

  void CallDidFirstVisuallyNonEmptyPaint(Shell* shell) {
    shell->DidFirstVisuallyNonEmptyPaint();
  }

  Shell::State GetSplashState(Shell* shell) { return shell->splash_state_; }

  bool IsMainFrameLoaded(Shell* shell) { return shell->is_main_frame_loaded_; }

  bool HasSwitchedToMainFrame(Shell* shell) {
    return shell->has_switched_to_main_frame_;
  }

  WebContents* GetSplashScreenWebContents(Shell* shell) {
    return shell->splash_screen_web_contents_.get();
  }

  // Returns the URL the splash screen WebContents is navigating to.
  GURL GetPendingSplashScreenURL(Shell* shell) {
    NavigationEntry* entry =
        GetSplashScreenWebContents(shell)->GetController().GetPendingEntry();
    return entry ? entry->GetURL() : GURL();
  }

  Shell* CreateShell(std::unique_ptr<WebContents> web_contents,
                     std::unique_ptr<WebContents> splash_contents,
                     const std::string& topic = "") {
    EXPECT_CALL(*platform_, SetContents(_));
    Shell* shell =
        new Shell(std::move(web_contents), std::move(splash_contents),
                  /*should_set_delegate=*/false, topic,
                  /*skip_for_testing=*/true);
    platform_->CreatePlatformWindow(shell, gfx::Size(1920, 1080));
    Shell::FinishShellInitialization(shell);
    return shell;
  }

  Shell* CreateShellWithSplashScreen(const std::string& topic = "") {
    WebContents::CreateParams create_params(browser_context());
    create_params.desired_renderer_state =
        WebContents::CreateParams::kNoRendererProcess;
    return CreateShell(
        std::unique_ptr<WebContents>(TestWebContents::Create(create_params)),
        std::unique_ptr<WebContents>(TestWebContents::Create(create_params)),
        topic);
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
  WebContents::CreateParams create_params(browser_context());
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
  CallOnSplashScreenLoadComplete(shell);
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(0);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(IsMainFrameLoaded(shell));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment()->FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_EQ(GetSplashScreenWebContents(shell), nullptr);
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, EarlyMainContentLoad) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));

  EXPECT_CALL(*platform_, LoadSplashScreenContents(shell));
  CallLoadSplashScreenWebContents(shell);
  CallOnSplashScreenLoadComplete(shell);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment()->FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, SplashClosesBeforeMainLoad) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));

  CallLoadSplashScreenWebContents(shell);
  CallClosingSplashScreenWebContents(shell);
  ExpectStateEnded(shell);
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, MainContentLoadAfterSplashEnd) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));

  CallLoadSplashScreenWebContents(shell);
  CallClosingSplashScreenWebContents(shell);
  ExpectStateEnded(shell);
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, DestroyShellWhileSwitching) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));

  CallLoadSplashScreenWebContents(shell);
  CallOnSplashScreenLoadComplete(shell);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  // Fast forward time to trigger any pending tasks.
  task_environment()->FastForwardBy(base::Milliseconds(1600));
}

TEST_F(SplashScreenTest, MainContentLoadFailure) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallLoadProgressChanged(shell, 0.5);
  task_environment()->FastForwardBy(base::Milliseconds(1600));
  EXPECT_FALSE(IsMainFrameLoaded(shell));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_NE(GetSplashScreenWebContents(shell), nullptr);
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, NoSplashScreen) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));

  Shell* shell = CreateShell(std::move(web_contents), nullptr);

  EXPECT_FALSE(
      IsMainFrameLoaded(shell));  // This flag is only set in splash flow
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  task_environment()->FastForwardBy(base::Milliseconds(1600));
  ExpectStateUninitialized(shell);
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, SplashClosesDuringDelay) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
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
  task_environment()->FastForwardBy(base::Milliseconds(1600));
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, MultipleLoadProgressEvents) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));

  CallLoadSplashScreenWebContents(shell);
  CallOnSplashScreenLoadComplete(shell);
  CallLoadProgressChanged(shell, 0.5);
  EXPECT_FALSE(IsMainFrameLoaded(shell));
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(IsMainFrameLoaded(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment()->FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, SplashTimeoutExceededBeforeMainLoad) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));
  CallLoadSplashScreenWebContents(shell);
  CallOnSplashScreenLoadComplete(shell);
  task_environment_.FastForwardBy(base::Milliseconds(2000));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  CallLoadProgressChanged(shell, 1.0);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, MainContentLoadBeforeSplashStart) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));

  CallLoadProgressChanged(shell, 1.0);
  EXPECT_CALL(*platform_, LoadSplashScreenContents(shell));
  CallLoadSplashScreenWebContents(shell);
  CallOnSplashScreenLoadComplete(shell);
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment()->FastForwardBy(base::Milliseconds(2000));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, MainContentDidStopLoadingFallback) {
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));

  EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
  Shell* shell =
      CreateShell(std::move(web_contents), std::move(splash_contents));

  CallLoadSplashScreenWebContents(shell);
  CallOnSplashScreenLoadComplete(shell);
  CallLoadProgressChanged(shell, 0.9);
  EXPECT_FALSE(IsMainFrameLoaded(shell));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  CallDidStopLoading(shell);
  EXPECT_TRUE(IsMainFrameLoaded(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment()->FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, PreloadSkipsSplashScreen) {
  // Re-initialize Shell in a hidden (preloaded) state for this test.
  ClearPlatform();
  Shell::Shutdown();
  InitializeShell(false /* is_visible */);

  // Attempt to create a window with a splash screen requested.
  Shell* shell = Shell::CreateNewWindow(
      browser_context(), GURL("about:blank"), nullptr, gfx::Size(),
      true /* create_splash_screen_web_contents */);

  // Verify that the main contents exist but the splash screen was skipped.
  ASSERT_NE(shell->web_contents(), nullptr);
  EXPECT_EQ(shell->splash_screen_web_contents(), nullptr);
  ExpectStateUninitialized(shell);

  // Verify that the main contents are initially hidden.
  EXPECT_EQ(shell->web_contents()->GetVisibility(), Visibility::HIDDEN);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, SplashTimerStartsOnLoadComplete) {
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
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  CallOnSplashScreenLoadComplete(shell);
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(1600));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));
  shell->Close();
}

TEST_F(SplashScreenTest,
       HideSystemSplashScreenOnSplashDidFirstVisuallyNonEmptyPaint) {
  Shell::ResetSystemSplashScreenForTesting();
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> splash_contents(
      TestWebContents::Create(create_params));
  SplashScreenWebContentsObserver observer(splash_contents.get(),
                                           base::DoNothing());

  // First paint of splash screen should trigger system splash dismissal.
  observer.DidFirstVisuallyNonEmptyPaint();

  // Subsequent calls should be safe no-ops.
  observer.DidFirstVisuallyNonEmptyPaint();
}

TEST_F(
    SplashScreenTest,
    HideSystemSplashScreenOnMainDidFirstVisuallyNonEmptyPaintWhenSplashSkipped) {
  Shell::ResetSystemSplashScreenForTesting();
  WebContents::CreateParams create_params(browser_context());
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents(
      TestWebContents::Create(create_params));
  Shell* shell = CreateShell(std::move(web_contents), nullptr);

  // When splash is omitted, first paint on the main page dismisses the system
  // splash.
  CallDidFirstVisuallyNonEmptyPaint(shell);

  // Subsequent calls should be safe no-ops.
  CallDidFirstVisuallyNonEmptyPaint(shell);
  shell->Close();
}

TEST_F(SplashScreenTest,
       HideSystemSplashScreenWhenSplashScreenFeatureDisabled) {
  Shell::ResetSystemSplashScreenForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      cobalt::features::kDisableSplashScreen);
  EXPECT_FALSE(switches::ShouldCreateSplashScreen());

  EXPECT_CALL(*platform_, SetContents(_));
  Shell* shell =
      Shell::CreateNewWindow(browser_context(), GURL("about:blank"), nullptr,
                             gfx::Size(), switches::ShouldCreateSplashScreen());

  ASSERT_NE(shell->web_contents(), nullptr);
  EXPECT_EQ(shell->splash_screen_web_contents(), nullptr);

  // When splash is disabled by feature/flag, first paint on the main page
  // dismisses the system splash.
  CallDidFirstVisuallyNonEmptyPaint(shell);

  // Subsequent calls should be safe no-ops.
  CallDidFirstVisuallyNonEmptyPaint(shell);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, SplashTimeoutFromFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kSplashScreenConfig, {{"timeout", "3s"}});
  Shell* shell = CreateShellWithSplashScreen();

  CallLoadSplashScreenWebContents(shell);
  CallOnSplashScreenLoadComplete(shell);
  CallLoadProgressChanged(shell, 1.0);
  EXPECT_TRUE(IsMainFrameLoaded(shell));

  // The default 1500ms timeout is overridden by the feature param.
  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(2900));
  EXPECT_FALSE(HasSwitchedToMainFrame(shell));

  EXPECT_CALL(*platform_, UpdateContents(shell)).Times(1);
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(HasSwitchedToMainFrame(shell));

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, SplashUrlDefaultsToBuiltInSplashScreen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      cobalt::features::kForceVideoSplashScreen);
  Shell* shell = CreateShellWithSplashScreen();

  CallLoadSplashScreenWebContents(shell);
  // No "timeout" query param, as SplashScreenConfig is disabled.
  EXPECT_EQ(
      GetPendingSplashScreenURL(shell),
      GURL(std::string(switches::kSplashScreenURL) + "?force_image=true"));

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
TEST_F(SplashScreenTest, SplashUrlFromFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{cobalt::features::kSplashScreenConfig,
        {{"url", "https://www.example.com/splash.html"}}}},
      {cobalt::features::kForceVideoSplashScreen});
  Shell* shell = CreateShellWithSplashScreen(/*topic=*/"music");

  CallLoadSplashScreenWebContents(shell);
  // The "force_image", "cache" and "timeout" query params are still appended.
  EXPECT_EQ(GetPendingSplashScreenURL(shell),
            GURL("https://www.example.com/splash.html"
                 "?force_image=true&cache=music&timeout=1500"));

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

TEST_F(SplashScreenTest, InvalidSplashUrlFallsBackToBuiltInSplashScreen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{cobalt::features::kSplashScreenConfig, {{"url", "not a url"}}}},
      {cobalt::features::kForceVideoSplashScreen});
  Shell* shell = CreateShellWithSplashScreen();

  CallLoadSplashScreenWebContents(shell);
  EXPECT_EQ(GetPendingSplashScreenURL(shell),
            GURL(std::string(switches::kSplashScreenURL) +
                 "?force_image=true&timeout=1500"));

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}
#else   // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
TEST_F(SplashScreenTest, SplashUrlIgnoredInGoldBuilds) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{cobalt::features::kSplashScreenConfig,
        {{"url", "https://www.example.com/splash.html"}}}},
      {cobalt::features::kForceVideoSplashScreen});
  Shell* shell = CreateShellWithSplashScreen();

  CallLoadSplashScreenWebContents(shell);
  // The "url" param doesn't exist in gold builds, so the built-in splash screen
  // is always used.
  EXPECT_EQ(GetPendingSplashScreenURL(shell),
            GURL(std::string(switches::kSplashScreenURL) +
                 "?force_image=true&timeout=1500"));

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

TEST_F(SplashScreenTest, SplashTimeoutPassedToSplashScreen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{cobalt::features::kSplashScreenConfig, {{"timeout", "3s"}}}},
      {cobalt::features::kForceVideoSplashScreen});
  Shell* shell = CreateShellWithSplashScreen();

  CallLoadSplashScreenWebContents(shell);
  // The splash screen gets the timeout in ms, so it stays up until then.
  EXPECT_EQ(GetPendingSplashScreenURL(shell),
            GURL(std::string(switches::kSplashScreenURL) +
                 "?force_image=true&timeout=3000"));

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
}

}  // namespace content
