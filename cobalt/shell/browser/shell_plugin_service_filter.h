// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
