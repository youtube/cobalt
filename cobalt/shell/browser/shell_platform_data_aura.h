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

#ifndef COBALT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_
#define COBALT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_

#include <memory>

#include "build/build_config.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace client {
class FocusClient;
class DefaultCaptureClient;
}  // namespace client
}  // namespace aura

#if BUILDFLAG(IS_OZONE)
namespace aura {
class ScreenOzone;
}
#endif

namespace gfx {
class Size;
}

namespace content {

class ShellPlatformDataAura {
 public:
  explicit ShellPlatformDataAura(const gfx::Size& initial_size);

  ShellPlatformDataAura(const ShellPlatformDataAura&) = delete;
  ShellPlatformDataAura& operator=(const ShellPlatformDataAura&) = delete;

  ~ShellPlatformDataAura();

  void ShowWindow();
  void ResizeWindow(const gfx::Size& size);

  aura::WindowTreeHost* host() { return host_.get(); }

 private:
#if BUILDFLAG(IS_OZONE)
  std::unique_ptr<aura::ScreenOzone> screen_;
#endif

  std::unique_ptr<aura::WindowTreeHost> host_;
  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;

 protected:
  std::unique_ptr<aura::client::FocusClient> focus_client_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_PLATFORM_DATA_AURA_H_
