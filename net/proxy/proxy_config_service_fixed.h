// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_
#pragma once

#include "net/base/net_errors.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"

namespace net {

// Implementation of ProxyConfigService that returns a fixed result.
class ProxyConfigServiceFixed : public ProxyConfigService {
 public:
  explicit ProxyConfigServiceFixed(const ProxyConfig& pc);
  virtual ~ProxyConfigServiceFixed();

  // ProxyConfigService methods:
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual ConfigAvailability GetLatestProxyConfig(ProxyConfig* config);

 private:
  ProxyConfig pc_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_FIXED_H_
