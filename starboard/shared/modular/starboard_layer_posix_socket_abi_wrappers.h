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

#include <ifaddrs.h>
#include <netdb.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_uio_abi_wrappers.h"

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
#define MUSL_SOCK_SEQPACKET 5

#define MUSL_IPPROTO_TCP 6
#define MUSL_IPPROTO_UDP 17

#define MUSL_AF_UNIX 1
#define MUSL_AF_INET 2
#define MUSL_AF_INET6 10

#define MUSL_SHUT_RD 0
#define MUSL_SHUT_WR 1
#define MUSL_SHUT_RDWR 2

#define MUSL_MSG_OOB 0x1
#define MUSL_MSG_PEEK 0x2
#define MUSL_MSG_DONTROUTE 0x4
#define MUSL_MSG_CTRUNC 0x8
#define MUSL_MSG_TRUNC 0x20
#define MUSL_MSG_DONTWAIT 0x40
#define MUSL_MSG_EOR 0x80
#define MUSL_MSG_WAITALL 0x100
#define MUSL_MSG_CONFIRM 0x800
#define MUSL_MSG_NOSIGNAL 0x4000
#define MUSL_MSG_MORE 0x8000

// Errcodes defined from MUSL's <netdb.h>
#define MUSL_EAI_BADFLAGS -1
#define MUSL_EAI_NONAME -2
#define MUSL_EAI_AGAIN -3
#define MUSL_EAI_FAIL -4
#define MUSL_EAI_NODATA -5
#define MUSL_EAI_FAMILY -6
#define MUSL_EAI_SOCKTYPE -7
#define MUSL_EAI_SERVICE -8
#define MUSL_EAI_MEMORY -10
#define MUSL_EAI_SYSTEM -11
#define MUSL_EAI_OVERFLOW -12

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

// The padding is defined based on the musl header
// in third_party/musl/include/sys/socket.h.
struct musl_cmsghdr {
#if SB_IS(64_BIT) && SB_IS(BIG_ENDIAN)
  int __pad1;
#endif
  socklen_t cmsg_len;
#if SB_IS(64_BIT) && SB_IS(LITTLE_ENDIAN)
  int __pad1;
#endif
  int cmsg_level;
  int cmsg_type;
};

struct musl_msghdr {
  void* msg_name;
  socklen_t msg_namelen;
  struct musl_iovec* msg_iov;
#if SB_IS(64_BIT) && SB_IS(BIG_ENDIAN)
  int __pad1;
#endif
  int msg_iovlen;
#if SB_IS(64_BIT) && SB_IS(LITTLE_ENDIAN)
  int __pad1;
#endif
  void* msg_control;
#if SB_IS(64_BIT) && SB_IS(BIG_ENDIAN)
  int __pad2;
#endif
  socklen_t msg_controllen;
#if SB_IS(64_BIT) && SB_IS(LITTLE_ENDIAN)
  int __pad2;
#endif
  int msg_flags;
};

#if SB_IS(64_BIT)
#if SB_IS(ARCH_X64)
static_assert(sizeof(struct musl_cmsghdr) == 16, "musl_cmsghdr size mismatch");
static_assert(offsetof(struct musl_cmsghdr, cmsg_level) == 8,
              "musl_cmsghdr.cmsg_level offset mismatch");
static_assert(sizeof(struct musl_msghdr) == 56, "musl_msghdr size mismatch");
static_assert(offsetof(struct musl_msghdr, msg_iov) == 16,
              "musl_msghdr.msg_iov offset mismatch");
#elif SB_IS(ARCH_ARM64)
static_assert(sizeof(struct musl_cmsghdr) == 16, "musl_cmsghdr size mismatch");
static_assert(offsetof(struct musl_cmsghdr, cmsg_level) == 8,
              "musl_cmsghdr.cmsg_level offset mismatch");
static_assert(sizeof(struct musl_msghdr) == 56, "musl_msghdr size mismatch");
static_assert(offsetof(struct musl_msghdr, msg_iov) == 16,
              "musl_msghdr.msg_iov offset mismatch");
#endif  // SB_IS(ARCH_X64)
#endif  // SB_IS(64_BIT)

#define MUSL_CMSG_ALIGN(len) \
  (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define MUSL_CMSG_HDR_LEN MUSL_CMSG_ALIGN(sizeof(struct musl_cmsghdr))
#define MUSL_CMSG_DATA(cmsg) ((unsigned char*)(cmsg) + MUSL_CMSG_HDR_LEN)
#define MUSL_CMSG_LEN(len) (MUSL_CMSG_HDR_LEN + (len))
#define MUSL_CMSG_SPACE(len) (MUSL_CMSG_HDR_LEN + MUSL_CMSG_ALIGN(len))

#define MUSL_CMSG_FIRSTHDR(mhdr)                           \
  (((mhdr)->msg_controllen >= sizeof(struct musl_cmsghdr)) \
       ? (struct musl_cmsghdr*)(mhdr)->msg_control         \
       : (struct musl_cmsghdr*)NULL)

#define MUSL_CMSG_NXTHDR(mhdr, cmsg)                                      \
  (((cmsg) == NULL)                                                       \
       ? MUSL_CMSG_FIRSTHDR(mhdr)                                         \
       : (((unsigned char*)(cmsg) + MUSL_CMSG_ALIGN((cmsg)->cmsg_len)) >= \
                  ((unsigned char*)((mhdr)->msg_control) +                \
                   (mhdr)->msg_controllen)                                \
              ? (struct musl_cmsghdr*)NULL                                \
              : (struct musl_cmsghdr*)((unsigned char*)(cmsg) +           \
                                       MUSL_CMSG_ALIGN((cmsg)->cmsg_len))))

SB_EXPORT int __abi_wrap_accept(int sockfd,
                                musl_sockaddr* addr,
                                socklen_t* addrlen_ptr);

SB_EXPORT int __abi_wrap_bind(int sockfd,
                              const musl_sockaddr* addr,
                              socklen_t addrlen);

SB_EXPORT int __abi_wrap_connect(int sockfd,
                                 const musl_sockaddr* addr,
                                 socklen_t addrlen);

SB_EXPORT const char* __abi_wrap_gai_strerror(int ecode);

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

SB_EXPORT int __abi_wrap_shutdown(int socket, int how);

SB_EXPORT ssize_t __abi_wrap_sendmsg(int sockfd,
                                     const struct musl_msghdr* msg,
                                     int flags);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SOCKET_ABI_WRAPPERS_H_
