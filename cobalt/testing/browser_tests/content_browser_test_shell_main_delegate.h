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

#ifndef COBALT_TESTING_BROWSER_TESTS_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_
#define COBALT_TESTING_BROWSER_TESTS_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_

#include <memory>
#include <optional>

#include "build/build_config.h"
#include "cobalt/testing/browser_tests/app/shell_main_test_delegate.h"

#if BUILDFLAG(IS_STARBOARD)
namespace ui {
class PlatformEventSourceStarboard;
}
#endif  // BUILDFLAG(IS_STARBOARD)

namespace content {

// Acts like normal ShellMainDelegate but inserts behaviour for browser tests.
class ContentBrowserTestShellMainDelegate : public ShellMainTestDelegate {
 public:
  ContentBrowserTestShellMainDelegate();
  ~ContentBrowserTestShellMainDelegate() override;

  // ContentMainDelegate implementation:
  void CreateThreadPool(std::string_view name) override;

  // ShellMainDelegate overrides.
  content::ContentBrowserClient* CreateContentBrowserClient() override;

 private:
#if BUILDFLAG(IS_STARBOARD)
  std::unique_ptr<ui::PlatformEventSourceStarboard> platform_event_source_;
#endif  // BUILDFLAG(IS_STARBOARD)
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_CONTENT_BROWSER_TEST_SHELL_MAIN_DELEGATE_H_
