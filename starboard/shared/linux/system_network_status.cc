// Copyright 2021 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <asm/types.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <memory.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

#include "starboard/common/log.h"
#include "starboard/shared/linux/singleton.h"
#include "starboard/shared/linux/system_network_status.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/system.h"

#if SB_API_VERSION >= 13

namespace {

// This function will repeatedly run on the NetworkNotifier thread untile the
// Cobalt application is kill. This function checks kernel message for IP
// address changes.
bool GetOnlineStatus(bool* is_online_ptr, int netlink_fd) {
  SB_DCHECK(is_online_ptr != NULL);

  struct sockaddr_nl sa;
  memset(&sa, 0, sizeof(sa));
  sa.nl_family = AF_NETLINK;
  sa.nl_groups = RTMGRP_IPV4_IFADDR;
  sa.nl_pid = getpid();
  int bind_result = bind(netlink_fd, (struct sockaddr*)&sa, sizeof(sa));
  SB_DCHECK(bind_result == 0);

  char buf[8192];
  struct iovec iov;
  iov.iov_base = buf;
  iov.iov_len = sizeof(buf);

  struct msghdr msg;
  {
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
  }

  ssize_t status;
  status = recvmsg(netlink_fd, &msg, MSG_DONTWAIT);
  bool has_message = false;
  while (status >= 0) {
    SB_DCHECK(msg.msg_namelen == sizeof(sa));

    struct nlmsghdr* header;

    for (header = (struct nlmsghdr*)buf; status >= (ssize_t)sizeof(*header);) {
      int len = header->nlmsg_len;
      int l = len - sizeof(*header);

      SB_DCHECK(l >= 0);
      SB_DCHECK(len <= status);

      switch (header->nlmsg_type) {
        case RTM_DELADDR:
          *is_online_ptr = false;
          has_message = true;
          break;
        case RTM_NEWADDR:
          *is_online_ptr = true;
          has_message = true;
          break;
      }

      status -= NLMSG_ALIGN(len);
      header = (struct nlmsghdr*)((char*)header + NLMSG_ALIGN(len));
    }
    status = recvmsg(netlink_fd, &msg, MSG_DONTWAIT);
  }

  return has_message;
}

}  // namespace

bool NetworkNotifier::Initialize() {
  SB_DCHECK(!SbThreadIsValid(notifier_thread_));
  notifier_thread_ = SbThreadCreate(
      0, kSbThreadPriorityLow, kSbThreadNoAffinity, false, "NetworkNotifier",
      &NetworkNotifier::NotifierThreadEntry, this);
  SB_DCHECK(SbThreadIsValid(notifier_thread_));
  return true;
}

void* NetworkNotifier::NotifierThreadEntry(void* context) {
  auto* notifier = static_cast<NetworkNotifier*>(context);
  int netlink_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  bool is_online;
  do {
    if (GetOnlineStatus(&is_online, netlink_fd)) {
      notifier->set_online(is_online);
      auto* application = starboard::shared::starboard::Application::Get();
      if (is_online) {
        application->InjectOsNetworkConnectedEvent();
      } else {
        application->InjectOsNetworkDisconnectedEvent();
      }
    }
    usleep(1000);
  } while (1);

  return nullptr;
}

bool SbSystemNetworkIsDisconnected() {
  return !NetworkNotifier::GetOrCreateInstance()->is_online();
}

#endif  // SB_API_VERSION >= 13
