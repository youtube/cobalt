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

#include "cobalt/shell/browser/shell_plugin_service_filter.h"

#include "content/public/common/webplugininfo.h"

namespace content {

ShellPluginServiceFilter::ShellPluginServiceFilter() = default;

ShellPluginServiceFilter::~ShellPluginServiceFilter() = default;

bool ShellPluginServiceFilter::IsPluginAvailable(
    content::BrowserContext* browser_context,
    const WebPluginInfo& plugin) {
  return plugin.name == u"Blink Test Plugin" ||
         plugin.name == u"Blink Deprecated Test Plugin" ||
         plugin.name == u"WebKit Test PlugIn";
}

bool ShellPluginServiceFilter::CanLoadPlugin(int render_process_id,
                                             const base::FilePath& path) {
  return true;
}

}  // namespace content
