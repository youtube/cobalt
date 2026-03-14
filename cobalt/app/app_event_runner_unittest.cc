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

#include "cobalt/app/app_event_runner.h"

#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/public/browser/visibility.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace cobalt {

class AppEventRunnerTest : public content::CobaltShellTestBase {
 public:
  AppEventRunnerTest() { runner_ = AppEventRunner::Create(); }

 protected:
  content::Shell* CreateTestShell(bool is_visible) {
    InitializeShell(is_visible);
    content::WebContents::CreateParams create_params(browser_context());
    create_params.desired_renderer_state =
        content::WebContents::CreateParams::kNoRendererProcess;
    create_params.initially_hidden = !is_visible;
    std::unique_ptr<content::WebContents> web_contents(
        content::TestWebContents::Create(create_params));

    if (is_visible) {
      EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
    }
    EXPECT_CALL(*platform_, SetContents(_));

    content::Shell* shell = new content::Shell(
        std::move(web_contents), nullptr /* splash_contents */,
        /*should_set_delegate=*/true, /*topic=*/"",
        /*skip_for_testing=*/true);

    if (is_visible) {
      platform_->CreatePlatformWindow(shell, gfx::Size(1920, 1080));
    }

    content::Shell::FinishShellInitialization(shell);

    return shell;
  }

  std::unique_ptr<AppEventRunner> runner_;
};

TEST_F(AppEventRunnerTest, OnBlur) {
  content::Shell* shell = CreateTestShell(true /* is_visible */);
  runner_->SetStateForTesting(true /* is_running */, true /* is_visible */);

  EXPECT_CALL(*platform_, OnBlur());
  runner_->OnBlur();

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(AppEventRunnerTest, OnFocus) {
  content::Shell* shell = CreateTestShell(true /* is_visible */);
  runner_->SetStateForTesting(true /* is_running */, true /* is_visible */);

  EXPECT_CALL(*platform_, OnFocus());
  runner_->OnFocus();

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(AppEventRunnerTest, OnConceal) {
  content::Shell* shell = CreateTestShell(true /* is_visible */);
  runner_->SetStateForTesting(true /* is_running */, true /* is_visible */);

  EXPECT_CALL(*platform_, OnConceal());
  EXPECT_CALL(*platform_, ConcealShell(shell));
  runner_->OnConceal();

  EXPECT_FALSE(runner_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(),
            content::Visibility::HIDDEN);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(AppEventRunnerTest, OnReveal) {
  content::Shell* shell = CreateTestShell(false /* is_visible */);
  runner_->SetStateForTesting(true /* is_running */, false /* is_visible */);

  EXPECT_CALL(*platform_, OnReveal());
  EXPECT_CALL(*platform_, RevealShell(shell));
  runner_->OnReveal();

  EXPECT_TRUE(runner_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(),
            content::Visibility::VISIBLE);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(AppEventRunnerTest, OnFreeze) {
  content::Shell* shell = CreateTestShell(false /* is_visible */);
  runner_->SetStateForTesting(true /* is_running */, false /* is_visible */);
  content::TestWebContents* test_web_contents =
      static_cast<content::TestWebContents*>(shell->web_contents());

  EXPECT_CALL(*platform_, OnFreeze());
  runner_->OnFreeze();
  EXPECT_TRUE(test_web_contents->IsPageFrozen());

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(AppEventRunnerTest, OnUnfreeze) {
  content::Shell* shell = CreateTestShell(false /* is_visible */);
  runner_->SetStateForTesting(true /* is_running */, false /* is_visible */);
  content::TestWebContents* test_web_contents =
      static_cast<content::TestWebContents*>(shell->web_contents());

  EXPECT_CALL(*platform_, OnUnfreeze());
  runner_->OnUnfreeze();
  EXPECT_FALSE(test_web_contents->IsPageFrozen());

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

}  // namespace cobalt
