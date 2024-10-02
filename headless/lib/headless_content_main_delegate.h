// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_
#define HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "headless/lib/headless_content_client.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class CommandLine;
}  // namespace base

namespace headless {

class HeadlessBrowserImpl;

// Exported for tests.
class HEADLESS_EXPORT HeadlessContentMainDelegate
    : public content::ContentMainDelegate {
 public:
  explicit HeadlessContentMainDelegate(
      std::unique_ptr<HeadlessBrowserImpl> browser);

  HeadlessContentMainDelegate(const HeadlessContentMainDelegate&) = delete;
  HeadlessContentMainDelegate& operator=(const HeadlessContentMainDelegate&) =
      delete;

  ~HeadlessContentMainDelegate() override;

 private:
  // content::ContentMainDelegate implementation:
  absl::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
  absl::optional<int> PreBrowserMain() override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

  absl::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
#if BUILDFLAG(IS_MAC)
  void PlatformPreBrowserMain();
#endif

  // TODO(caseq): get rid of this method and GetInstance(), tests should get
  // browser through other means.
  // Note this is nullptr in processes other than the browser.
  HeadlessBrowserImpl* browser() const { return browser_.get(); }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void ZygoteForked() override;
#endif

  friend class HeadlessBrowserTest;

  static HeadlessContentMainDelegate* GetInstance();

  void InitLogging(const base::CommandLine& command_line);
  void InitCrashReporter(const base::CommandLine& command_line);

  std::unique_ptr<content::ContentRendererClient> renderer_client_;
  std::unique_ptr<content::ContentBrowserClient> browser_client_;
  std::unique_ptr<content::ContentUtilityClient> utility_client_;
  HeadlessContentClient content_client_;

  std::unique_ptr<HeadlessBrowserImpl> browser_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_
