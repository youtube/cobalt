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

#include "cobalt/testing/shell/content_browser_test_content_browser_client.h"

#include "base/test/task_environment.h"
#include "cobalt/shell/browser/shell_content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

ContentBrowserTestContentBrowserClient::
    ContentBrowserTestContentBrowserClient() {
  if (GetShellContentBrowserClientInstances().size() > 1) {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(this);
  }
}

ContentBrowserTestContentBrowserClient::
    ~ContentBrowserTestContentBrowserClient() {
  // ShellContentBrowserClient is responsible for removing `this` from
  // GetShellContentBrowserClientInstances(). Only set ContentClient's
  // variable when there is at least one more
  // ContentBrowserTestContentBrowserClient. This is necessary as the
  // last instance is owned by ContentClient and this function is called
  // during ContentClient's destruction.
  const size_t client_count = GetShellContentBrowserClientInstances().size();
  if (client_count > 1) {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(
        GetShellContentBrowserClientInstances()[client_count - 2]);
  }
}

bool ContentBrowserTestContentBrowserClient::CreateThreadPool(
    base::StringPiece name) {
  // Injects a test TaskTracker to watch for long-running tasks and produce a
  // useful timeout message in order to find the cause of flaky timeout tests.
  base::test::TaskEnvironment::CreateThreadPool();
  return true;
}

}  // namespace content
