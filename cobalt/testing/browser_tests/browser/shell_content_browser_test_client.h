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

#ifndef COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_CONTENT_BROWSER_TEST_CLIENT_H_
#define COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_CONTENT_BROWSER_TEST_CLIENT_H_

#include "cobalt/shell/browser/shell_content_browser_client.h"

namespace content {

class ShellContentBrowserTestClient : public ShellContentBrowserClient {
 public:
  ShellContentBrowserTestClient();
  ~ShellContentBrowserTestClient() override;

  // ShellContentBrowserClient overrides.
  void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe) override;
  bool HasCustomSchemeHandler(content::BrowserContext* browser_context,
                              const std::string& scheme) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      BrowserContext* browser_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  void CreateFeatureListAndFieldTrials() override;

 private:
  void SetUpFieldTrials();
};

// The delay for sending reports when running with --run-web-tests
constexpr base::TimeDelta kReportingDeliveryIntervalTimeForWebTests =
    base::Milliseconds(100);

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_CONTENT_BROWSER_TEST_CLIENT_H_
