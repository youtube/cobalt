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

namespace starboard {
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
  // set socket reuseable
  const int on = 1;
  result =
      setsockopt(*listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  if (result != 0) {
    close(*listen_socket_fd);
    return -1;
  }
  // bind socket with local address
  struct sockaddr_in address = {};
  result =
      PosixGetLocalAddressiIPv4(reinterpret_cast<struct sockaddr*>(&address));

  address.sin_port = port;
  address.sin_family = AF_INET;
  if (result != 0) {
    return -1;
  }

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
              sizeof(struct sockaddr_in));
  if (result != 0) {
    close(*listen_socket_fd);
    close(*client_socket_fd);
    return -1;
  }

  int64_t start = CurrentMonotonicTime();
  while ((CurrentMonotonicTime() - start < timeout)) {
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
      SbThreadYield();
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

int PosixGetLocalAddressiIPv4(sockaddr* address_ptr) {
  int result = -1;
  struct ifaddrs* ifaddr = NULL;
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
    if (ifa->ifa_addr->sa_family == AF_INET) {
      memcpy(address_ptr, ifa->ifa_addr, sizeof(struct sockaddr_in));
      result = 0;
      break;
    }
  }

  freeifaddrs(ifaddr);
  return result;
}

#if SB_HAS(IPV6)
int PosixGetLocalAddressiIPv6(sockaddr_in6* address_ptr) {
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
#endif

}  // namespace nplb
}  // namespace starboard
