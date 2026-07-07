// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/socket.h"

#if SB_HAS_IPV6

#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "starboard/common/log.h"
#include "starboard/shared/posix/handle_eintr.h"

namespace {
bool ProbeIpv6Support() {
  const int probe_socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (probe_socket_fd < 0) {
    SB_LOG(WARNING) << "IPv6 Probe: Failed to create socket. error=" << errno;
    return false;
  }

  int reuse_addr_enabled = 1;
  if (setsockopt(probe_socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_enabled, sizeof(reuse_addr_enabled)) != 0) {
    SB_LOG(WARNING) << "IPv6 Probe: Failed to set SO_REUSEADDR. error=" << errno;
  }

  struct sockaddr_in6 probe_address;
  memset(&probe_address, 0, sizeof(probe_address));
  probe_address.sin6_family = AF_INET6;
  probe_address.sin6_addr = in6addr_any;
  probe_address.sin6_port = 0;

  const int bind_status = HANDLE_EINTR(bind(
      probe_socket_fd, reinterpret_cast<struct sockaddr*>(&probe_address),
      sizeof(probe_address)));
  
  if (bind_status != 0) {
    SB_LOG(WARNING) << "IPv6 Probe: Failed to bind test socket. error=" << errno;
  }

  close(probe_socket_fd);

  return (bind_status == 0);
}
}  // namespace

#endif  // SB_HAS_IPV6

bool SbSocketIsIpv6Supported() {
#if SB_HAS_IPV6
  return ProbeIpv6Support();
#else
  return false;
#endif
}
