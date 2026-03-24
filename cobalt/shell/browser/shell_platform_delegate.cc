// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/shell_platform_delegate.h"

#include "cobalt/shell/browser/shell.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace content {

bool ShellPlatformDelegate::IsVisible() const {
  return is_visible_;
}

void ShellPlatformDelegate::OnBlur() {
  CHECK(IsVisible());
  for (auto* shell : Shell::windows()) {
    if (shell->web_contents()) {
      shell->web_contents()
          ->GetPrimaryMainFrame()
          ->GetRenderWidgetHost()
          ->Blur();
    }
  }
}

void ShellPlatformDelegate::OnFocus() {
  CHECK(IsVisible());
  for (auto* shell : Shell::windows()) {
    if (shell->web_contents()) {
      shell->web_contents()->Focus();
    }
  }
}

void ShellPlatformDelegate::OnConceal() {
  CHECK(IsVisible());
  set_is_visible(false);
  for (auto* shell : Shell::windows()) {
    ConcealShell(shell);
    shell->web_contents()->WasHidden();
  }
}

void ShellPlatformDelegate::OnReveal() {
  CHECK(!IsVisible());
  set_is_visible(true);
  for (auto* shell : Shell::windows()) {
    RevealShell(shell);
    shell->web_contents()->WasShown();
  }
}

void ShellPlatformDelegate::OnFreeze() {
  CHECK(!IsVisible());
  for (auto* shell : Shell::windows()) {
    shell->web_contents()->SetPageFrozen(true);
  }
}

void ShellPlatformDelegate::OnUnfreeze() {
  CHECK(!IsVisible());
  for (auto* shell : Shell::windows()) {
    shell->web_contents()->SetPageFrozen(false);
  }
}

void ShellPlatformDelegate::OnStop() {}

void ShellPlatformDelegate::DidCreateOrAttachWebContents(
    Shell* shell,
    WebContents* web_contents) {}

void ShellPlatformDelegate::DidCloseLastWindow() {
  Shell::Shutdown();
}

std::unique_ptr<JavaScriptDialogManager>
ShellPlatformDelegate::CreateJavaScriptDialogManager(Shell* shell) {
  return nullptr;
}

bool ShellPlatformDelegate::HandlePointerLockRequest(
    Shell* shell,
    WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  return false;
}

bool ShellPlatformDelegate::ShouldAllowRunningInsecureContent(Shell* shell) {
  return false;
}

}  // namespace content
