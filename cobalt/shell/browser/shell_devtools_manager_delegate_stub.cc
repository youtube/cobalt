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

#include "cobalt/shell/browser/shell_devtools_manager_delegate.h"

#include <string>

#include "content/public/browser/devtools_agent_host.h"

namespace content {

// static
int ShellDevToolsManagerDelegate::GetHttpHandlerPort() {
  return 0;
}

// static
void ShellDevToolsManagerDelegate::StartHttpHandler(
    BrowserContext* browser_context) {}

// static
void ShellDevToolsManagerDelegate::StopHttpHandler() {}

ShellDevToolsManagerDelegate::ShellDevToolsManagerDelegate(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ShellDevToolsManagerDelegate::~ShellDevToolsManagerDelegate() = default;

BrowserContext* ShellDevToolsManagerDelegate::GetDefaultBrowserContext() {
  return browser_context_;
}

void ShellDevToolsManagerDelegate::ClientAttached(
    content::DevToolsAgentHostClientChannel* channel) {}

void ShellDevToolsManagerDelegate::ClientDetached(
    content::DevToolsAgentHostClientChannel* channel) {}

scoped_refptr<DevToolsAgentHost> ShellDevToolsManagerDelegate::CreateNewTarget(
    const GURL& url,
    content::DevToolsManagerDelegate::TargetType target_type,
    bool new_window) {
  return nullptr;
}

std::string ShellDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return std::string();
}

bool ShellDevToolsManagerDelegate::HasBundledFrontendResources() {
  return false;
}

}  // namespace content
