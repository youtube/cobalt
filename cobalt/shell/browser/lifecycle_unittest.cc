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

  Shell* CreateTestShell(bool is_visible) {
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

    Shell* shell =
        new Shell(std::move(web_contents), nullptr /* splash_contents */,
                  /*should_set_delegate=*/true, /*topic=*/"",
                  /*skip_for_testing=*/true);

    if (is_visible) {
      platform_->CreatePlatformWindow(shell, gfx::Size(1920, 1080));
    }

    Shell::FinishShellInitialization(shell);

    return shell;
  }
};

TEST_F(LifecycleTest, StartupVisible) {
  Shell* shell = CreateTestShell(true /* is_visible */);
  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(), Visibility::VISIBLE);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(LifecycleTest, StartupHidden) {
  Shell* shell = CreateTestShell(false /* is_visible */);
  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(), Visibility::HIDDEN);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(LifecycleTest, Reveal) {
  Shell* shell = CreateTestShell(false /* is_visible */);
  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(), Visibility::HIDDEN);

  // Trigger reveal.
  EXPECT_CALL(*platform_, OnReveal());
  EXPECT_CALL(*platform_, RevealShell(shell));
  Shell::OnReveal();

  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(), Visibility::VISIBLE);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(LifecycleTest, Conceal) {
  Shell* shell = CreateTestShell(true /* is_visible */);
  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(), Visibility::VISIBLE);

  // Trigger conceal.
  EXPECT_CALL(*platform_, OnConceal());
  EXPECT_CALL(*platform_, ConcealShell(shell));
  Shell::OnConceal();

  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell->web_contents()->GetVisibility(), Visibility::HIDDEN);

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(LifecycleTest, FreezeUnfreeze) {
  Shell* shell = CreateTestShell(false /* is_visible */);
  TestWebContents* test_web_contents =
      static_cast<TestWebContents*>(shell->web_contents());

  // Trigger freeze.
  EXPECT_CALL(*platform_, OnFreeze());
  Shell::OnFreeze();
  EXPECT_TRUE(test_web_contents->IsPageFrozen());

  // Trigger unfreeze.
  EXPECT_CALL(*platform_, OnUnfreeze());
  Shell::OnUnfreeze();
  EXPECT_FALSE(test_web_contents->IsPageFrozen());

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

TEST_F(LifecycleTest, BlurFocus) {
  Shell* shell = CreateTestShell(true /* is_visible */);

  // Trigger blur.
  EXPECT_CALL(*platform_, OnBlur());
  Shell::OnBlur();

  // Trigger focus.
  EXPECT_CALL(*platform_, OnFocus());
  Shell::OnFocus();

  EXPECT_CALL(*platform_, DestroyShell(shell));
  EXPECT_CALL(*platform_, CleanUp(shell));
  shell->Close();
  task_environment()->RunUntilIdle();
}

}  // namespace content
