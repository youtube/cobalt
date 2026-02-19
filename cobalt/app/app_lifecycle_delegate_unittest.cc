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
#include "base/test/mock_callback.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace cobalt {

class AppLifecycleDelegateTest : public content::ShellTestBase {
 public:
  AppLifecycleDelegateTest() {
    AppLifecycleDelegate::Callbacks callbacks;
    callbacks.on_start = mock_on_start_.Get();
    callbacks.on_reveal = base::BindRepeating(&content::Shell::OnReveal);
    callbacks.on_stop = mock_on_stop_.Get();
    delegate_ = std::make_unique<AppLifecycleDelegate>(
        std::move(callbacks), true /* skip_init_for_testing */);
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

  base::MockCallback<base::RepeatingCallback<
      void(bool is_visible, int argc, const char** argv, const char* link)>>
      mock_on_start_;
  base::MockCallback<base::RepeatingClosure> mock_on_reveal_;
  base::MockCallback<base::RepeatingClosure> mock_on_stop_;

  std::unique_ptr<AppLifecycleDelegate> delegate_;
};

TEST_F(AppLifecycleDelegateTest, StartVisible) {
  EXPECT_CALL(mock_on_start_, Run(true, _, _, _));
  SendStartEvent(kSbEventTypeStart);
}

TEST_F(AppLifecycleDelegateTest, StartPreloaded) {
  EXPECT_CALL(mock_on_start_, Run(false, _, _, _));
  SendStartEvent(kSbEventTypePreload);
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
  EXPECT_CALL(mock_on_stop_, Run());
  SendStopEvent();
}

}  // namespace cobalt
