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

#ifndef COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_
#define COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

class BrowserContext;

class ShellDevToolsManagerDelegate : public DevToolsManagerDelegate {
 public:
  static void StartHttpHandler(BrowserContext* browser_context);
  static void StopHttpHandler();
  static int GetHttpHandlerPort();

  explicit ShellDevToolsManagerDelegate(BrowserContext* browser_context);

  ShellDevToolsManagerDelegate(const ShellDevToolsManagerDelegate&) = delete;
  ShellDevToolsManagerDelegate& operator=(const ShellDevToolsManagerDelegate&) =
      delete;

  ~ShellDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  BrowserContext* GetDefaultBrowserContext() override;
  scoped_refptr<DevToolsAgentHost> CreateNewTarget(const GURL& url,
                                                   TargetType target_type,
                                                   bool new_window) override;
  std::string GetDiscoveryPageHTML() override;
  bool HasBundledFrontendResources() override;
  void ClientAttached(
      content::DevToolsAgentHostClientChannel* channel) override;
  void ClientDetached(
      content::DevToolsAgentHostClientChannel* channel) override;

 private:
  raw_ptr<BrowserContext, DanglingUntriaged> browser_context_;
  base::flat_set<content::DevToolsAgentHostClient*> clients_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_
