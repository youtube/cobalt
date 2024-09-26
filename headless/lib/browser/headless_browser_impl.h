// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "headless/public/headless_browser.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "headless/lib/browser/headless_devtools_manager_delegate.h"
#include "headless/public/headless_export.h"

#if defined(HEADLESS_USE_PREFS)
class PrefService;
#endif

#if defined(HEADLESS_USE_POLICY)
namespace policy {
class PolicyService;
}  // namespace policy
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/display/screen.h"
#endif

namespace ui {
class Compositor;
}  // namespace ui

namespace gfx {
class Rect;
}  // namespace gfx

namespace headless {

class HeadlessBrowserContextImpl;
class HeadlessBrowserMainParts;
class HeadlessRequestContextManager;
class HeadlessWebContentsImpl;

extern const base::FilePath::CharType kDefaultProfileName[];

// Exported for tests.
class HEADLESS_EXPORT HeadlessBrowserImpl : public HeadlessBrowser {
 public:
  explicit HeadlessBrowserImpl(
      base::OnceCallback<void(HeadlessBrowser*)> on_start_callback);

  HeadlessBrowserImpl(const HeadlessBrowserImpl&) = delete;
  HeadlessBrowserImpl& operator=(const HeadlessBrowserImpl&) = delete;

  ~HeadlessBrowserImpl() override;

  // HeadlessBrowser implementation:
  HeadlessBrowserContext::Builder CreateBrowserContextBuilder() override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const override;
  void Shutdown() override;
  std::vector<HeadlessBrowserContext*> GetAllBrowserContexts() override;
  HeadlessWebContents* GetWebContentsForDevToolsAgentHostId(
      const std::string& devtools_agent_host_id) override;
  HeadlessBrowserContext* GetBrowserContextForId(
      const std::string& id) override;
  void SetDefaultBrowserContext(
      HeadlessBrowserContext* browser_context) override;
  HeadlessBrowserContext* GetDefaultBrowserContext() override;

  void set_browser_main_parts(HeadlessBrowserMainParts* browser_main_parts);
  HeadlessBrowserMainParts* browser_main_parts() const;

  void RunOnStartCallback();

  void SetOptions(HeadlessBrowser::Options options);
  HeadlessBrowser::Options* options() { return &options_.value(); }

  HeadlessBrowserContext* CreateBrowserContext(
      HeadlessBrowserContext::Builder* builder);
  // Close given |browser_context| and delete it
  // (all web contents associated with it go away too).
  void DestroyBrowserContext(HeadlessBrowserContextImpl* browser_context);

  HeadlessWebContentsImpl* GetWebContentsForWindowId(const int window_id);

  base::WeakPtr<HeadlessBrowserImpl> GetWeakPtr();

  // All the methods that begin with Platform need to be implemented by the
  // platform specific headless implementation.
  // Helper for one time initialization of application
  void PlatformInitialize();
  void PlatformStart();
  void PlatformInitializeWebContents(HeadlessWebContentsImpl* web_contents);
  void PlatformSetWebContentsBounds(HeadlessWebContentsImpl* web_contents,
                                    const gfx::Rect& bounds);
  ui::Compositor* PlatformGetCompositor(HeadlessWebContentsImpl* web_contents);

  void ShutdownWithExitCode(int exit_code);

  int exit_code() const { return exit_code_; }

#if defined(HEADLESS_USE_PREFS)
  PrefService* GetPrefs();
#endif

#if defined(HEADLESS_USE_POLICY)
  policy::PolicyService* GetPolicyService();
#endif

 private:
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<display::ScopedNativeScreen> screen_;
#endif

  base::OnceCallback<void(HeadlessBrowser*)> on_start_callback_;
  absl::optional<HeadlessBrowser::Options> options_;
  raw_ptr<HeadlessBrowserMainParts, DanglingUntriaged> browser_main_parts_ =
      nullptr;
  int exit_code_ = 0;

  base::flat_map<std::string, std::unique_ptr<HeadlessBrowserContextImpl>>
      browser_contexts_;
  raw_ptr<HeadlessBrowserContext, DanglingUntriaged> default_browser_context_ =
      nullptr;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::unique_ptr<HeadlessRequestContextManager>
      system_request_context_manager_;
  base::WeakPtrFactory<HeadlessBrowserImpl> weak_ptr_factory_{this};
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
