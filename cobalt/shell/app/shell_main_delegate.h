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

#ifndef COBALT_SHELL_APP_SHELL_MAIN_DELEGATE_H_
#define COBALT_SHELL_APP_SHELL_MAIN_DELEGATE_H_

#include <memory>
#include <optional>
#include <variant>

#include "build/build_config.h"
#include "components/memory_system/memory_system.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class ShellContentClient;
class ShellContentBrowserClient;
class ShellContentRendererClient;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS_TVOS) && \
    !BUILDFLAG(IS_STARBOARD)
class WebTestBrowserMainRunner;
#endif

class ShellMainDelegate : public ContentMainDelegate {
 public:
  explicit ShellMainDelegate();

  ShellMainDelegate(const ShellMainDelegate&) = delete;
  ShellMainDelegate& operator=(const ShellMainDelegate&) = delete;

  ~ShellMainDelegate() override;

  // ContentMainDelegate implementation:
  std::optional<int> BasicStartupComplete() override;
  bool ShouldCreateFeatureList(InvokedIn invoked_in) override;
  bool ShouldInitializeMojo(InvokedIn invoked_in) override;
  void PreSandboxStartup() override;
  std::variant<int, MainFunctionParams> RunProcess(
      const std::string& process_type,
      MainFunctionParams main_function_params) override;
#if BUILDFLAG(IS_LINUX)
  void ZygoteForked() override;
#endif
  std::optional<int> PreBrowserMain() override;
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  ContentClient* CreateContentClient() override;
  ContentBrowserClient* CreateContentBrowserClient() override;
  ContentRendererClient* CreateContentRendererClient() override;

  static void InitializeResourceBundle();

 protected:
  // Only present when running content_browsertests, which run inside Content
  // Shell.
  //
  // content_browsertests should not set the kRunWebTests command line flag, so
  // |is_content_browsertests_| and |web_test_runner_| are mututally exclusive.
  bool is_content_browsertests_;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS_TVOS) && \
    !BUILDFLAG(IS_STARBOARD)
  // Only present when running web tests, which run inside Content Shell.
  //
  // Web tests are not browser tests, so |is_content_browsertests_| and
  // |web_test_runner_| are mututally exclusive.
  std::unique_ptr<WebTestBrowserMainRunner> web_test_runner_;
#endif

  std::unique_ptr<ShellContentBrowserClient> browser_client_;
  std::unique_ptr<ShellContentRendererClient> renderer_client_;
  std::unique_ptr<ShellContentClient> content_client_;

  memory_system::MemorySystem memory_system_;
};

}  // namespace content

#endif  // COBALT_SHELL_APP_SHELL_MAIN_DELEGATE_H_
