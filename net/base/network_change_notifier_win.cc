// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_win.h"

#include <iphlpapi.h>
#include <windows.h>
#include <winsock2.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/object_watcher.h"
#include "net/base/net_log.h"

namespace net {

class NetworkChangeNotifierWin::Impl
    : public base::ObjectWatcher::Delegate {
 public:
  explicit Impl(NetworkChangeNotifierWin* notifier);
  virtual ~Impl();

  void WatchForAddressChange();

  // ObjectWatcher::Delegate methods:

  virtual void OnObjectSignaled(HANDLE object);

 private:
  NetworkChangeNotifierWin* const notifier_;
  base::ObjectWatcher addr_watcher_;
  OVERLAPPED addr_overlapped_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

NetworkChangeNotifierWin::Impl::Impl(NetworkChangeNotifierWin* notifier)
    : notifier_(notifier) {
  memset(&addr_overlapped_, 0, sizeof(addr_overlapped_));
  addr_overlapped_.hEvent = WSACreateEvent();
}

NetworkChangeNotifierWin::Impl::~Impl() {
  CancelIPChangeNotify(&addr_overlapped_);
  addr_watcher_.StopWatching();
  WSACloseEvent(addr_overlapped_.hEvent);
  memset(&addr_overlapped_, 0, sizeof(addr_overlapped_));
}

void NetworkChangeNotifierWin::Impl::WatchForAddressChange() {
  HANDLE handle = NULL;
  DWORD ret = NotifyAddrChange(&handle, &addr_overlapped_);
  CHECK(ret == ERROR_IO_PENDING);
  addr_watcher_.StartWatching(addr_overlapped_.hEvent, this);
}

void NetworkChangeNotifierWin::Impl::OnObjectSignaled(HANDLE object) {
  notifier_->OnIPAddressChanged();

  // Start watching for further address changes.
  WatchForAddressChange();
}

NetworkChangeNotifierWin::NetworkChangeNotifierWin(NetLog* net_log)
    : impl_(new Impl(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      net_log_(net_log) {
  impl_->WatchForAddressChange();
}
void NetworkChangeNotifierWin::OnIPAddressChanged() {
  DCHECK(CalledOnValidThread());
  BoundNetLog net_log =
      BoundNetLog::Make(net_log_, NetLog::SOURCE_NETWORK_CHANGE_NOTIFIER);
  net_log.AddEvent(NetLog::TYPE_NETWORK_IP_ADDRESS_CHANGED, NULL);
  FOR_EACH_OBSERVER(Observer, observers_, OnIPAddressChanged());
}

void NetworkChangeNotifierWin::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void NetworkChangeNotifierWin::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

NetworkChangeNotifierWin::~NetworkChangeNotifierWin() {
  DCHECK(CalledOnValidThread());
}

}  // namespace net
