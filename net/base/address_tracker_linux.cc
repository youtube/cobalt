// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_tracker_linux.h"

#include <errno.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "net/base/network_change_notifier_linux.h"

namespace net {
namespace internal {

namespace {

// Retrieves address from NETLINK address message.
bool GetAddress(const struct nlmsghdr* header, IPAddressNumber* out) {
  const struct ifaddrmsg* msg =
      reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(header));
  size_t address_length = 0;
  switch (msg->ifa_family) {
    case AF_INET:
      address_length = kIPv4AddressSize;
      break;
    case AF_INET6:
      address_length = kIPv6AddressSize;
      break;
    default:
      // Unknown family.
      return false;
  }
  // Use IFA_ADDRESS unless IFA_LOCAL is present. This behavior here is based on
  // getaddrinfo in glibc (check_pf.c). Judging from kernel implementation of
  // NETLINK, IPv4 addresses have only the IFA_ADDRESS attribute, while IPv6
  // have the IFA_LOCAL attribute.
  unsigned char* address = NULL;
  unsigned char* local = NULL;
  size_t length = IFA_PAYLOAD(header);
  for (const struct rtattr* attr =
           reinterpret_cast<const struct rtattr*>(IFA_RTA(msg));
       RTA_OK(attr, length);
       attr = RTA_NEXT(attr, length)) {
    switch (attr->rta_type) {
      case IFA_ADDRESS:
        DCHECK_GE(RTA_PAYLOAD(attr), address_length);
        address = reinterpret_cast<unsigned char*>(RTA_DATA(attr));
        break;
      case IFA_LOCAL:
        DCHECK_GE(RTA_PAYLOAD(attr), address_length);
        local = reinterpret_cast<unsigned char*>(RTA_DATA(attr));
        break;
      default:
        break;
    }
  }
  if (local)
    address = local;
  if (!address)
    return false;
  out->assign(address, address + address_length);
  return true;
}

void CloseSocket(int fd) {
  if (HANDLE_EINTR(close(fd)) < 0)
    PLOG(ERROR) << "Could not close NETLINK socket.";
}

}  // namespace

AddressTrackerLinux::AddressTrackerLinux(const base::Closure& callback)
    : callback_(callback),
      netlink_fd_(-1) {
  DCHECK(!callback.is_null());
}

AddressTrackerLinux::~AddressTrackerLinux() {
  if (netlink_fd_ >= 0)
    CloseSocket(netlink_fd_);
}

void AddressTrackerLinux::Init() {
  int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (sock < 0) {
    PLOG(ERROR) << "Could not create NETLINK socket";
    return;
  }

  // Request notifications.
  struct sockaddr_nl addr = {};
  addr.nl_family = AF_NETLINK;
  addr.nl_pid = getpid();
  // TODO(szym): Track RTMGRP_LINK as well for ifi_type, http://crbug.com/113993
  addr.nl_groups = RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_NOTIFY;
  int rv = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
  if (rv < 0) {
    PLOG(ERROR) << "Could not bind NETLINK socket";
    CloseSocket(sock);
    return;
  }

  // Watch for asynchronous messages.
  if (SetNonBlocking(sock)) {
    PLOG(ERROR) << "Could not make NETLINK socket non-blocking";
    CloseSocket(sock);
    return;
  }

  rv = MessageLoopForIO::current()->WatchFileDescriptor(
      sock, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
  if (rv < 0) {
    PLOG(ERROR) << "Could not watch NETLINK socket";
    CloseSocket(sock);
    return;
  }

  // Request dump of addresses.
  struct sockaddr_nl peer = {};
  peer.nl_family = AF_NETLINK;

  struct {
    struct nlmsghdr header;
    struct rtgenmsg msg;
  } request = {};

  request.header.nlmsg_len = NLMSG_LENGTH(sizeof(request.msg));
  request.header.nlmsg_type = RTM_GETADDR;
  request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  request.header.nlmsg_pid = getpid();
  request.msg.rtgen_family = AF_UNSPEC;

  rv = HANDLE_EINTR(sendto(sock, &request, request.header.nlmsg_len, 0,
                           reinterpret_cast<struct sockaddr*>(&peer),
                           sizeof(peer)));
  if (rv < 0) {
    PLOG(ERROR) << "Could not send NETLINK request";
    CloseSocket(sock);
    return;
  }

  netlink_fd_ = sock;

  // Consume any pending messages to populate the AddressMap, but don't notify.
  ReadMessages();
}

AddressTrackerLinux::AddressMap AddressTrackerLinux::GetAddressMap() const {
  base::AutoLock lock(lock_);
  return map_;
}

bool AddressTrackerLinux::ReadMessages() {
  char buffer[4096];
  bool changed = false;
  for (;;) {
    int rv = HANDLE_EINTR(recv(netlink_fd_, buffer, sizeof(buffer), 0));
    if (rv == 0) {
      LOG(ERROR) << "Unexpected shutdown of NETLINK socket.";
      return false;
    }
    if (rv < 0) {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        break;
      PLOG(ERROR) << "Failed to recv from netlink socket";
      return false;
    }
    changed |= HandleMessage(buffer, rv);
  };
  return changed;
}

bool AddressTrackerLinux::HandleMessage(const char* buffer, size_t length) {
  DCHECK(buffer);
  bool changed = false;
  for (const struct nlmsghdr* header =
          reinterpret_cast<const struct nlmsghdr*>(buffer);
       NLMSG_OK(header, length);
       header = NLMSG_NEXT(header, length)) {
    switch (header->nlmsg_type) {
      case NLMSG_DONE:
        return changed;
      case NLMSG_ERROR:
        LOG(ERROR) << "Unexpected netlink error.";
        return changed;
      case RTM_NEWADDR: {
        IPAddressNumber address;
        if (GetAddress(header, &address)) {
          base::AutoLock lock(lock_);
          const struct ifaddrmsg* msg =
              reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(header));
          // Only indicate change if the address is new or ifaddrmsg info has
          // changed.
          AddressMap::iterator it = map_.find(address);
          if (it == map_.end()) {
            map_.insert(it, std::make_pair(address, *msg));
            changed = true;
          } else if (memcmp(&it->second, msg, sizeof(*msg))) {
            it->second = *msg;
            changed = true;
          }
        }
      } break;
      case RTM_DELADDR: {
        IPAddressNumber address;
        if (GetAddress(header, &address)) {
          base::AutoLock lock(lock_);
          if (map_.erase(address))
            changed = true;
        }
      } break;
      default:
        break;
    }
  }
  return changed;
}

void AddressTrackerLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(netlink_fd_, fd);
  if (ReadMessages())
    callback_.Run();
}

void AddressTrackerLinux::OnFileCanWriteWithoutBlocking(int /* fd */) {}

}  // namespace internal
}  // namespace net
