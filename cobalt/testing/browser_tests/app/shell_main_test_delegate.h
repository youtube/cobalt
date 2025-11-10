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

#ifndef COBALT_TESTING_BROWSER_TESTS_APP_SHELL_MAIN_TEST_DELEGATE_H_
#define COBALT_TESTING_BROWSER_TESTS_APP_SHELL_MAIN_TEST_DELEGATE_H_

#include <memory>
#include <optional>

#include "cobalt/shell/app/shell_main_delegate.h"

namespace content {

class ShellContentUtilityClient;

class ShellMainTestDelegate : public ShellMainDelegate {
 public:
  explicit ShellMainTestDelegate();

  ShellMainTestDelegate(const ShellMainTestDelegate&) = delete;
  ShellMainTestDelegate& operator=(const ShellMainTestDelegate&) = delete;

  ~ShellMainTestDelegate() override;

  // ContentMainDelegate implementation:
  ContentClient* CreateContentClient() override;
  std::optional<int> BasicStartupComplete() override;
  ContentUtilityClient* CreateContentUtilityClient() override;

 private:
  std::unique_ptr<ShellContentUtilityClient> utility_client_;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_APP_SHELL_MAIN_TEST_DELEGATE_H_
