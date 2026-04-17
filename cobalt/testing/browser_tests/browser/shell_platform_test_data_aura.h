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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_PLATFORM_TEST_DATA_AURA_H_
#define COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_PLATFORM_TEST_DATA_AURA_H_

#include <memory>

#include "cobalt/shell/browser/shell_platform_data_aura.h"

namespace aura {
namespace client {
class CursorShapeClient;
class WindowParentingClient;
}  // namespace client
}  // namespace aura

namespace content {

class ShellPlatformTestDataAura : public ShellPlatformDataAura {
 public:
  explicit ShellPlatformTestDataAura(const gfx::Size& initial_size);
  ShellPlatformTestDataAura(const ShellPlatformTestDataAura&) = delete;
  ShellPlatformTestDataAura& operator=(const ShellPlatformTestDataAura&) =
      delete;
  ~ShellPlatformTestDataAura() override;

 private:
  std::unique_ptr<aura::client::WindowParentingClient> window_parenting_client_;
  std::unique_ptr<aura::client::CursorShapeClient> cursor_shape_client_;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_PLATFORM_TEST_DATA_AURA_H_
