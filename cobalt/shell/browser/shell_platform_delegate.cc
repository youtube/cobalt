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
#include "base/threading/hang_watcher.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "cobalt/browser/lifecycle/cobalt_lifecycle_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
#include "ui/aura/window_tree_host_platform.h"
#include "ui/ozone/platform/starboard/platform_window_starboard.h"
#endif

namespace content {

namespace {
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
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

class ShellPlatformDelegate::WebContentsTracker final
    : public content::WebContentsObserver {
 public:
  WebContentsTracker(content::WebContents* web_contents,
                     ShellPlatformDelegate* delegate)
      : content::WebContentsObserver(web_contents), delegate_(delegate) {}
  ~WebContentsTracker() override = default;

  void WebContentsDestroyed() override {
    delegate_->RemovePreviouslyVisibleWebContents(web_contents());
  }

 private:
  ShellPlatformDelegate* delegate_;
};

void ShellPlatformDelegate::WebContentsTrackerDeleter::operator()(
    WebContentsTracker* ptr) const {
  delete ptr;
}

void ShellPlatformDelegate::TrackPreviouslyVisibleWebContents(
    content::WebContents* web_contents) {
  previously_visible_web_contents_[web_contents] =
      std::unique_ptr<WebContentsTracker, WebContentsTrackerDeleter>(
          new WebContentsTracker(web_contents, this));
}

void ShellPlatformDelegate::RemovePreviouslyVisibleWebContents(
    content::WebContents* web_contents) {
  previously_visible_web_contents_.erase(web_contents);
}

void ShellPlatformDelegate::AddPreviouslyVisibleWebContentsForTesting(
    content::WebContents* web_contents) {
  TrackPreviouslyVisibleWebContents(web_contents);
}

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
  if (IsWaitingForRevealAck()) {
    deferred_focus_ = true;
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

  // Register as lifecycle manager observer to receive OnAllFramesConcealed
  // callback!
  cobalt::CobaltLifecycleManager::GetInstance()->AddObserver(
      static_cast<cobalt::CobaltLifecycleManagerObserver*>(this));

  // Save the set of WebContents that were visible before conceal.
  // This is used on reveal to decide which WebContents we should wait for
  // Reveal ACK from. We only wait for those that were actually active/visible.
  previously_visible_web_contents_.clear();
  for (auto* shell : Shell::windows()) {
    if (shell->web_contents()->GetVisibility() ==
        content::Visibility::VISIBLE) {
      TrackPreviouslyVisibleWebContents(shell->web_contents());
    }
    // Trigger logical JS conceal.
    shell->web_contents()->WasHidden();
    // Note: ConcealShell() is called asynchronously inside
    // OnAllFramesConcealed() after all frames have completed their deactivation
    // ACKs.
  }
}

void ShellPlatformDelegate::OnReveal() {
  if (IsVisible()) {
    return;
  }
  // Used to ensure we only register as observer once, even if there are
  // multiple windows to wait for.
  bool started_waiting = false;
  for (auto* shell : Shell::windows()) {
    if (previously_visible_web_contents_.count(shell->web_contents())) {
      if (!started_waiting) {
        waiting_for_reveal_ack_ = true;
        cobalt::CobaltLifecycleManager::GetInstance()->AddObserver(
            static_cast<cobalt::CobaltLifecycleManagerObserver*>(this));
        started_waiting = true;
      }
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
      auto* platform_window = GetPlatformWindowStarboard(shell);
      if (platform_window) {
        platform_window->SetWaitingForRevealAck(true);
      }
#endif
    }
    RevealShell(shell);
  }
  is_visible_ = true;
}

void ShellPlatformDelegate::OnFreeze() {
  CHECK(!IsVisible());

  // In frozen state the process may not be scheduled for execution, so we
  // tell the hang watcher to ignore this period of inactivity.
  base::HangWatcher::Suspend();
  for (auto* shell : Shell::windows()) {
    shell->web_contents()->SetPageFrozen(true);
  }
}

void ShellPlatformDelegate::OnUnfreeze() {
  CHECK(!IsVisible());

  // Resume the hangwatcher.
  base::HangWatcher::Resume();
  for (auto* shell : Shell::windows()) {
    shell->web_contents()->SetPageFrozen(false);
  }
}

void ShellPlatformDelegate::OnStop() {}

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
  waiting_for_reveal_ack_ = false;
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
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

bool ShellPlatformDelegate::IsWaitingForRevealAck() const {
  bool new_is_waiting = false;
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
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
#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
  auto* shell = Shell::windows().empty() ? nullptr : Shell::windows().front();
  if (shell) {
    auto* platform_window = GetPlatformWindowStarboard(shell);
    if (platform_window) {
      platform_window->SetWaitingForRevealAck(false);
    }
  }
#endif
}

#if defined(USE_AURA) && BUILDFLAG(IS_STARBOARD)
void ShellPlatformDelegate::OnProactiveMapWindow(
    content::WebContents* web_contents) {
  Shell* shell = Shell::FromWebContents(web_contents);
  if (shell) {
    SetContents(shell);
    MapWindowShell(shell);
  }
}
#endif

void ShellPlatformDelegate::OnAllFramesVisible(
    content::WebContents* web_contents) {
  // Called by CobaltLifecycleManager when all frames in the specified
  // WebContents have completed layout and are visible. This breaks the wait
  // initiated in OnReveal.
  ClearWaitingForRevealAck();
  is_visible_ = true;

  // If an OS focus event arrived while we were waiting, apply it now that
  // the page is ready.
  if (deferred_focus_) {
    for (auto* w : Shell::windows()) {
      w->Focus();
    }
    deferred_focus_ = false;
  }

  // Stop observing as we only need one notification per reveal.
  cobalt::CobaltLifecycleManager::GetInstance()->RemoveObserver(
      static_cast<cobalt::CobaltLifecycleManagerObserver*>(this));
}

void ShellPlatformDelegate::OnAllFramesConcealed(
    content::WebContents* web_contents) {
  Shell* shell = Shell::FromWebContents(web_contents);
  if (shell) {
    ConcealShell(shell);
  }
  is_visible_ = false;

  // Stop observing as we only need one notification per conceal.
  cobalt::CobaltLifecycleManager::GetInstance()->RemoveObserver(
      static_cast<cobalt::CobaltLifecycleManagerObserver*>(this));
}

#if !defined(USE_AURA) || !BUILDFLAG(IS_STARBOARD)
void ShellPlatformDelegate::DidCreateOrAttachWebContents(
    Shell* shell,
    WebContents* web_contents) {}
#endif

}  // namespace content
