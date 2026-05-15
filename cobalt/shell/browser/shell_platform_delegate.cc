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

#include "base/logging.h"
#include "base/notreached.h"
#include "cobalt/shell/browser/shell.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window.h"
#if defined(USE_AURA)
#include "ui/ozone/platform/starboard/platform_window_starboard.h"
#endif

namespace content {

namespace {
#if defined(USE_AURA)
ui::PlatformWindowStarboard* GetPlatformWindowStarboard(Shell* shell) {
  auto* native_view = shell->web_contents()->GetNativeView();
  if (!native_view) {
    return nullptr;
  }
  auto* root_window = native_view->GetRootWindow();
  if (!root_window) {
    return nullptr;
  }
  auto* host = root_window->GetHost();
  if (!host) {
    return nullptr;
  }
  auto* host_platform = static_cast<aura::WindowTreeHostPlatform*>(host);
  return static_cast<ui::PlatformWindowStarboard*>(
      host_platform->platform_window());
}
#endif
}  // namespace

bool ShellPlatformDelegate::IsVisible() const {
  return is_visible_;
}

void ShellPlatformDelegate::OnBlur() {
  if (!IsVisible()) {
    return;
  }
  for (auto* shell : Shell::windows()) {
    if (shell->web_contents()) {
      auto* rwh =
          shell->web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
      if (rwh) {
        rwh->Blur();
      }
    }
  }
}

void ShellPlatformDelegate::OnFocus() {
  if (!IsVisible()) {
    return;
  }
  for (auto* shell : Shell::windows()) {
    shell->Focus();
  }
}

void ShellPlatformDelegate::OnConceal() {
  if (!IsVisible()) {
    return;
  }
  for (auto* shell : Shell::windows()) {
    shell->web_contents()->WasHidden();
    ConcealShell(shell);
  }
  is_visible_ = false;
}

void ShellPlatformDelegate::OnReveal() {
  if (IsVisible()) {
    return;
  }
  waiting_for_reveal_ack_ = true;
  for (auto* shell : Shell::windows()) {
    auto* native_view = shell->web_contents()->GetNativeView();
    if (native_view) {
      if (native_view->GetRootWindow()) {
        auto* host = native_view->GetRootWindow()->GetHost();
        if (host) {
          auto* host_platform =
              static_cast<aura::WindowTreeHostPlatform*>(host);
#if defined(USE_AURA)
          auto* platform_window = static_cast<ui::PlatformWindowStarboard*>(
              host_platform->platform_window());
          if (platform_window) {
            platform_window->SetWaitingForRevealAck(true);
          }
#endif
        }
      }
    }
    RevealShell(shell);
  }
  is_visible_ = true;
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

void ShellPlatformDelegate::OnPageVisibilityVisible(Shell* shell) {
  if (IsWaitingForRevealAck()) {
    waiting_for_reveal_ack_ = false;
#if defined(USE_AURA)
    auto* platform_window = GetPlatformWindowStarboard(shell);
    if (platform_window) {
      platform_window->SetWaitingForRevealAck(false);
    }
    auto* native_view = shell->web_contents()->GetNativeView();
    if (native_view) {
      if (native_view->GetRootWindow()) {
        native_view->GetRootWindow()->Show();
      }
      native_view->Show();
    }
#endif
    shell->web_contents()->WasShown();
  }
}

bool ShellPlatformDelegate::IsWaitingForRevealAck() const {
  bool new_is_waiting = false;
#if defined(USE_AURA)
  auto* shell = Shell::windows().empty() ? nullptr : Shell::windows().front();
  if (shell) {
    auto* platform_window = GetPlatformWindowStarboard(shell);
    if (platform_window) {
      new_is_waiting = platform_window->IsWaitingForRevealAck();
    }
  }
#endif
  // We check both the internal flag and the platform window state.
  // The platform window needs to track this state to block focus events
  // synchronously. However, during early resume stages, the platform window
  // might not be accessible, so we must also maintain the state internally
  // in ShellPlatformDelegate to avoid losing it.
  // We cannot simply assume it is true when the platform window is
  // inaccessible, because that is also the case during initial startup, where
  // we are NOT waiting for a reveal ACK and must not block focus.
  // Furthermore, we need the local flag to override the platform window state
  // if it holds a stale 'false' value (e.g., if it was non-null but not yet
  // updated when queried).
  return waiting_for_reveal_ack_ || new_is_waiting;
}

void ShellPlatformDelegate::ClearWaitingForRevealAck() {
  waiting_for_reveal_ack_ = false;
#if defined(USE_AURA)
  auto* shell = Shell::windows().empty() ? nullptr : Shell::windows().front();
  if (shell) {
    auto* platform_window = GetPlatformWindowStarboard(shell);
    if (platform_window) {
      platform_window->SetWaitingForRevealAck(false);
    }
  }
#endif
}
}  // namespace content
