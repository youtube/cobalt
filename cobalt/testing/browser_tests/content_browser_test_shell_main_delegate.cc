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

#include "base/no_destructor.h"
#include "base/test/task_environment.h"
#include "cobalt/testing/browser_tests/browser/shell_content_browser_test_client.h"
#include <string>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/test/task_environment.h"
#include "cobalt/testing/browser_tests/browser/shell_content_browser_test_client.h"
#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"
#include "components/memory_system/initializer.h"
#include "components/memory_system/parameters.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"

namespace content {

ContentBrowserTestShellMainDelegate::ContentBrowserTestShellMainDelegate()
    : ShellMainTestDelegate() {}

ContentBrowserTestShellMainDelegate::~ContentBrowserTestShellMainDelegate() =
    default;

std::optional<int> ContentBrowserTestShellMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (!ShouldCreateFeatureList(invoked_in)) {
    static_cast<ShellContentBrowserClient*>(
        GetContentClientForTesting()->browser())
        ->CreateFeatureListAndFieldTrials();
  }
  if (!ShouldInitializeMojo(invoked_in)) {
    InitializeMojoCore();
  }

  const std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  // ShellMainDelegate has GWP-ASan as well as Profiling Client disabled.
  // Consequently, we provide no parameters for these two. The memory_system
  // includes the PoissonAllocationSampler dynamically only if the Profiling
  // Client is enabled. However, we are not sure if this is the only user of
  // PoissonAllocationSampler in the ContentShell. Therefore, enforce inclusion
  // at the moment.
  //
  // TODO(crbug.com/40062835): Clarify which users of
  // PoissonAllocationSampler we have in the ContentShell. Do we really need to
  // enforce it?
  memory_system::Initializer()
      .SetDispatcherParameters(memory_system::DispatcherParameters::
                                   PoissonAllocationSamplerInclusion::kEnforce,
                               memory_system::DispatcherParameters::
                                   AllocationTraceRecorderInclusion::kIgnore,
                               process_type)
      .Initialize(memory_system_);

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
  static base::NoDestructor<ContentBrowserTestContentBrowserClient> browser_client;
  return browser_client.get();
}

}  // namespace content
