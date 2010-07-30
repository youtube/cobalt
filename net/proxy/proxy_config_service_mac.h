// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
#pragma once

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "net/base/network_config_watcher_mac.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"

namespace net {

class ProxyConfigServiceMac : public ProxyConfigService,
                              public NetworkConfigWatcherMac {
 public:
  // Constructs a ProxyConfigService that watches the Mac OS system settings.
  // This instance is expected to be operated and deleted on |io_loop|
  // (however it may be constructed from a different thread).
  explicit ProxyConfigServiceMac(MessageLoop* io_loop);
  virtual ~ProxyConfigServiceMac();

 public:
  // ProxyConfigService implementation:
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual bool GetLatestProxyConfig(ProxyConfig* config);

 protected:
  // NetworkConfigWatcherMac implementation:
  virtual void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store);
  virtual void OnNetworkConfigChange(CFArrayRef changed_keys);

 private:
  class Helper;

  // Called when the proxy configuration has changed, to notify the observers.
  void OnProxyConfigChanged(const ProxyConfig& new_config);

  ObserverList<Observer> observers_;

  // Holds the last system proxy settings that we fetched.
  bool has_fetched_config_;
  ProxyConfig last_config_fetched_;

  scoped_refptr<Helper> helper_;

  // The thread that we expect to be operated on.
  MessageLoop* io_loop_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceMac);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
