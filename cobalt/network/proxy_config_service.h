/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_NETWORK_PROXY_CONFIG_SERVICE_H_
#define COBALT_NETWORK_PROXY_CONFIG_SERVICE_H_

#include "base/logging.h"
#include "base/optional.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"

namespace cobalt {
namespace network {

class ProxyConfigService : public net::ProxyConfigServiceFixed {
 public:
  // If the optional ProxyConfig parameter is set, it overrides the system
  // proxy configuration.
  explicit ProxyConfigService(
      const base::optional<net::ProxyConfig>& proxy_config)
      : ProxyConfigServiceFixed(proxy_config.value_or(GetProxyConfig())) {}

  virtual ~ProxyConfigService();

  // Returns the proxy configuration from the system.
  net::ProxyConfig GetProxyConfig();
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_PROXY_CONFIG_SERVICE_H_
