// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_win.h"

#include <iphlpapi.h>
#include <winsock2.h>

#pragma comment(lib, "iphlpapi.lib")

namespace net {

NetworkChangeNotifierWin::NetworkChangeNotifierWin() {
  memset(&addr_overlapped_, 0, sizeof addr_overlapped_);
  addr_overlapped_.hEvent = WSACreateEvent();
  WatchForAddressChange();
}

NetworkChangeNotifierWin::~NetworkChangeNotifierWin() {
  CancelIPChangeNotify(&addr_overlapped_);
  addr_watcher_.StopWatching();
  WSACloseEvent(addr_overlapped_.hEvent);
}

void NetworkChangeNotifierWin::OnObjectSignaled(HANDLE object) {
  NotifyObserversOfIPAddressChange();

  // Start watching for the next address change.
  WatchForAddressChange();
}

void NetworkChangeNotifierWin::WatchForAddressChange() {
  HANDLE handle = NULL;
  DWORD ret = NotifyAddrChange(&handle, &addr_overlapped_);
  CHECK(ret == ERROR_IO_PENDING);
  addr_watcher_.StartWatching(addr_overlapped_.hEvent, this);
}

}  // namespace net
