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

#include "cobalt/testing/browser_tests/browser/shell_platform_test_data_aura.h"

#include <memory>

#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_window_parenting_client.h"
#include "ui/wm/core/cursor_loader.h"
#include "ui/wm/core/default_activation_client.h"

namespace content {

ShellPlatformTestDataAura::ShellPlatformTestDataAura(
    const gfx::Size& initial_size)
    : ShellPlatformDataAura(initial_size, false) {
  focus_client_ =
      std::make_unique<aura::test::TestFocusClient>(host()->window());
  aura::client::SetFocusClient(host()->window(), focus_client_.get());

  new wm::DefaultActivationClient(host()->window());

  window_parenting_client_ =
      std::make_unique<aura::test::TestWindowParentingClient>(host()->window());
  // TODO(https://crbug.com/1336055): this is needed for
  // mouse_cursor_overlay_controller_browsertest.cc on cast_shell_linux as
  // currently, when is_castos = true, the views toolkit isn't used.
  cursor_shape_client_ = std::make_unique<wm::CursorLoader>();
  aura::client::SetCursorShapeClient(cursor_shape_client_.get());
}

ShellPlatformTestDataAura::~ShellPlatformTestDataAura() {
  aura::client::SetCursorShapeClient(nullptr);
  aura::client::SetWindowParentingClient(host()->window(), nullptr);
}

}  // namespace content
