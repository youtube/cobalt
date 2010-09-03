// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
#pragma once

#include <SystemConfiguration/SCDynamicStore.h>

#include "base/basictypes.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_config_watcher_mac.h"

namespace net {

class NetworkChangeNotifierMac : public NetworkConfigWatcherMac,
                                 public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierMac();

 protected:
  // NetworkConfigWatcherMac implementation:
  virtual void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store);
  virtual void OnNetworkConfigChange(CFArrayRef changed_keys);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierMac);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
