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

#include "base/containers/contains.h"
#include "base/logging.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_data_aura.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"

namespace content {

struct ShellPlatformDelegate::ShellData {
  gfx::NativeWindow window;
  gfx::Size initial_size_;
};

struct ShellPlatformDelegate::PlatformData {
  std::unique_ptr<ShellPlatformDataAura> aura;
  gfx::Size default_window_size;
};

ShellPlatformDelegate::ShellPlatformDelegate() = default;
ShellPlatformDelegate::~ShellPlatformDelegate() = default;

ShellPlatformDataAura* ShellPlatformDelegate::GetShellPlatformDataAura() {
  return platform_->aura.get();
}

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
  platform_ = std::make_unique<PlatformData>();
  platform_->default_window_size = default_window_size;
  platform_->aura = nullptr;
}

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];
  shell_data.initial_size_ = initial_size;

  if (IsConcealed()) {
    shell_data.window = nullptr;
  } else if (platform_->aura) {
    platform_->aura->ResizeWindow(initial_size);
    shell_data.window = platform_->aura->host()->window();
  } else {
    shell_data.window = nullptr;
  }
}

gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];
  return shell_data.window;
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  aura::Window* content = shell->web_contents()->GetNativeView();
  if (!platform_->aura) {
    return;
  }
  aura::Window* parent = platform_->aura->host()->window();
  if (parent && !parent->Contains(content)) {
    parent->AddChild(content);
  }

  content->Show();
}

void ShellPlatformDelegate::DoBlur() {
  // Empty.
}

void ShellPlatformDelegate::DoFocus() {
  // Empty.
}

void ShellPlatformDelegate::RevealShell(Shell* shell) {
  if (!platform_->aura) {
    platform_->aura =
        std::make_unique<ShellPlatformDataAura>(platform_->default_window_size);
  }

  ShellData& shell_data = shell_data_map_.at(shell);
  if (!shell_data.window) {
    shell_data.window = platform_->aura->host()->window();
    platform_->aura->ResizeWindow(shell_data.initial_size_);
    SetContents(shell);
  }

  platform_->aura->host()->Show();
}

void ShellPlatformDelegate::DoBlur() {
  // Empty.
}

void ShellPlatformDelegate::DoFocus() {
  // Empty.
}

void ShellPlatformDelegate::DoConceal() {
  if (platform_->aura) {
    platform_->aura.reset();
    std::vector<Shell*> windows = Shell::windows();
    for (auto* s : windows) {
      if (base::Contains(shell_data_map_, s)) {
        shell_data_map_.at(s).window = nullptr;
      }
    }
  }
}

void ShellPlatformDelegate::DoReveal() {
  std::vector<Shell*> windows = Shell::windows();
  for (auto* shell : windows) {
    RevealShell(shell);
  }
}

void ShellPlatformDelegate::DoFreeze() {
  // Empty.
}

void ShellPlatformDelegate::DoUnfreeze() {
  // Empty.
}

void ShellPlatformDelegate::DoStop() {
  // Empty.
}

void ShellPlatformDelegate::LoadSplashScreenContents(Shell* shell) {}

void ShellPlatformDelegate::UpdateContents(Shell* shell) {}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  shell->web_contents()->GetRenderWidgetHostView()->SetSize(content_size);
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const std::u16string& title) {}

void ShellPlatformDelegate::MainFrameCreated(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  return false;  // Shell destroys itself.
}

}  // namespace content
