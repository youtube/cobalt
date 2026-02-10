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

#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

#include <sched.h>
#include <sys/socket.h>

#include <string>
#include <tuple>
#include <utility>

#include "build/build_config.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {

// Add helper functions for posix socket tests.
int PosixSocketCreateAndConnect(int server_domain,
                                int client_domain,
                                int port,
                                int64_t timeout,
                                int* listen_socket_fd,
                                int* client_socket_fd,
                                int* server_socket_fd) {
  int result = -1;
  // create listen socket, bind and listen on <port>
  *listen_socket_fd = socket(server_domain, SOCK_STREAM, IPPROTO_TCP);
  if (*listen_socket_fd < 0) {
    return -1;
  }
  // set socket reusable
  const int on = 1;
  result =
      setsockopt(*listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  if (result != 0) {
    close(*listen_socket_fd);
    return -1;
  }

  // bind socket with local address
  sockaddr_in6 address = {};
  EXPECT_TRUE(
      PosixGetLocalAddressIPv4(reinterpret_cast<sockaddr*>(&address)) == 0 ||
      PosixGetLocalAddressIPv6(reinterpret_cast<sockaddr*>(&address)) == 0);
  address.sin6_port = htons(PosixGetPortNumberForTests());

  result = bind(*listen_socket_fd, reinterpret_cast<struct sockaddr*>(&address),
                sizeof(struct sockaddr_in));
  if (result != 0) {
    close(*listen_socket_fd);
    return -1;
  }

  result = listen(*listen_socket_fd, kMaxConn);
  if (result != 0) {
    close(*listen_socket_fd);
    return -1;
  }

  // create client socket and connect to localhost:<port>
  *client_socket_fd = socket(client_domain, SOCK_STREAM, IPPROTO_TCP);
  if (*client_socket_fd < 0) {
    close(*listen_socket_fd);
    return -1;
  }

  result =
      setsockopt(*client_socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if (result != 0) {
    close(*listen_socket_fd);
    close(*client_socket_fd);
    return -1;
  }

  result =
      connect(*client_socket_fd, reinterpret_cast<struct sockaddr*>(&address),
              sizeof(struct sockaddr));
  if (result != 0) {
    close(*listen_socket_fd);
    close(*client_socket_fd);
    return -1;
  }

  int64_t start = starboard::CurrentMonotonicTime();
  while ((starboard::CurrentMonotonicTime() - start < timeout)) {
    *server_socket_fd = accept(*listen_socket_fd, NULL, NULL);
    if (*server_socket_fd > 0) {
      return 0;
    }

    // If we didn't get a socket, it should be pending.
    if (errno == EINPROGRESS || errno == EAGAIN
#if EWOULDBLOCK != EAGAIN
        || errno == EWOULDBLOCK)
#else
    )
#endif
    {
      // Just being polite.
      sched_yield();
    }
  }

  // timeout expired
  close(*listen_socket_fd);
  close(*client_socket_fd);
  return -1;
}

int PosixSocketSetReceiveBufferSize(int socket_fd, int32_t size) {
  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }

  return setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, "SO_RCVBUF", size);
}

int PosixSocketSetSendBufferSize(int socket_fd, int32_t size) {
  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }

  return setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, "SO_SNDBUF", size);
}

int PosixGetLocalAddressIPv4(sockaddr* address_ptr) {
  int result = -1;
  struct ifaddrs* ifaddr = NULL;
  if (getifaddrs(&ifaddr) != 0) {
    return -1;
  }
  /* Walk through linked list, maintaining head pointer so we
     can free list later. */
  for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }

    /* For an AF_INET* interface address, display the address. */
    if (ifa->ifa_addr->sa_family == AF_INET) {
      memcpy(address_ptr, ifa->ifa_addr, sizeof(struct sockaddr_in));
      result = 0;
      break;
    }
  }

  freeifaddrs(ifaddr);
  return result;
}

