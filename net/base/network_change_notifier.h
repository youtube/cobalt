// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_H_

#include "base/basictypes.h"
#include "base/non_thread_safe.h"

namespace net {

// NetworkChangeNotifier monitors the system for network changes, and notifies
// observers on those events.
class NetworkChangeNotifier : public NonThreadSafe {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Will be called when the IP address of the primary interface changes.
    // This includes when the primary interface itself changes.
    virtual void OnIPAddressChanged() = 0;

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  NetworkChangeNotifier() {}
  virtual ~NetworkChangeNotifier() {}

  // These functions add and remove observers to/from the NetworkChangeNotifier.
  // Each call to AddObserver() must be matched with a corresponding call to
  // RemoveObserver() with the same parameter.  Observers must remove themselves
  // before they get deleted, otherwise the NetworkChangeNotifier may try to
  // notify the wrong object.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // This will create the platform specific default NetworkChangeNotifier.
  static NetworkChangeNotifier* CreateDefaultNetworkChangeNotifier();

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifier);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
