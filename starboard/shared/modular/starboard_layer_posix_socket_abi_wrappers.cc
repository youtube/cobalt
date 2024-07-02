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
#include <stdlib.h>
#include <string.h>

#if SB_HAS_QUIRK(SOCKADDR_WITH_LENGTH)
#include <net.h>
#endif
#include "starboard/log.h"

namespace {
// Corresponding arrays to for musl<->platform translation.
int MUSL_AI_ORDERED[] = {
    MUSL_AI_PASSIVE, MUSL_AI_CANONNAME,  MUSL_AI_NUMERICHOST, MUSL_AI_V4MAPPED,
    MUSL_AI_ALL,     MUSL_AI_ADDRCONFIG, MUSL_AI_NUMERICSERV,
};
int PLATFORM_AI_ORDERED[] = {
    AI_PASSIVE, AI_CANONNAME,  AI_NUMERICHOST, AI_V4MAPPED,
    AI_ALL,     AI_ADDRCONFIG, AI_NUMERICSERV,
};
// Corresponding arrays to for musl<->platform translation.
int MUSL_SOCK_ORDERED[] = {
    MUSL_SOCK_STREAM,
    MUSL_SOCK_DGRAM,
    MUSL_SOCK_RAW,
};
int PLATFORM_SOCK_ORDERED[] = {
    SOCK_STREAM,
    SOCK_DGRAM,
    SOCK_RAW,
};
// Corresponding arrays to for musl<->platform translation.
int MUSL_IPPROTO_ORDERED[] = {
    MUSL_IPPROTO_TCP,
    MUSL_IPPROTO_UDP,
};
int PLATFORM_IPPROTO_ORDERED[] = {
    IPPROTO_TCP,
    IPPROTO_UDP,
};
// Corresponding arrays to for musl<->platform translation.
int MUSL_AF_ORDERED[] = {
    MUSL_AF_INET,
    MUSL_AF_INET6,
};
int PLATFORM_AF_ORDERED[] = {
    AF_INET,
    AF_INET6,
};
int musl_hints_to_platform_hints(const struct musl_addrinfo* hints,
                                 struct addrinfo* platform_hints) {
  int ai_left = hints->ai_flags;
  for (int i = 0; i < sizeof(MUSL_AI_ORDERED) / sizeof(int); i++) {
    if (hints->ai_flags & MUSL_AI_ORDERED[i]) {
      platform_hints->ai_flags |= PLATFORM_AI_ORDERED[i];
      ai_left &= ~MUSL_AI_ORDERED[i];
    }
  }
  for (int i = 0; i < sizeof(MUSL_SOCK_ORDERED) / sizeof(int); i++) {
    if (hints->ai_socktype == MUSL_SOCK_ORDERED[i])
      platform_hints->ai_socktype = PLATFORM_SOCK_ORDERED[i];
  }
  for (int i = 0; i < sizeof(MUSL_IPPROTO_ORDERED) / sizeof(int); i++) {
    if (hints->ai_protocol == MUSL_IPPROTO_ORDERED[i])
      platform_hints->ai_protocol = PLATFORM_IPPROTO_ORDERED[i];
  }
  for (int i = 0; i < sizeof(MUSL_AF_ORDERED) / sizeof(int); i++) {
    if (hints->ai_family == MUSL_AF_ORDERED[i])
      platform_hints->ai_family = PLATFORM_AF_ORDERED[i];
  }
  if (ai_left != 0 ||
      (hints->ai_socktype != 0 && platform_hints->ai_socktype == 0) ||
      (hints->ai_protocol != 0 && platform_hints->ai_protocol == 0) ||
      (hints->ai_family != 0 && platform_hints->ai_family == 0))
    return -1;
  return 0;
}
int platform_hints_to_musl_hints(const struct addrinfo* hints,
                                 struct musl_addrinfo* musl_hints) {
  int ai_left = hints->ai_flags;
  for (int i = 0; i < sizeof(PLATFORM_AI_ORDERED) / sizeof(int); i++) {
    if (hints->ai_flags & PLATFORM_AI_ORDERED[i]) {
      musl_hints->ai_flags |= MUSL_AI_ORDERED[i];
      ai_left &= ~PLATFORM_AI_ORDERED[i];
    }
  }
  for (int i = 0; i < sizeof(PLATFORM_SOCK_ORDERED) / sizeof(int); i++) {
    if (hints->ai_socktype == PLATFORM_SOCK_ORDERED[i])
      musl_hints->ai_socktype = MUSL_SOCK_ORDERED[i];
  }
  for (int i = 0; i < sizeof(PLATFORM_IPPROTO_ORDERED) / sizeof(int); i++) {
    if (hints->ai_protocol == PLATFORM_IPPROTO_ORDERED[i])
      musl_hints->ai_protocol = MUSL_IPPROTO_ORDERED[i];
  }
  for (int i = 0; i < sizeof(PLATFORM_AF_ORDERED) / sizeof(int); i++) {
    if (hints->ai_family == PLATFORM_AF_ORDERED[i])
      musl_hints->ai_family = MUSL_AF_ORDERED[i];
  }
  if (ai_left != 0 ||
      (hints->ai_socktype != 0 && musl_hints->ai_socktype == 0) ||
      (hints->ai_protocol != 0 && musl_hints->ai_protocol == 0) ||
      (hints->ai_family != 0 && musl_hints->ai_family == 0))
    return -1;
  return 0;
}
}  // namespace

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
                                     const struct musl_addrinfo* hints,
                                     struct musl_addrinfo** res) {
  if (res == nullptr)
    return -1;
  // musl addrinfo definition might differ from platform definition.
  // So we need to do a manual conversion to avoid header mismatches.
  struct addrinfo new_hints = {0};

  if (hints != nullptr) {
    SbLogRawFormatF("hints ai_family: %d\n", hints->ai_family);
    SbLogRawFormatF("hints ai_socktype: %d\n", hints->ai_socktype);
    SbLogRawFormatF("hints ai_flags: %d\n", hints->ai_flags);
    SbLogRawFormatF("hints ai_protocol: %d\n", hints->ai_protocol);
    if (musl_hints_to_platform_hints(hints, &new_hints) == -1) {
      SbLogRawFormatF("musl_hints_to_platform_hints failed");
      return -1;
    }
    SbLogRawFormatF("new_hints ai_family: %d\n", new_hints.ai_family);
    SbLogRawFormatF("new_hints ai_socktype: %d\n", new_hints.ai_socktype);
    SbLogRawFormatF("new_hints ai_flags: %d\n", new_hints.ai_flags);
    SbLogRawFormatF("new_hints ai_protocol: %d\n", new_hints.ai_protocol);
  }

  addrinfo* native_res = nullptr;
  int result = getaddrinfo(
      node, service,
      (hints == nullptr) ? nullptr
                         : reinterpret_cast<const struct addrinfo*>(&new_hints),
      &native_res);

  struct addrinfo* ai = native_res;
  struct musl_addrinfo* last_ai = nullptr;
  *res = nullptr;
  while (ai != nullptr) {
    if (ai->ai_addr != nullptr) {
      addrinfo ai_copy = *ai;
      struct musl_addrinfo* musl_ai =
          (struct musl_addrinfo*)calloc(1, sizeof(struct musl_addrinfo));
      SbLogRawFormatF("returned ai_flags: %d\n", ai_copy.ai_flags);
      SbLogRawFormatF("returned ai_socktype: %d\n", ai_copy.ai_socktype);
      SbLogRawFormatF("returned ai_protocol: %d\n", ai_copy.ai_protocol);
      SbLogRawFormatF("returned ai_family: %d\n", ai_copy.ai_family);
      if (platform_hints_to_musl_hints(&ai_copy, musl_ai) == -1) {
        SbLogRawFormatF("platform_hints_to_musl_hints failed");
        free(musl_ai);
        return -1;
      }
      SbLogRawFormatF("translated ai_flags: %d\n", musl_ai->ai_flags);
      SbLogRawFormatF("translated ai_socktype: %d\n", musl_ai->ai_socktype);
      SbLogRawFormatF("translated ai_protocol: %d\n", musl_ai->ai_protocol);
      SbLogRawFormatF("translated ai_family: %d\n", musl_ai->ai_family);
      musl_ai->ai_addrlen = ai_copy.ai_addrlen;
      musl_ai->ai_addr =
          (struct musl_sockaddr*)calloc(1, sizeof(struct musl_sockaddr));
      SbLogRawFormatF("ai_addrlen: %d\n", ai_copy.ai_addrlen);
      for (int i = 0; i < sizeof(PLATFORM_AF_ORDERED) / sizeof(int); i++) {
        if (ai_copy.ai_addr->sa_family == PLATFORM_AF_ORDERED[i])
          musl_ai->ai_addr->sa_family = MUSL_AF_ORDERED[i];
      }
      SbLogRawFormatF("ai_copy.ai_addr->sa_family: %d\n",
                      ai_copy.ai_addr->sa_family);
      SbLogRawFormatF("musl_ai->ai_addr->sa_family: %d\n",
                      musl_ai->ai_addr->sa_family);
      if (ai_copy.ai_addr->sa_data != nullptr) {
        memcpy(musl_ai->ai_addr->sa_data, ai_copy.ai_addr->sa_data,
               sizeof(ai_copy.ai_addr->sa_data));
      }
      SbLogRawFormatF("musl_ai->ai_addr->sa_data: %s\n",
                      musl_ai->ai_addr->sa_data);

      SbLogRaw(musl_ai->ai_addr->sa_data);
      if (ai_copy.ai_canonname) {
        size_t canonname_len = strlen(ai_copy.ai_canonname);
        musl_ai->ai_canonname =
            reinterpret_cast<char*>(calloc(canonname_len + 1, sizeof(char)));
        memcpy(musl_ai->ai_canonname, ai_copy.ai_canonname, canonname_len);
      }
      if (*res == nullptr) {
        *res = musl_ai;
        last_ai = musl_ai;
      } else {
        last_ai->ai_next = musl_ai;
        last_ai = musl_ai;
      }
    }
    ai = ai->ai_next;
  }
  freeaddrinfo(native_res);

  return result;
}

SB_EXPORT void __abi_wrap_freeaddrinfo(struct musl_addrinfo* ai) {
  struct musl_addrinfo* ptr = ai;
  while (ai != nullptr) {
    if (ai->ai_addr != nullptr) {
      free(ai->ai_addr);
    }
    if (ai->ai_canonname != nullptr) {
      free(ai->ai_canonname);
    }
    ai = ai->ai_next;
    free(ptr);
    ptr = ai;
  }
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
