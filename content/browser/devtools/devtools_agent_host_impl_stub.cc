// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  return false;
}

// static
void DevToolsAgentHost::DetachAllClients() {}

// static
DevToolsAgentHost::List DevToolsAgentHost::GetOrCreateAll() {
  return {};
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(
    const std::string& id) {
  return nullptr;
}

// static
scoped_refptr<DevToolsAgentHostImpl> DevToolsAgentHostImpl::GetForId(
    const std::string& id) {
  return nullptr;
}

// static
void DevToolsAgentHostImpl::GetOrCreateAll() {}

bool DevToolsAgentHostImpl::Inspect() {
  return false;
}

}  // namespace content
