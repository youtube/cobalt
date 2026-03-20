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

#include "cobalt/app/app_lifecycle_delegate.h"

#include "base/functional/bind.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace cobalt {

class MockAppLifecycleRunner : public AppLifecycleRunner {
 public:
  MockAppLifecycleRunner() = default;
  ~MockAppLifecycleRunner() override = default;

  MOCK_METHOD(void, InitializeSystem, (), (override));
  MOCK_METHOD(void, CreateMainDelegate, (bool, const char*), (override));
  MOCK_METHOD(cobalt::CobaltMainDelegate*, GetMainDelegate, (), (override));
  MOCK_METHOD(int, Run, (content::ContentMainParams), (override));
  MOCK_METHOD(void, ShutDown, (), (override));
  MOCK_METHOD(bool, IsRunning, (), (const, override));
};

class AppLifecycleDelegateTest : public content::ShellTestBase {
 public:
  AppLifecycleDelegateTest() {
    auto runner = std::make_unique<MockAppLifecycleRunner>();
    runner_ = runner.get();
    delegate_ = std::make_unique<AppLifecycleDelegate>(std::move(runner));
  }

 protected:
  content::Shell* CreateTestShell(bool is_visible) {
    InitializeShell(is_visible);
    content::WebContents::CreateParams create_params(browser_context_.get());
    create_params.desired_renderer_state =
        content::WebContents::CreateParams::kNoRendererProcess;
    create_params.initially_hidden = !is_visible;
    std::unique_ptr<content::WebContents> web_contents(
        content::TestWebContents::Create(create_params));

    content::Shell* shell = new content::Shell(std::move(web_contents),
                                               nullptr /* splash_contents */,
                                               /*should_set_delegate=*/true,
                                               /*topic*/ "",
                                               /*skip_for_testing=*/true);
    if (is_visible) {
      EXPECT_CALL(*platform_, CreatePlatformWindow(shell, _));
      content::Shell::GetPlatform()->CreatePlatformWindow(shell, gfx::Size());
    }
    EXPECT_CALL(*platform_, SetContents(shell));
    content::Shell::FinishShellInitialization(shell);
    return shell;
  }

  void SendStartEvent(SbEventType type) {
    SbEventStartData data = {nullptr, 0, nullptr};
    SbEvent event = {type, 0, &data};
    delegate_->HandleEvent(&event);
  }

  void SendRevealEvent() {
    SbEvent event = {kSbEventTypeReveal, 0, nullptr};
    delegate_->HandleEvent(&event);
  }

  void SendStopEvent() {
    SbEvent event = {kSbEventTypeStop, 0, nullptr};
    delegate_->HandleEvent(&event);
  }

  MockAppLifecycleRunner* runner_;
  std::unique_ptr<AppLifecycleDelegate> delegate_;
};

TEST_F(AppLifecycleDelegateTest, StartVisible) {
  InitializeShell(true);

  EXPECT_CALL(*runner_, InitializeSystem());
  EXPECT_CALL(*runner_, CreateMainDelegate(true, _));
  EXPECT_CALL(*runner_, GetMainDelegate()).WillOnce(Return(nullptr));
  EXPECT_CALL(*runner_, Run(_)).WillOnce(Return(0));

  SendStartEvent(kSbEventTypeStart);

  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(true));
  EXPECT_TRUE(delegate_->IsRunning());
  EXPECT_TRUE(platform_->IsVisible());
}

TEST_F(AppLifecycleDelegateTest, StartPreloaded) {
  InitializeShell(false);

  EXPECT_CALL(*runner_, InitializeSystem());
  EXPECT_CALL(*runner_, CreateMainDelegate(false, _));
  EXPECT_CALL(*runner_, GetMainDelegate()).WillOnce(Return(nullptr));
  EXPECT_CALL(*runner_, Run(_)).WillOnce(Return(0));

  SendStartEvent(kSbEventTypePreload);

  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(true));
  EXPECT_TRUE(delegate_->IsRunning());
  EXPECT_FALSE(platform_->IsVisible());
}

TEST_F(AppLifecycleDelegateTest, IntegratedReveal) {
  content::Shell* shell = CreateTestShell(false /* is_visible */);
  EXPECT_FALSE(platform_->IsVisible());

  EXPECT_CALL(*platform_, RevealShell(shell));
  SendRevealEvent();

  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(),
            content::Visibility::VISIBLE);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  shell->Close();
}

TEST_F(AppLifecycleDelegateTest, IntegratedRedundantReveal) {
  content::Shell* shell = CreateTestShell(false /* is_visible */);

  EXPECT_CALL(*platform_, RevealShell(shell)).Times(1);
  SendRevealEvent();

  EXPECT_CALL(*platform_, RevealShell(_)).Times(0);
  SendRevealEvent();

  EXPECT_CALL(*platform_, DestroyShell(shell));
  shell->Close();
}

TEST_F(AppLifecycleDelegateTest, RevealBeforeStartDoesNotCrash) {
  SendRevealEvent();
}

TEST_F(AppLifecycleDelegateTest, Stop) {
  // Expectations for OnStart (from SendStartEvent)
  EXPECT_CALL(*runner_, InitializeSystem());
  EXPECT_CALL(*runner_, CreateMainDelegate(true, _));
  EXPECT_CALL(*runner_, GetMainDelegate()).WillOnce(Return(nullptr));
  EXPECT_CALL(*runner_, Run(_)).WillOnce(Return(0));
  SendStartEvent(kSbEventTypeStart);

  // Expectations for IsRunning check at start of OnStop (from SendStopEvent)
  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(true));
  EXPECT_CALL(*runner_, ShutDown());
  SendStopEvent();

  // Expectation for final IsRunning check
  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(false));
  EXPECT_FALSE(delegate_->IsRunning());
}

}  // namespace cobalt
