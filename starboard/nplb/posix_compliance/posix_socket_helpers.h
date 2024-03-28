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

#include "starboard/common/time.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"

namespace starboard {
namespace nplb {

#if defined(SOMAXCONN)
const int kMaxConn = SOMAXCONN;
#else
// Some posix platforms such as FreeBSD do not define SOMAXCONN.
// In this case, set the value to an arbitrary number large enough to
// satisfy most use-cases and tests, empirically we have found that 128
// is sufficient.  All implementations of listen() specify that a backlog
// parameter larger than the system max will be silently truncated to the
// system's max.
const int kMaxConn = 128;
#endif

int PosixSocketCreateAndConnect(int server_domain,
                                int client_domain,
                                int port,
                                int64_t timeout,
                                int* listen_socket_fd,
                                int* server_socket_fd,
                                int* client_socket_fd);
int PosixGetLocalAddressiIPv4(sockaddr* address_ptr);
#if SB_HAS(IPV6)
int PosixGetLocalAddressiIPv6(sockaddr_in6* address_ptr);
#endif  // SB_HAS(IPV6)

int PosixSocketSetReceiveBufferSize(int socket_fd, int32_t size);
int PosixSocketSetSendBufferSize(int socket_fd, int32_t size);

#if defined(MSG_NOSIGNAL)
const int kSendFlags = MSG_NOSIGNAL;
#else
const int kSendFlags = 0;
#endif

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_SOCKET_HELPERS_H_
