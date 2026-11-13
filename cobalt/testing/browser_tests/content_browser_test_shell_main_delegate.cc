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

#include <optional>
#include <variant>

#include "base/test/task_environment.h"
#include "cobalt/common/features/starboard_features_initialization.h"
#include "cobalt/testing/browser_tests/browser/shell_content_browser_test_client.h"
#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"

namespace content {

ContentBrowserTestShellMainDelegate::ContentBrowserTestShellMainDelegate()
    : ShellMainTestDelegate() {}

ContentBrowserTestShellMainDelegate::~ContentBrowserTestShellMainDelegate() =
    default;

std::optional<int> ContentBrowserTestShellMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  std::optional<int> result =
      ShellMainTestDelegate::PostEarlyInitialization(invoked_in);
  if (result.has_value()) {
    return result;
  }
  if (std::holds_alternative<InvokedInBrowserProcess>(invoked_in)) {
    // Generic ContentBrowserTest delegates do not execute
    // CobaltContentBrowserClient::CreateFeatureListAndFieldTrials(). Push the
    // initialized base::FeatureList state down to Starboard once per process
    // (for per-test overrides in reused processes, use
    // starboard::features::ScopedFeatureList).
    static bool starboard_features_initialized = false;
    if (!starboard_features_initialized) {
      starboard_features_initialized = true;
      cobalt::features::InitializeStarboardFeatures();
    }
  }
  return std::nullopt;
}

void ContentBrowserTestShellMainDelegate::CreateThreadPool(
    std::string_view name) {
  // Injects a test TaskTracker to watch for long-running tasks and produce a
  // useful timeout message in order to find the cause of flaky timeout tests.
  base::test::TaskEnvironment::CreateThreadPool();
}

ContentBrowserClient*
ContentBrowserTestShellMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<ContentBrowserTestContentBrowserClient>();
  return browser_client_.get();
}

}  // namespace content
