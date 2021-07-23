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

#include "starboard/linux/shared/routes.h"

#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
// This include has to be loaded after net/if.h.
#include <linux/wireless.h>  // NOLINT(build/include_order)
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vector>

#include "starboard/common/log.h"
#include "starboard/linux/shared/netlink.h"

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

namespace {
void LogLastError(const char* msg) {
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  SbSystemError error_code = SbSystemGetLastError();
  if (SbSystemGetErrorString(error_code, msgbuf, kErrorMessageBufferSize) > 0) {
    SB_LOG(ERROR) << msg << ": " << msgbuf;
  }
}
}  // namespace

// Return true if the route has the default destination address.
// static
bool Routes::IsDefaultRoute(const Route& route) {
  switch (route.family) {
    case AF_INET6:
      return memcmp(&route.dst_addr6, &in6addr_any, sizeof(in6addr_any)) == 0;
    case AF_INET:
      return route.dst_addr == INADDR_ANY;
    default:
      return false;
  }
}

// Return true if the route uses a wireless interface.
// static
bool Routes::IsWirelessInterface(const Route& route) {
  int socket_fd = -1;

  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    LogLastError("socket");
    return false;
  }

  struct iwreq request;
  memset(&request, 0, sizeof(request));
  strncpy(request.ifr_name, route.name, IFNAMSIZ);
  if (ioctl(socket_fd, SIOCGIWNAME, &request) != -1) {
    close(socket_fd);
    return true;
  }

  close(socket_fd);
  return false;
}

// Open the netlink socket for routing table information.
bool Routes::Open() {
  //  https://datatracker.ietf.org/doc/html/rfc3549#section-3.1

  //  https://man7.org/linux/man-pages/man7/rtnetlink.7.html
  return NetLink::Open(SOCK_DGRAM, NETLINK_ROUTE);
}

// Request a dump of the routing tables from the netlink socket.
bool Routes::RequestDump() {
  if (!IsOpened())
    Open();
  struct rtmsg header;
  memset(&header, 0, sizeof(header));
  // header.rtm_family=AF_INET6;
  header.rtm_table = RT_TABLE_UNSPEC;
  header.rtm_protocol = RTPROT_UNSPEC;
  header.rtm_scope = RT_SCOPE_UNIVERSE;
  header.rtm_type = RTN_UNSPEC;
  return Request(RTM_GETROUTE, NLM_F_DUMP, &header, sizeof(header));
}

// static
void Routes::GetRouteFromNetlinkMessage(struct nlmsghdr* message,
                                        Route& route) {
  auto route_message = reinterpret_cast<struct rtmsg*>(NLMSG_DATA(message));
  memset(&route, 0, sizeof(route));
  route.family = route_message->rtm_family;
  int payload = RTM_PAYLOAD(message);
  for (struct rtattr* attribute = RTM_RTA(route_message);
       RTA_OK(attribute, payload); attribute = RTA_NEXT(attribute, payload)) {
    void* data = RTA_DATA(attribute);
    const int& value(*reinterpret_cast<int*>(data));
    switch (attribute->rta_type) {
      case RTA_OIF:
        if (!if_indextoname(value, route.name)) {
          continue;
        }
        break;
      case RTA_DST:
        if (route.family == AF_INET6) {
          memcpy(&route.dst_addr6, data, sizeof(route.dst_addr6));
        } else {
          SB_DCHECK(route.family == AF_INET);
          memcpy(&route.dst_addr, data, sizeof(route.dst_addr));
        }
        break;
      case RTA_PRIORITY:
        route.priority = value;
    }
  }
}

// Return the next route from the netlink interface message. Returns nullptr
// when no more routes are available.
Routes::Route* Routes::GetNextRoute() {
  struct nlmsghdr* message = nullptr;
  while ((message = GetNextMessage())) {
    auto route = reinterpret_cast<struct rtmsg*>(NLMSG_DATA(message));
    if (route->rtm_table != RT_TABLE_MAIN) {
      continue;
    }
    if (route->rtm_family == AF_INET || route->rtm_family == AF_INET6) {
      GetRouteFromNetlinkMessage(message, last_route_);
      return &last_route_;
    }
  }
  return nullptr;
}

}  // namespace shared
}  // namespace starboard
