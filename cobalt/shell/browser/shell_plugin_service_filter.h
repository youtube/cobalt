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

#ifndef COBALT_SHELL_BROWSER_SHELL_PLUGIN_SERVICE_FILTER_H_
#define COBALT_SHELL_BROWSER_SHELL_PLUGIN_SERVICE_FILTER_H_

#include "content/public/browser/plugin_service_filter.h"

namespace content {

class ShellPluginServiceFilter : public PluginServiceFilter {
 public:
  ShellPluginServiceFilter();

  ShellPluginServiceFilter(const ShellPluginServiceFilter&) = delete;
  ShellPluginServiceFilter& operator=(const ShellPluginServiceFilter&) = delete;

  ~ShellPluginServiceFilter() override;

  // PluginServiceFilter implementation.
  bool IsPluginAvailable(content::BrowserContext* browser_context,
                         const WebPluginInfo& plugin) override;

  bool CanLoadPlugin(int render_process_id,
                     const base::FilePath& path) override;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_PLUGIN_SERVICE_FILTER_H_
