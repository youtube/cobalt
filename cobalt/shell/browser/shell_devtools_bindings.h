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

#ifndef COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_BINDINGS_H_
#define COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_BINDINGS_H_

#include <memory>
#include <set>

#include "base/containers/span.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class NavigationHandle;

class ShellDevToolsDelegate {
 public:
  virtual void Close() = 0;
  virtual ~ShellDevToolsDelegate() {}
};

class WebContents;

class ShellDevToolsBindings : public WebContentsObserver,
                              public DevToolsAgentHostClient {
 public:
  ShellDevToolsBindings(WebContents* devtools_contents,
                        WebContents* inspected_contents,
                        ShellDevToolsDelegate* delegate);

  void InspectElementAt(int x, int y);
  virtual void Attach();
  void UpdateInspectedWebContents(WebContents* new_contents,
                                  base::OnceCallback<void()> callback);

  void CallClientFunction(
      const std::string& object_name,
      const std::string& method_name,
      const base::Value arg1 = {},
      const base::Value arg2 = {},
      const base::Value arg3 = {},
      base::OnceCallback<void(base::Value)> cb = base::NullCallback());

  ShellDevToolsBindings(const ShellDevToolsBindings&) = delete;
  ShellDevToolsBindings& operator=(const ShellDevToolsBindings&) = delete;

  ~ShellDevToolsBindings() override;

  WebContents* inspected_contents() { return inspected_contents_; }

 private:
  // content::DevToolsAgentHostClient implementation.
  void AgentHostClosed(DevToolsAgentHost* agent_host) override;
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;

  void HandleMessageFromDevToolsFrontend(base::Value::Dict message);

  // WebContentsObserver overrides
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  void SendMessageAck(int request_id, const base::Value::Dict arg);
  void AttachInternal();

  raw_ptr<WebContents, DanglingUntriaged> inspected_contents_;
  raw_ptr<ShellDevToolsDelegate, DanglingUntriaged> delegate_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  int inspect_element_at_x_;
  int inspect_element_at_y_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<DevToolsFrontendHost> frontend_host_;
#endif

  class NetworkResourceLoader;
  std::set<std::unique_ptr<NetworkResourceLoader>, base::UniquePtrComparator>
      loaders_;

  base::Value::Dict preferences_;

  using ExtensionsAPIs = std::map<std::string, std::string>;
  ExtensionsAPIs extensions_api_;
  base::WeakPtrFactory<ShellDevToolsBindings> weak_factory_{this};
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_DEVTOOLS_BINDINGS_H_
