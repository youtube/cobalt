// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/rtnetlink.h>
#include <string.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

const int kInvalidSocket = -1;

// Return true on success, false on failure.
// Too small a function to bother putting in a library?
bool SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (-1 == flags)
    return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 ? true : false;
}

}  // namespace

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux()
    : netlink_fd_(kInvalidSocket),
      loop_(MessageLoopForIO::current()) {
  netlink_fd_ = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (netlink_fd_ < 0) {
    PLOG(ERROR) << "Error creating netlink socket";
    return;
  }

  if (!SetNonBlocking(netlink_fd_)) {
    PLOG(ERROR) << "Failed to set netlink socket to non-blocking mode.";
    if (close(netlink_fd_) != 0)
      PLOG(ERROR) << "Failed to close socket";
    netlink_fd_ = kInvalidSocket;
    return;
  }

  memset(&local_addr_, 0, sizeof(local_addr_));
  local_addr_.nl_family = AF_NETLINK;
  local_addr_.nl_pid = getpid();
  local_addr_.nl_groups =
      RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_NOTIFY;
  int ret = bind(netlink_fd_, reinterpret_cast<struct sockaddr*>(&local_addr_),
                 sizeof(local_addr_));
  if (ret < 0) {
    PLOG(ERROR) << "Error binding netlink socket";
    if (close(netlink_fd_) != 0)
      PLOG(ERROR) << "Failed to close socket";
    netlink_fd_ = kInvalidSocket;
    return;
  }

  ListenForNotifications();
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  if (netlink_fd_ != kInvalidSocket) {
    if (close(netlink_fd_) != 0)
      PLOG(ERROR) << "Failed to close socket";
    netlink_fd_ = kInvalidSocket;
    netlink_watcher_.StopWatchingFileDescriptor();
  }
}

void NetworkChangeNotifierLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, netlink_fd_);

  ListenForNotifications();
}

void NetworkChangeNotifierLinux::OnFileCanWriteWithoutBlocking(int /* fd */) {
  NOTREACHED();
}

void NetworkChangeNotifierLinux::ListenForNotifications() {
  char buf[4096];
  int rv = ReadNotificationMessage(buf, arraysize(buf));
  while (rv > 0 ) {
    const struct nlmsghdr* netlink_message_header =
        reinterpret_cast<struct nlmsghdr*>(buf);
    HandleNotifications(netlink_message_header, rv);
    rv = ReadNotificationMessage(buf, arraysize(buf));
  }

  if (rv == ERR_IO_PENDING) {
    rv = loop_->WatchFileDescriptor(
        netlink_fd_, false, MessageLoopForIO::WATCH_READ, &netlink_watcher_,
        this);
    LOG_IF(ERROR, !rv) << "Failed to watch netlink socket: " << netlink_fd_;
  }
}

int NetworkChangeNotifierLinux::ReadNotificationMessage(char* buf, size_t len) {
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

void NetworkChangeNotifierLinux::HandleNotifications(
    const struct nlmsghdr* netlink_message_header, size_t len) {
  DCHECK(netlink_message_header);
  for (; NLMSG_OK(netlink_message_header, len);
       netlink_message_header = NLMSG_NEXT(netlink_message_header, len)) {
    int netlink_message_type = netlink_message_header->nlmsg_type;
    switch (netlink_message_type) {
      case NLMSG_DONE:
        NOTREACHED()
            << "This is a monitoring netlink socket.  It should never be done.";
        return;
      case NLMSG_ERROR:
        LOG(ERROR) << "Unexpected netlink error.";
        return;
      // During IP address changes, we will see all these messages.  Only fire
      // the notification when we get a new address or remove an address.  We
      // may still end up notifying observers more than strictly necessary, but
      // if the primary interface goes down and back up, then this is necessary.
      case RTM_NEWADDR:
      case RTM_DELADDR:
        helper_.OnIPAddressChanged();
        return;
      case RTM_NEWLINK:
      case RTM_DELLINK:
        return;
      default:
        LOG(DFATAL) << "Received unexpected netlink message type: "
                    << netlink_message_type;
        return;
    }
  }
}

}  // namespace net
