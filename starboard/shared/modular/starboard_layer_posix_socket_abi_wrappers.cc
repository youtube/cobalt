// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_socket_abi_wrappers.h"
#include <string.h>

#if SB_HAS_QUIRK(SOCKADDR_WITH_LENGTH)
#include <net.h>
#endif

SB_EXPORT int __abi_wrap_accept(int sockfd,
                                musl_sockaddr* addr,
                                socklen_t* addrlen_ptr) {
#if SB_HAS_QUIRK(SOCKADDR_WITH_LENGTH)
  if (addr != nullptr) {
    struct sockaddr new_addr = {};
    new_addr.sa_family = addr->sa_family;
    new_addr.sa_len = 0;
    memcpy(new_addr.sa_data, addr->sa_data, 14);
    addr = reinterpret_cast<musl_sockaddr*>(&new_addr);
  }
#endif
  return accept(sockfd, reinterpret_cast<struct sockaddr*>(addr), addrlen_ptr);
}

SB_EXPORT int __abi_wrap_bind(int sockfd,
                              const musl_sockaddr* addr,
                              socklen_t addrlen) {
#if SB_HAS_QUIRK(SOCKADDR_WITH_LENGTH)
  if (addr != nullptr) {
    struct sockaddr new_addr = {};
    new_addr.sa_family = addr->sa_family;
    new_addr.sa_len = 0;
    memcpy(new_addr.sa_data, addr->sa_data, 14);
    return bind(sockfd, reinterpret_cast<const struct sockaddr*>(&new_addr),
                addrlen);
  } else {
    return bind(sockfd, reinterpret_cast<const struct sockaddr*>(addr),
                addrlen);
  }
#else
  return bind(sockfd, reinterpret_cast<const struct sockaddr*>(addr), addrlen);
#endif
}

SB_EXPORT int __abi_wrap_connect(int sockfd,
                                 const musl_sockaddr* addr,
                                 socklen_t addrlen) {
#if SB_HAS_QUIRK(SOCKADDR_WITH_LENGTH)
  if (addr != nullptr) {
    struct sockaddr new_addr = {};
    new_addr.sa_family = addr->sa_family;
    new_addr.sa_len = 0;
    memcpy(new_addr.sa_data, addr->sa_data, 14);
    return connect(sockfd, reinterpret_cast<const struct sockaddr*>(&new_addr),
                   addrlen);
  } else {
    return connect(sockfd, reinterpret_cast<const struct sockaddr*>(addr),
                   addrlen);
  }
#else
  return connect(sockfd, reinterpret_cast<const struct sockaddr*>(addr),
                 addrlen);
#endif
}

SB_EXPORT int __abi_wrap_getaddrinfo(const char* node,
                                     const char* service,
                                     const struct addrinfo* hints,
                                     struct addrinfo** res) {
  int result = getaddrinfo(node, service, hints, res);

#if SB_HAS_QUIRK(SOCKADDR_WITH_LENGTH)
  struct addrinfo* ai = *res;
  while (ai != nullptr) {
    if (ai->ai_addr != nullptr) {
      musl_sockaddr* musl_addr_ptr =
          reinterpret_cast<musl_sockaddr*>(ai->ai_addr);
      struct sockaddr* addr_ptr =
          reinterpret_cast<struct sockaddr*>(ai->ai_addr);
      uint8_t sa_family = addr_ptr->sa_family;
      musl_addr_ptr->sa_family = sa_family;
    }
    ai = ai->ai_next;
  }
#endif
  return result;
}

SB_EXPORT int __abi_wrap_getifaddrs(struct ifaddrs** ifap) {
  int result = getifaddrs(ifap);
#if SB_HAS_QUIRK(SOCKADDR_WITH_LENGTH)
  struct ifaddrs* ptr = *ifap;
  struct ifaddrs* last_ptr = ptr;
  while (ptr != nullptr) {
    if (ptr->ifa_addr != nullptr) {
      musl_sockaddr* musl_addr_ptr =
          reinterpret_cast<musl_sockaddr*>(ptr->ifa_addr);
      struct sockaddr* addr_ptr =
          reinterpret_cast<struct sockaddr*>(ptr->ifa_addr);
      uint8_t sa_family = addr_ptr->sa_family;
      musl_addr_ptr->sa_family = sa_family;
    }
    ptr = ptr->ifa_next;
  }
#endif
  return result;
}

SB_EXPORT int __abi_wrap_setsockopt(int socket,
                                    int level,
                                    int option_name,
                                    const void* option_value,
                                    socklen_t option_len) {
  if (socket <= 0) {
    return -1;
  }
  int is_supported = 1;

#if SB_HAS_QUIRK(SOCKADDR_WITH_LENGTH)

  // The value from POSIX
#define MUSL_SOL_SOCKET 1  // level
#define MUSL_SO_REUSEADDR 2
#define MUSL_SO_RCVBUF 8
#define MUSL_SO_SNDBUF 7
#define MUSL_SO_KEEPALIVE 9

#define MUSL_SOL_TCP 6  // level
#define MUSL_TCP_NODELAY 1
#define MUSL_TCP_KEEPIDLE 4
#define MUSL_TCP_KEEPINTVL 5

#define MUSL_IPPROTO_TCP 6  // level

  if (level == MUSL_SOL_SOCKET) {
    level = SOL_SOCKET;
    switch (option_name) {
      case MUSL_SO_REUSEADDR:
        option_name = SO_REUSEADDR;
        break;
      case MUSL_SO_RCVBUF:
        option_name = SO_RCVBUF;
        break;
      case MUSL_SO_SNDBUF:
        option_name = SO_SNDBUF;
        break;
      case MUSL_SO_KEEPALIVE:
        is_supported = 0;
        break;
      default:
        is_supported = 0;
    }
  }
  if (level == MUSL_IPPROTO_TCP) {
    level = IPPROTO_TCP;
    switch (option_name) {
      case MUSL_TCP_NODELAY:
        option_name = SCE_NET_TCP_NODELAY;
        break;
      default:
        is_supported = 0;
    }
  }
  if (level = MUSL_SOL_TCP) {
    is_supported = 0;
  }
#endif

  if (is_supported) {
    return setsockopt(socket, level, option_name, option_value, option_len);
  }
  return 0;
}
