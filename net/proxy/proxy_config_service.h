// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_H_
#pragma once

namespace net {

class ProxyConfig;

// Service for watching when the proxy settings have changed.
class ProxyConfigService {
 public:
  // Observer for being notified when the proxy settings have changed.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnProxyConfigChanged(const ProxyConfig& config) = 0;
  };

  virtual ~ProxyConfigService() {}

  // Adds/Removes an observer that will be called whenever the proxy
  // configuration has changed.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets the most recent value of the proxy configuration. Returns false if
  // it is not available yet. In the case where we returned false, it is
  // guaranteed that subscribed observers will be notified of a change at
  // some point in the future once the configuration is available.
  // Note that to avoid re-entrancy problems, implementations should not
  // dispatch any change notifications from within this function.
  virtual bool GetLatestProxyConfig(ProxyConfig* config) = 0;

  // ProxyService will call this periodically during periods of activity.
  // It can be used as a signal for polling-based implementations.
  //
  // Note that this is purely used as an optimization -- polling
  // implementations could simply set a global timer that goes off every
  // X seconds at which point they check for changes. However that has
  // the disadvantage of doing continuous work even during idle periods.
  virtual void OnLazyPoll() {}
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_H_
