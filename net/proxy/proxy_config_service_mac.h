// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_

#include "net/proxy/proxy_config_service.h"

namespace net {

class ProxyConfigServiceMac : public ProxyConfigService {
 public:
  // ProxyConfigService methods:
  virtual int GetProxyConfig(ProxyConfig* config);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
