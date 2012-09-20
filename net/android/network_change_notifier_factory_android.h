// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_ANDROID_H_

#include "base/compiler_specific.h"
#include "net/base/network_change_notifier_factory.h"

namespace net {

class NetworkChangeNotifier;

// NetworkChangeNotifierFactory creates Android-specific specialization of
// NetworkChangeNotifier.
class NetworkChangeNotifierFactoryAndroid :
    public NetworkChangeNotifierFactory {
 public:
  NetworkChangeNotifierFactoryAndroid();

  // Overrides of NetworkChangeNotifierFactory.
  virtual NetworkChangeNotifier* CreateInstance() OVERRIDE;
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
