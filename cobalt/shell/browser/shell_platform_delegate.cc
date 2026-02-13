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
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents.h"

namespace content {

void ShellPlatformDelegate::UpdateWebContentsVisibility(bool visible) {
  for (auto* shell : Shell::windows()) {
    if (visible) {
      shell->web_contents()->WasShown();
    } else {
      shell->web_contents()->WasHidden();
    }
  }
}

void ShellPlatformDelegate::UpdateWebContentsFrozen(bool frozen) {
  for (auto* shell : Shell::windows()) {
    shell->web_contents()->SetPageFrozen(frozen);
  }
}

bool ShellPlatformDelegate::IsVisible() const {
  return application_state_ == kApplicationStateStarted ||
         application_state_ == kApplicationStateBlurred;
}

bool ShellPlatformDelegate::IsConcealed() const {
  return application_state_ >= kApplicationStateConcealed;
}

void ShellPlatformDelegate::TransitionToState(ApplicationState target_state) {
  while (application_state_ != target_state) {
    ApplicationState current = application_state_;

    if (current < target_state) {
      // Moving "down" the lifecycle (e.g. Started -> Blurred -> Concealed ->
      // Frozen -> Stopped)
      switch (current) {
        case kApplicationStateStarted:
          application_state_ = kApplicationStateBlurred;
          DoBlur();
          break;
        case kApplicationStateBlurred:
          UpdateWebContentsVisibility(false);
          application_state_ = kApplicationStateConcealed;
          DoConceal();
          break;
        case kApplicationStateConcealed:
          UpdateWebContentsFrozen(true);
          application_state_ = kApplicationStateFrozen;
          DoFreeze();
          break;
        case kApplicationStateFrozen:
          application_state_ = kApplicationStateStopped;
          DoStop();
          break;
        default:
          return;
      }
    } else {
      // Moving "up" the lifecycle (e.g. Frozen -> Concealed -> Blurred ->
      // Started)
      switch (current) {
        case kApplicationStateFrozen:
          UpdateWebContentsFrozen(false);
          application_state_ = kApplicationStateConcealed;
          DoUnfreeze();
          break;
        case kApplicationStateConcealed:
          application_state_ = kApplicationStateBlurred;
          DoReveal();
          break;
        case kApplicationStateBlurred:
          UpdateWebContentsVisibility(true);
          application_state_ = kApplicationStateStarted;
          DoFocus();
          break;
        default:
          return;
      }
    }

    if (application_state_ == current) {
      NOTREACHED() << "State transition failed to progress from " << current;
      break;
    }
  }
}

void ShellPlatformDelegate::OnBlur() {
  TransitionToState(kApplicationStateBlurred);
}

void ShellPlatformDelegate::OnFocus() {
  TransitionToState(kApplicationStateStarted);
}

void ShellPlatformDelegate::OnConceal() {
  TransitionToState(kApplicationStateConcealed);
}

void ShellPlatformDelegate::OnReveal() {
  TransitionToState(kApplicationStateBlurred);
}

void ShellPlatformDelegate::OnFreeze() {
  TransitionToState(kApplicationStateFrozen);
}

void ShellPlatformDelegate::OnUnfreeze() {
  TransitionToState(kApplicationStateConcealed);
}

void ShellPlatformDelegate::OnStop() {
  TransitionToState(kApplicationStateStopped);
}

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

bool ShellPlatformDelegate::HandleRequestToLockMouse(
    Shell* shell,
    WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  return false;
}

bool ShellPlatformDelegate::ShouldAllowRunningInsecureContent(Shell* shell) {
  return false;
}

#if !BUILDFLAG(IS_IOS)
void ShellPlatformDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    scoped_refptr<FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  listener->FileSelectionCanceled();
}
#endif

}  // namespace content
