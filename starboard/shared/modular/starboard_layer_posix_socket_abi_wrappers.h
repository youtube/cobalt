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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SOCKET_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SOCKET_ABI_WRAPPERS_H_

#include <stdint.h>

#include <ifaddrs.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "starboard/configuration.h"
#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// The value from musl headers
#define MUSL_AI_PASSIVE 0x01
#define MUSL_AI_CANONNAME 0x02
#define MUSL_AI_NUMERICHOST 0x04
#define MUSL_AI_V4MAPPED 0x08
#define MUSL_AI_ALL 0x10
#define MUSL_AI_ADDRCONFIG 0x20
#define MUSL_AI_NUMERICSERV 0x400

#define MUSL_SOCK_STREAM 1
#define MUSL_SOCK_DGRAM 2
#define MUSL_SOCK_RAW 3

#define MUSL_IPPROTO_TCP 6
#define MUSL_IPPROTO_UDP 17

#define MUSL_AF_INET 2
#define MUSL_AF_INET6 10

// sizeof(sockaddr_in6) = 28
// This size enables musl_sockaddr to work for both IPv4 and IPv6
#define MUSL_SOCKADDR_SA_DATA_SIZE 28
typedef struct musl_sockaddr {
  uint16_t sa_family;
  char sa_data[MUSL_SOCKADDR_SA_DATA_SIZE];
} musl_sockaddr;

typedef struct musl_addrinfo {
  int32_t /* int */ ai_flags;
  int32_t /* int */ ai_family;
  int32_t /* int */ ai_socktype;
  int32_t /* int */ ai_protocol;
  uint32_t /* socklen_t */ ai_addrlen;
  struct musl_sockaddr* ai_addr;
  char* ai_canonname;
  struct musl_addrinfo* ai_next;
} musl_addrinfo;

SB_EXPORT int __abi_wrap_accept(int sockfd,
                                musl_sockaddr* addr,
                                socklen_t* addrlen_ptr);

SB_EXPORT int __abi_wrap_bind(int sockfd,
                              const musl_sockaddr* addr,
                              socklen_t addrlen);

SB_EXPORT int __abi_wrap_connect(int sockfd,
                                 const musl_sockaddr* addr,
                                 socklen_t addrlen);

SB_EXPORT int __abi_wrap_getaddrinfo(const char* node,
                                     const char* service,
                                     const struct musl_addrinfo* hints,
                                     struct musl_addrinfo** res);

SB_EXPORT void __abi_wrap_freeaddrinfo(struct musl_addrinfo* ai);

SB_EXPORT int __abi_wrap_getifaddrs(struct ifaddrs** ifap);

SB_EXPORT int __abi_wrap_setsockopt(int socket,
                                    int level,
                                    int option_name,
                                    const void* option_value,
                                    socklen_t option_len);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SOCKET_ABI_WRAPPERS_H_
