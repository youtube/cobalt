// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_APP_MAIN_DELEGATE_H_
#define UI_WEBUI_EXAMPLES_APP_MAIN_DELEGATE_H_

#include "content/public/app/content_main_delegate.h"

namespace content {
class ContentBrowserClient;
class ContentClient;
}  // namespace content

namespace webui_examples {

class MainDelegate : public content::ContentMainDelegate {
 public:
  MainDelegate();
  MainDelegate(const MainDelegate&) = delete;
  MainDelegate& operator=(const MainDelegate&) = delete;
  ~MainDelegate() override;

 private:
  // content::ContentMainDelegate:
  absl::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;

  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<content::ContentBrowserClient> content_browser_client_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_APP_MAIN_DELEGATE_H_
