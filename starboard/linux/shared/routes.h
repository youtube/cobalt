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

#ifndef STARBOARD_LINUX_SHARED_ROUTES_H_
#define STARBOARD_LINUX_SHARED_ROUTES_H_

#include <arpa/inet.h>
#include <net/if.h>

#include "starboard/common/log.h"
#include "starboard/linux/shared/netlink.h"
#include "starboard/system.h"

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

// Helper class for examining device routes from the netlink socket API.
class Routes : public NetLink {
 public:
  struct Route {
    sa_family_t family;
    union {
      in_addr_t dst_addr;
      struct in6_addr dst_addr6;
    };
    int priority;
    char name[IF_NAMESIZE];
  };

  // Return true if the route has the default destination address.
  static bool IsDefaultRoute(const Route& route);

  // Return true if the route uses a wireless interface.
  static bool IsWirelessInterface(const Route& route);

  Routes() {}
  ~Routes() {}

  // Open the netlink socket for routing table information.
  bool Open();

  // Request a dump of the routing tables from the netlink socket.
  bool RequestDump();

  // Return the next route from the netlink interface message. Returns nullptr
  // when no more routes are available.
  Route* GetNextRoute();

 private:
  static void GetRouteFromNetlinkMessage(struct nlmsghdr* message,
                                         Route& route);

  Route last_route_;
};

}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_LINUX_SHARED_ROUTES_H_
