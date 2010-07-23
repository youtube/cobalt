// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_

#include "net/proxy/polling_proxy_config_service.h"

namespace net {

// TODO(eroman): Use notification-based system APIs instead of polling for
//               changes.
class ProxyConfigServiceMac : public PollingProxyConfigService {
 public:
  ProxyConfigServiceMac();
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
