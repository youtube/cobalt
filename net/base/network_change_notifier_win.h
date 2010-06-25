// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/object_watcher.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetworkChangeNotifierWin : public NetworkChangeNotifier,
                                 public base::ObjectWatcher::Delegate {
 public:
  NetworkChangeNotifierWin();

 private:
  virtual ~NetworkChangeNotifierWin();

  // ObjectWatcher::Delegate methods:
  virtual void OnObjectSignaled(HANDLE object);

  // Begins listening for a single subsequent address change.
  void WatchForAddressChange();

  base::ObjectWatcher addr_watcher_;
  OVERLAPPED addr_overlapped_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierWin);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_WIN_H_
