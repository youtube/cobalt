// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MOCK_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_MOCK_NETWORK_CHANGE_NOTIFIER_H_

#include "base/basictypes.h"
#include "net/base/network_change_notifier.h"

namespace net {

class MockNetworkChangeNotifier : public NetworkChangeNotifier {
 public:
  MockNetworkChangeNotifier() : observer_(NULL) {}

  virtual ~MockNetworkChangeNotifier() {
    CHECK(!observer_);
  }

  void NotifyIPAddressChange() {
    if (observer_)
      observer_->OnIPAddressChanged();
  }

  virtual void AddObserver(Observer* observer) {
    CHECK(!observer_);
    observer_ = observer;
  }

  virtual void RemoveObserver(Observer* observer) {
    CHECK(observer_ == observer);
    observer_ = NULL;
  }

 private:
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(MockNetworkChangeNotifier);
};

}  // namespace net

#endif  // NET_BASE_MOCK_NETWORK_CHANGE_NOTIFIER_H_
