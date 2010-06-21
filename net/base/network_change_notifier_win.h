// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/object_watcher.h"
#include "base/observer_list.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetLog;

class NetworkChangeNotifierWin : public NetworkChangeNotifier,
                                 public NonThreadSafe {
 public:
  explicit NetworkChangeNotifierWin(NetLog* net_log);

  // Called by NetworkChangeNotifierWin::Impl.
  void OnIPAddressChanged();

  // NetworkChangeNotifier methods:
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

 private:
  class Impl;

  virtual ~NetworkChangeNotifierWin();

  // TODO(willchan): Fix the URLRequestContextGetter leaks and flip the false to
  // true so we assert that all observers have been removed.
  ObserverList<Observer, false> observers_;
  scoped_ptr<Impl> impl_;
  NetLog* const net_log_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierWin);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
