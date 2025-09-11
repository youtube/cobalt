// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_APP_COBALT_MAIN_DELEGATE_H_
#define COBALT_APP_COBALT_MAIN_DELEGATE_H_

#include "build/build_config.h"
#include "cobalt/browser/cobalt_content_browser_client.h"
#include "cobalt/gpu/cobalt_content_gpu_client.h"
#include "cobalt/renderer/cobalt_content_renderer_client.h"
#include "cobalt/shell/app/shell_main_delegate.h"
#include "cobalt/utility/cobalt_content_utility_client.h"
#include "content/public/browser/browser_main_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cobalt {

class CobaltMainDelegate : public content::ShellMainDelegate {
 public:
  explicit CobaltMainDelegate(bool is_content_browsertests = false);

  CobaltMainDelegate(const CobaltMainDelegate&) = delete;
  CobaltMainDelegate& operator=(const CobaltMainDelegate&) = delete;

  // ContentMainDelegate implementation:
  absl::optional<int> BasicStartupComplete() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;
  absl::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;

  // Override the RunProcess method to store the  reference to
  // BrowserMainRunner instead of leaking it. The reference would
  // be used for proper shutdown and cleanup.
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;

  // Shutdown method that trigger the BrowserMainRunner shutdown.
  void Shutdown();

  ~CobaltMainDelegate() override;

 private:
  std::unique_ptr<content::BrowserMainRunner> main_runner_;
  std::unique_ptr<CobaltContentBrowserClient> browser_client_;
  std::unique_ptr<CobaltContentGpuClient> gpu_client_;
  std::unique_ptr<CobaltContentRendererClient> renderer_client_;
  std::unique_ptr<CobaltContentUtilityClient> utility_client_;

  void InitializeHangWatcher();
};

}  // namespace cobalt

#endif  // COBALT_APP_COBALT_MAIN_DELEGATE_H_
