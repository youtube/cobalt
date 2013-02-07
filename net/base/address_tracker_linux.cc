// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_tracker_linux.h"

#include <errno.h>
#include <linux/if.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"
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

}  // namespace

AddressTrackerLinux::AddressTrackerLinux(const base::Closure& address_callback,
                                         const base::Closure& link_callback)
    : address_callback_(address_callback),
      link_callback_(link_callback),
      netlink_fd_(-1),
      is_offline_(true),
      is_offline_initialized_(false),
      is_offline_initialized_cv_(&is_offline_lock_) {
  DCHECK(!address_callback.is_null());
  DCHECK(!link_callback.is_null());
}

AddressTrackerLinux::~AddressTrackerLinux() {
  CloseSocket();
}

void AddressTrackerLinux::Init() {
  netlink_fd_ = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (netlink_fd_ < 0) {
    PLOG(ERROR) << "Could not create NETLINK socket";
    AbortAndForceOnline();
    return;
  }

  // Request notifications.
  struct sockaddr_nl addr = {};
  addr.nl_family = AF_NETLINK;
  addr.nl_pid = getpid();
  // TODO(szym): Track RTMGRP_LINK as well for ifi_type, http://crbug.com/113993
  addr.nl_groups = RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_NOTIFY |
      RTMGRP_LINK;
  int rv = bind(netlink_fd_,
                reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr));
  if (rv < 0) {
    PLOG(ERROR) << "Could not bind NETLINK socket";
    AbortAndForceOnline();
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

  rv = HANDLE_EINTR(sendto(netlink_fd_, &request, request.header.nlmsg_len,
                           0, reinterpret_cast<struct sockaddr*>(&peer),
                           sizeof(peer)));
  if (rv < 0) {
    PLOG(ERROR) << "Could not send NETLINK request";
    AbortAndForceOnline();
    return;
  }

  // Consume pending message to populate the AddressMap, but don't notify.
  // Sending another request without first reading responses results in EBUSY.
  bool address_changed;
  bool link_changed;
  ReadMessages(&address_changed, &link_changed);

  // Request dump of link state
  request.header.nlmsg_type = RTM_GETLINK;

  rv = HANDLE_EINTR(sendto(netlink_fd_, &request, request.header.nlmsg_len, 0,
                           reinterpret_cast<struct sockaddr*>(&peer),
                           sizeof(peer)));
  if (rv < 0) {
    PLOG(ERROR) << "Could not send NETLINK request";
    AbortAndForceOnline();
    return;
  }

  // Consume pending message to populate links_online_, but don't notify.
  ReadMessages(&address_changed, &link_changed);
  {
    base::AutoLock lock(is_offline_lock_);
    is_offline_initialized_ = true;
    is_offline_initialized_cv_.Signal();
  }

  rv = MessageLoopForIO::current()->WatchFileDescriptor(
      netlink_fd_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
  if (rv < 0) {
    PLOG(ERROR) << "Could not watch NETLINK socket";
    AbortAndForceOnline();
    return;
  }
}

void AddressTrackerLinux::AbortAndForceOnline() {
  CloseSocket();
  base::AutoLock lock(is_offline_lock_);
  is_offline_ = false;
  is_offline_initialized_ = true;
  is_offline_initialized_cv_.Signal();
}

AddressTrackerLinux::AddressMap AddressTrackerLinux::GetAddressMap() const {
  base::AutoLock lock(address_map_lock_);
  return address_map_;
}

NetworkChangeNotifier::ConnectionType
AddressTrackerLinux::GetCurrentConnectionType() {
  // http://crbug.com/125097
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  base::AutoLock lock(is_offline_lock_);
  // Make sure the initial offline state is set before returning.
  while (!is_offline_initialized_) {
    is_offline_initialized_cv_.Wait();
  }
  // TODO(droger): Return something more detailed than CONNECTION_UNKNOWN.
  // http://crbug.com/160537
  return is_offline_ ? NetworkChangeNotifier::CONNECTION_NONE :
                       NetworkChangeNotifier::CONNECTION_UNKNOWN;
}

void AddressTrackerLinux::ReadMessages(bool* address_changed,
                                       bool* link_changed) {
  *address_changed = false;
  *link_changed = false;
  char buffer[4096];
  bool first_loop = true;
  for (;;) {
    int rv = HANDLE_EINTR(recv(netlink_fd_,
                               buffer,
                               sizeof(buffer),
                               // Block the first time through loop.
                               first_loop ? 0 : MSG_DONTWAIT));
    first_loop = false;
    if (rv == 0) {
      LOG(ERROR) << "Unexpected shutdown of NETLINK socket.";
      return;
    }
    if (rv < 0) {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        break;
      PLOG(ERROR) << "Failed to recv from netlink socket";
      return;
    }
    HandleMessage(buffer, rv, address_changed, link_changed);
  };
  if (*link_changed) {
    base::AutoLock lock(is_offline_lock_);
    is_offline_ = online_links_.empty();
  }
}

void AddressTrackerLinux::HandleMessage(const char* buffer,
                                        size_t length,
                                        bool* address_changed,
                                        bool* link_changed) {
  DCHECK(buffer);
  for (const struct nlmsghdr* header =
          reinterpret_cast<const struct nlmsghdr*>(buffer);
       NLMSG_OK(header, length);
       header = NLMSG_NEXT(header, length)) {
    switch (header->nlmsg_type) {
      case NLMSG_DONE:
        return;
      case NLMSG_ERROR: {
        const struct nlmsgerr* msg =
            reinterpret_cast<struct nlmsgerr*>(NLMSG_DATA(header));
        LOG(ERROR) << "Unexpected netlink error " << msg->error << ".";
      } return;
      case RTM_NEWADDR: {
        IPAddressNumber address;
        if (GetAddress(header, &address)) {
          base::AutoLock lock(address_map_lock_);
          const struct ifaddrmsg* msg =
              reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(header));
          // Only indicate change if the address is new or ifaddrmsg info has
          // changed.
          AddressMap::iterator it = address_map_.find(address);
          if (it == address_map_.end()) {
            address_map_.insert(it, std::make_pair(address, *msg));
            *address_changed = true;
          } else if (memcmp(&it->second, msg, sizeof(*msg))) {
            it->second = *msg;
            *address_changed = true;
          }
        }
      } break;
      case RTM_DELADDR: {
        IPAddressNumber address;
        if (GetAddress(header, &address)) {
          base::AutoLock lock(address_map_lock_);
          if (address_map_.erase(address))
            *address_changed = true;
        }
      } break;
      case RTM_NEWLINK: {
        const struct ifinfomsg* msg =
            reinterpret_cast<struct ifinfomsg*>(NLMSG_DATA(header));
        if (!(msg->ifi_flags & IFF_LOOPBACK) && (msg->ifi_flags & IFF_UP) &&
            (msg->ifi_flags & IFF_LOWER_UP) && (msg->ifi_flags & IFF_RUNNING)) {
          if (online_links_.insert(msg->ifi_index).second)
            *link_changed = true;
        } else {
          if (online_links_.erase(msg->ifi_index))
            *link_changed = true;
        }
      } break;
      case RTM_DELLINK: {
        const struct ifinfomsg* msg =
            reinterpret_cast<struct ifinfomsg*>(NLMSG_DATA(header));
        if (online_links_.erase(msg->ifi_index))
          *link_changed = true;
      } break;
      default:
        break;
    }
  }
}

void AddressTrackerLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(netlink_fd_, fd);
  bool address_changed;
  bool link_changed;
  ReadMessages(&address_changed, &link_changed);
  if (address_changed)
    address_callback_.Run();
  if (link_changed)
    link_callback_.Run();
}

void AddressTrackerLinux::OnFileCanWriteWithoutBlocking(int /* fd */) {}

void AddressTrackerLinux::CloseSocket() {
  if (netlink_fd_ >= 0 && HANDLE_EINTR(close(netlink_fd_)) < 0)
    PLOG(ERROR) << "Could not close NETLINK socket.";
  netlink_fd_ = -1;
}

}  // namespace internal
}  // namespace net
