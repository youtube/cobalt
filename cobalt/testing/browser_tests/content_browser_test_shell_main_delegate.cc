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

#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"

#include "base/test/task_environment.h"
#include "cobalt/shell/browser/shell_content_browser_client.h"
#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"

namespace content {

ContentBrowserTestShellMainDelegate::ContentBrowserTestShellMainDelegate()
    : ShellMainDelegate(/*is_content_browsertests=*/true) {}

ContentBrowserTestShellMainDelegate::~ContentBrowserTestShellMainDelegate() =
    default;

absl::optional<int> ContentBrowserTestShellMainDelegate::PreBrowserMain() {
  absl::optional<int> exit_code = ShellMainDelegate::PreBrowserMain();
  if (exit_code.has_value()) {
    return exit_code;
  }
  // TODO(b/433354983): Support more platforms.
  ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());
  return absl::nullopt;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
absl::optional<int>
ContentBrowserTestShellMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (absl::holds_alternative<InvokedInBrowserProcess>(invoked_in)) {
    // Browser tests on Lacros requires a non-null LacrosService.
    lacros_service_ = std::make_unique<chromeos::LacrosService>();
  }
  ShellMainDelegate::PostEarlyInitialization(invoked_in);
  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

ContentBrowserClient*
ContentBrowserTestShellMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<ContentBrowserTestContentBrowserClient>();
  return browser_client_.get();
}

}  // namespace content
