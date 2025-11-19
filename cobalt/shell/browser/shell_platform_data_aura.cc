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

#include "cobalt/shell/browser/shell_platform_data_aura.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/focus_controller.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/aura/screen_ozone.h"
#endif

namespace content {

namespace {

class FillLayout : public aura::LayoutManager {
 public:
  explicit FillLayout(aura::Window* root)
      : root_(root), has_bounds_(!root->bounds().IsEmpty()) {}

  FillLayout(const FillLayout&) = delete;
  FillLayout& operator=(const FillLayout&) = delete;

  ~FillLayout() override {}

 private:
  // aura::LayoutManager:
  void OnWindowResized() override {
    // If window bounds were not set previously then resize all children to
    // match the size of the parent.
    if (!has_bounds_) {
      has_bounds_ = true;
      for (aura::Window* child : root_->children()) {
        SetChildBoundsDirect(child, gfx::Rect(root_->bounds().size()));
      }
    }
  }

  void OnWindowAddedToLayout(aura::Window* child) override {
    child->SetBounds(root_->bounds());
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}

  void OnWindowRemovedFromLayout(aura::Window* child) override {}

  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}

  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  raw_ptr<aura::Window> root_;
  bool has_bounds_;
};

class FocusRules : public wm::BaseFocusRules {
 public:
  FocusRules() = default;
  FocusRules(const FocusRules&) = delete;
  FocusRules& operator=(const FocusRules&) = delete;
  ~FocusRules() override = default;

 private:
  // wm::BaseFocusRules:
  bool SupportsChildActivation(const aura::Window* window) const override {
    return true;
  }
};

}  // namespace

ShellPlatformDataAura::ShellPlatformDataAura(const gfx::Size& initial_size) {
  CHECK(aura::Env::GetInstance());

#if BUILDFLAG(IS_OZONE)
  // Setup global display::Screen singleton.
  screen_ = std::make_unique<aura::ScreenOzone>();
#endif  // BUILDFLAG(IS_OZONE)

  ui::PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(initial_size);

  host_ = aura::WindowTreeHost::Create(std::move(properties));
  host_->InitHost();
  host_->window()->Show();
  host_->window()->SetLayoutManager(
      std::make_unique<FillLayout>(host_->window()));

  // Reference void AuraContext::InitializeWindowTreeHost(aura::WindowTreeHost*
  // host) from ui/webui/examples/browser/ui/aura/aura_context.cc
  focus_client_ = std::make_unique<wm::FocusController>(new FocusRules());
  aura::client::SetFocusClient(host_->window(), focus_client_.get());

  capture_client_ =
      std::make_unique<aura::client::DefaultCaptureClient>(host_->window());
}

ShellPlatformDataAura::~ShellPlatformDataAura() {
  aura::client::SetCursorShapeClient(nullptr);
}

void ShellPlatformDataAura::ShowWindow() {
  host_->Show();
}

void ShellPlatformDataAura::ResizeWindow(const gfx::Size& size) {
  host_->SetBoundsInPixels(gfx::Rect(size));
}

}  // namespace content