int PosixGetLocalAddressIPv6(sockaddr* address_ptr) {
  int result = -1;
  struct ifaddrs* ifaddr;
  if (getifaddrs(&ifaddr) == -1) {
    return -1;
  }
  /* Walk through linked list, maintaining head pointer so we
              can free list later. */
  for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }

    /* For an AF_INET* interface address, display the address. */
    if (ifa->ifa_addr->sa_family == AF_INET6) {
      memcpy(address_ptr, ifa->ifa_addr, sizeof(struct sockaddr_in6));
      result = 0;
      break;
    }
  }

  freeifaddrs(ifaddr);
  return result;
}

int port_number_for_tests = 0;
pthread_once_t valid_port_once_control = PTHREAD_ONCE_INIT;

void PosixInitializePortNumberForTests() {
  // Create a listening socket. Let the system choose a port for us.
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_fd < 0) {
    ADD_FAILURE() << "Socket create failed errno: " << errno;
    return;
  }

  int on = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
    ADD_FAILURE() << "Socket Set Reuse Address failed, errno: " << errno;
    HANDLE_EINTR(close(socket_fd));
    return;
  }

  // bind socket with local address
  struct sockaddr_in address = {};
  address.sin_port = 0;
  address.sin_family = AF_INET;

  int bind_result =
      bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr));

  if (bind_result != 0) {
    ADD_FAILURE() << "Socket Bind to 0 failed, errno: " << errno;
    HANDLE_EINTR(close(socket_fd));
    return;
  }

  int listen_result = listen(socket_fd, kMaxConn);
  if (listen_result != 0) {
    ADD_FAILURE() << "Socket Listen failed, errno: " << errno;
    HANDLE_EINTR(close(socket_fd));
    return;
  }

  // Query which port this socket was bound to and save it to valid_port_number.
  struct sockaddr_in addr_in = {0};
  socklen_t socklen = static_cast<socklen_t>(sizeof(addr_in));
  int local_add_result =
      getsockname(socket_fd, reinterpret_cast<sockaddr*>(&addr_in), &socklen);

  if (local_add_result < 0) {
    ADD_FAILURE() << "Socket get local address failed: " << local_add_result
                  << " Errno: " << errno;
    HANDLE_EINTR(close(socket_fd));
    return;
  }
  port_number_for_tests = addr_in.sin_port;

  // Clean up the socket.
  bool result = HANDLE_EINTR(close(socket_fd)) >= 0;
  SB_DCHECK(result);
}

int PosixGetPortNumberForTests() {
  pthread_once(&valid_port_once_control, &PosixInitializePortNumberForTests);
  SB_DLOG(INFO) << "PosixGetPortNumberForTests port_number_for_tests : "
                << port_number_for_tests;
  return port_number_for_tests;
}

namespace {
const char* PosixAddressFamilyName(int family) {
  const char* name = "unknown";
  switch (family) {
    case AF_UNSPEC:
      name = "unspecified";
      break;
    case AF_INET:
      name = "inet";
      break;
    case AF_INET6:
      name = "inet6";
      break;
  }
  return name;
}

const char* PosixSocketTypeName(int socktype) {
  const char* name = "unknown";
  switch (socktype) {
    case 0:
      name = "unspecified";
      break;
    case SOCK_STREAM:
      name = "stream";
      break;
    case SOCK_DGRAM:
      name = "dgram";
      break;
  }
  return name;
}

const char* PosixProtocolName(int protocol) {
  const char* name = "unknown";
  switch (protocol) {
    case 0:
      name = "unspecified";
      break;
    case IPPROTO_UDP:
      name = "udp";
      break;
    case IPPROTO_TCP:
      name = "tcp";
      break;
  }
  return name;
}
}  // namespace

std::string GetPosixSocketHintsName(
    ::testing::TestParamInfo<std::tuple<int, std::pair<int, int>>> info) {
  return starboard::FormatString(
      "family_%s_socktype_%s_protocol_%s",
      PosixAddressFamilyName(std::get<0>(info.param)),
      PosixSocketTypeName(std::get<1>(info.param).first),
      PosixProtocolName(std::get<1>(info.param).second));
}

}  // namespace nplb
