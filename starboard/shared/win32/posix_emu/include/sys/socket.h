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

#include <winsock2.h>
#undef NO_ERROR  // http://b/302733082#comment15
#include <BaseTsd.h>
#include <ws2tcpip.h>

#ifdef __cplusplus
extern "C" {
#endif

int sb_socket(int domain, int type, int protocol);
#define socket sb_socket

int sb_bind(int socket, const struct sockaddr* address, socklen_t address_len);
#define bind sb_bind

int sb_listen(int socket, int backlog);
#define listen sb_listen

int sb_accept(int socket, sockaddr* addr, int* addrlen);
#define accept sb_accept

int sb_connect(int socket, sockaddr* name, int namelen);
#define connect sb_connect

int sb_send(int sockfd, const void* buf, size_t len, int flags);
#define send sb_send

int sb_recv(int sockfd, void* buf, size_t len, int flags);
#define recv sb_recv

int sb_setsockopt(int socket,
                  int level,
                  int option_name,
                  const void* option_value,
                  int option_len);
#define setsockopt sb_setsockopt

#ifdef __cplusplus
}
#endif
#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_SOCKET_H_
