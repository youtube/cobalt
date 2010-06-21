// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include <errno.h>
#include <sys/socket.h>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/network_change_notifier_netlink_linux.h"

namespace net {

namespace {

const int kInvalidSocket = -1;

}  // namespace

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux(NetLog* net_log)
    : netlink_fd_(kInvalidSocket),
#if defined(OS_CHROMEOS)
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)),
#endif
      loop_(MessageLoopForIO::current()),
      net_log_(net_log) {
  netlink_fd_ = InitializeNetlinkSocket();
  if (netlink_fd_ < 0) {
    netlink_fd_ = kInvalidSocket;
    return;
  }

  ListenForNotifications();
  loop_->AddDestructionObserver(this);
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  DCHECK(CalledOnValidThread());
  StopWatching();

  if (loop_)
    loop_->RemoveDestructionObserver(this);
}

void NetworkChangeNotifierLinux::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void NetworkChangeNotifierLinux::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void NetworkChangeNotifierLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(fd, netlink_fd_);

  ListenForNotifications();
}

void NetworkChangeNotifierLinux::OnFileCanWriteWithoutBlocking(int /* fd */) {
  DCHECK(CalledOnValidThread());
  NOTREACHED();
}

void NetworkChangeNotifierLinux::WillDestroyCurrentMessageLoop() {
  DCHECK(CalledOnValidThread());
  StopWatching();
  loop_ = NULL;
}

void NetworkChangeNotifierLinux::ListenForNotifications() {
  DCHECK(CalledOnValidThread());
  char buf[4096];
  int rv = ReadNotificationMessage(buf, arraysize(buf));
  while (rv > 0 ) {
    if (HandleNetlinkMessage(buf, rv)) {
      LOG(INFO) << "Detected IP address changes.";

#if defined(OS_CHROMEOS)
      // TODO(zelidrag): chromium-os:3996 - introduced artificial delay to
      // work around the issue of proxy initialization before name resolving
      // is functional in ChromeOS. This should be removed once this bug
      // is properly fixed.
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          factory_.NewRunnableMethod(
              &NetworkChangeNotifierLinux::NotifyObserversIPAddressChanged),
          500);
#else
      NotifyObserversIPAddressChanged();
#endif
    }
    rv = ReadNotificationMessage(buf, arraysize(buf));
  }

  if (rv == ERR_IO_PENDING) {
    rv = loop_->WatchFileDescriptor(
        netlink_fd_, false, MessageLoopForIO::WATCH_READ, &netlink_watcher_,
        this);
    LOG_IF(ERROR, !rv) << "Failed to watch netlink socket: " << netlink_fd_;
  }
}

void NetworkChangeNotifierLinux::NotifyObserversIPAddressChanged() {
  BoundNetLog net_log =
      BoundNetLog::Make(net_log_, NetLog::SOURCE_NETWORK_CHANGE_NOTIFIER);
  // TODO(willchan): Add the netlink information into an EventParameter.
  net_log.AddEvent(NetLog::TYPE_NETWORK_IP_ADDRESS_CHANGED, NULL);
  FOR_EACH_OBSERVER(Observer, observers_, OnIPAddressChanged());
}

int NetworkChangeNotifierLinux::ReadNotificationMessage(char* buf, size_t len) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(len, 0u);
  DCHECK(buf);

  memset(buf, 0, sizeof(buf));
  int rv = recv(netlink_fd_, buf, len, 0);
  if (rv > 0) {
    return rv;
  } else {
    DCHECK_NE(rv, 0);
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      PLOG(DFATAL) << "recv";
      return ERR_FAILED;
    }

    return ERR_IO_PENDING;
  }
}

void NetworkChangeNotifierLinux::StopWatching() {
  DCHECK(CalledOnValidThread());
  if (netlink_fd_ != kInvalidSocket) {
    if (HANDLE_EINTR(close(netlink_fd_)) != 0)
      PLOG(ERROR) << "Failed to close socket";
    netlink_fd_ = kInvalidSocket;
    netlink_watcher_.StopWatchingFileDescriptor();
  }
}

}  // namespace net
