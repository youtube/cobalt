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

#include "cobalt/browser/proxy_server_support.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cobalt {
namespace browser {

void ConfigureProxyFromCommandLineIfNeeded(
    network::mojom::NetworkContextParams* network_context_params) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string proxy_server;
  if (command_line.HasSwitch("proxy-server")) {
    proxy_server = command_line.GetSwitchValueASCII("proxy-server");
  } else if (command_line.HasSwitch("proxy")) {
    proxy_server = command_line.GetSwitchValueASCII("proxy");
  }

  if (!proxy_server.empty()) {
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy_server);

    if (command_line.HasSwitch("proxy-bypass-list")) {
      std::string bypass_list =
          command_line.GetSwitchValueASCII("proxy-bypass-list");
      proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }

    network_context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation(
            proxy_config, net::DefineNetworkTrafficAnnotation(
                              "cobalt_manual_proxy",
                              "Manually configured proxy via command line"));
    LOG(INFO) << "Configuring Cobalt to use proxy: " << proxy_server;
  }
}

}  // namespace browser
}  // namespace cobalt
