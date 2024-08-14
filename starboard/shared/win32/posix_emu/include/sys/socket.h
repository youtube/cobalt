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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_SOCKET_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_SOCKET_H_

#include <sys/types.h>
#include <sys/uio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "starboard/shared/win32/posix_emu/include/remove_problematic_windows_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

struct msghdr {
  void* msg_name;        /* Optional address */
  socklen_t msg_namelen; /* Size of address */
  struct iovec* msg_iov; /* Scatter/gather array */
  size_t msg_iovlen;     /* # elements in msg_iov */
  void* msg_control;     /* Ancillary data, see below */
  size_t msg_controllen; /* Ancillary data buffer len */
  int msg_flags;         /* Flags (unused) */
};

struct ip_mreqn {
  struct in_addr imr_multiaddr; /* IP multicast group address */
  struct in_addr imr_address;   /* IP address of local interface */
  int imr_ifindex;              /* interface index */
};

int sb_socket(int domain, int type, int protocol);
#define socket sb_socket

int sb_bind(int socket, const struct sockaddr* address, socklen_t address_len);
#define bind sb_bind

int sb_listen(int socket, int backlog);
#define listen sb_listen

int sb_accept(int socket, sockaddr* addr, int* addrlen);
#define accept sb_accept

int sb_connect(int socket, const sockaddr* name, int namelen);
#define connect sb_connect

int sb_send(int sockfd, const void* buf, size_t len, int flags);
#define send sb_send

int sb_recv(int sockfd, void* buf, size_t len, int flags);
#define recv sb_recv

int sb_sendto(int sockfd,
              const void* buf,
              size_t len,
              int flags,
              const struct sockaddr* dest_addr,
              socklen_t dest_len);
#define sendto sb_sendto

int sb_recvfrom(int sockfd,
                void* buf,
                size_t len,
                int flags,
                struct sockaddr* address,
                socklen_t* address_len);
#define recvfrom sb_recvfrom

int sb_getsockopt(int socket,
                  int level,
                  int option_name,
                  void* option_value,
                  int* option_len);
#define getsockopt sb_getsockopt

int sb_setsockopt(int socket,
                  int level,
                  int option_name,
                  const void* option_value,
                  int option_len);
#define setsockopt sb_setsockopt

int sb_getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
#define getsockname sb_getsockname

int recvmsg(int socket, struct msghdr* message, int flags);

int posix_socket_get_fd_from_handle(SOCKET socket);
SOCKET posix_socket_get_handle_from_fd(int socket);

#ifdef __cplusplus
}
#endif
#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_SOCKET_H_
