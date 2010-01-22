// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_

#include "base/basictypes.h"
#include "net/base/network_change_notifier_helper.h"

namespace net {

class NetworkChangeNotifierWin : public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierWin();

  virtual void AddObserver(Observer* observer) {
    helper_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) {
    helper_.RemoveObserver(observer);
  }

 private:
  virtual ~NetworkChangeNotifierWin();

  void OnIPAddressChanged() { helper_.OnIPAddressChanged(); }

  internal::NetworkChangeNotifierHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierWin);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
