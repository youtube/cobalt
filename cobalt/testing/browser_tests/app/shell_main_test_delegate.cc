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

#include "cobalt/testing/browser_tests/app/shell_main_test_delegate.h"

#include <iostream>

#include "base/command_line.h"
#include "cobalt/shell/common/shell_test_switches.h"
#include "cobalt/testing/browser_tests/common/shell_content_test_client.h"
#include "cobalt/testing/browser_tests/utility/shell_content_utility_client.h"

namespace content {

ShellMainTestDelegate::ShellMainTestDelegate() : ShellMainDelegate() {}

ShellMainTestDelegate::~ShellMainTestDelegate() {}

std::optional<int> ShellMainTestDelegate::BasicStartupComplete() {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch("run-layout-test")) {
    std::cerr << std::string(79, '*') << "\n"
              << "* The flag --run-layout-test is obsolete. Please use --"
              << switches::kRunWebTests << " instead. *\n"
              << std::string(79, '*') << "\n";
    command_line.AppendSwitch(switches::kRunWebTests);
  }
  return ShellMainDelegate::BasicStartupComplete();
}

ContentClient* ShellMainTestDelegate::CreateContentClient() {
  content_client_ = std::make_unique<ShellContentTestClient>();
  return content_client_.get();
}

ContentUtilityClient* ShellMainTestDelegate::CreateContentUtilityClient() {
  utility_client_ = std::make_unique<ShellContentUtilityClient>();
  return utility_client_.get();
}

}  // namespace content
