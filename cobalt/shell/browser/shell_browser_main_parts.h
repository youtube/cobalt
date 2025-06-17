// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  ShellBrowserMainParts();

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
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<crash_reporter::ChildExitObserver> child_exit_observer_;
#endif
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
