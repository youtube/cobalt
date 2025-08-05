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

#ifndef STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_SOCKET_HELPERS_H_
#define STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_SOCKET_HELPERS_H_

#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"

namespace starboard {
namespace nplb {

#if defined(SOMAXCONN)
const int kMaxConn = SOMAXCONN;
#else
const int kMaxConn = 128;
#endif

int PosixSocketCreateAndConnect(int server_domain,
                                int client_domain,
                                int port,
                                int64_t timeout,
                                int* listen_socket_fd,
                                int* server_socket_fd,
                                int* client_socket_fd);
int PosixGetLocalAddressIPv4(sockaddr* address_ptr);
int PosixGetLocalAddressIPv6(sockaddr* address_ptr);

int PosixSocketSetReceiveBufferSize(int socket_fd, int32_t size);
int PosixSocketSetSendBufferSize(int socket_fd, int32_t size);

// Returns a valid port number that can be bound to for use in nplb tests.
// This will always return the same port number.
int PosixGetPortNumberForTests();

// Creates a TCP/IP socket listening on all interfaces on the given port.
// int PosixCreateBoundListeningTcpSocket();

#if defined(MSG_NOSIGNAL)
const int kSendFlags = MSG_NOSIGNAL;
#else
const int kSendFlags = 0;
#endif

// Thread entry point to continuously write to a socket that is expected to
// be closed on another thread.
struct trio_socket_fd {
  int* listen_socket_fd_ptr;
  int* client_socket_fd_ptr;
  int* server_socket_fd_ptr;
};

// Writes the given data to socket, spinning until success or error.
bool PosixWriteBySpinning(int socket,
                          const char* data,
                          int data_size,
                          int64_t timeout);

// Reads the given amount of data from socket, spinning until success or error.
bool PosixReadBySpinning(int socket,
                         char* out_data,
                         int data_size,
                         int64_t timeout);

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_SOCKET_HELPERS_H_
