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

#ifndef COBALT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
#define COBALT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "content/public/browser/browser_main_parts.h"

namespace performance_manager {
class PerformanceManagerLifetime;
}  // namespace performance_manager

#if BUILDFLAG(IS_ANDROID)
namespace crash_reporter {
class ChildExitObserver;
}
#endif

namespace content {
class ShellPlatformDelegate;

class ShellBrowserMainParts : public BrowserMainParts {
 public:
  explicit ShellBrowserMainParts(bool is_visible = true);

  ShellBrowserMainParts(const ShellBrowserMainParts&) = delete;
  ShellBrowserMainParts& operator=(const ShellBrowserMainParts&) = delete;

  ~ShellBrowserMainParts() override;

  // BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  int PreCreateThreads() override;
  void PostCreateThreads() override;
  void PostCreateMainMessageLoop() override;
  void ToolkitInitialized() override;
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  ShellBrowserContext* browser_context() { return browser_context_.get(); }
  ShellBrowserContext* off_the_record_browser_context() {
    return off_the_record_browser_context_.get();
  }

 protected:
  virtual void InitializeBrowserContexts();
  virtual void InitializeMessageLoopContext();
  // Gets the ShellPlatformDelegate to be used. May be a subclass of
  // ShellPlatformDelegate to change behaviour based on platform or for tests.
  virtual std::unique_ptr<ShellPlatformDelegate> CreateShellPlatformDelegate();

  void set_browser_context(ShellBrowserContext* context) {
    browser_context_.reset(context);
  }
  void set_off_the_record_browser_context(ShellBrowserContext* context) {
    off_the_record_browser_context_.reset(context);
  }

 private:
  std::unique_ptr<ShellBrowserContext> browser_context_;
  std::unique_ptr<ShellBrowserContext> off_the_record_browser_context_;

  std::unique_ptr<performance_manager::PerformanceManagerLifetime>
      performance_manager_lifetime_;
  bool is_visible_;
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<crash_reporter::ChildExitObserver> child_exit_observer_;
#endif
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
