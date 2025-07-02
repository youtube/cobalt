// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <iomanip>

#include "starboard/common/log.h"
#include "starboard/configuration.h"

namespace starboard {
namespace {
constexpr int kInvalidSocketId = -1;
}

struct sockaddr_storage GetUnspecifiedAddress(int address_type, int port) {
  switch (address_type) {
    case AF_INET: {
      struct sockaddr_storage address = {};
      struct sockaddr_in* addr =
          reinterpret_cast<struct sockaddr_in*>(&address);
      addr->sin_family = AF_INET;
      addr->sin_addr.s_addr = INADDR_ANY;
      addr->sin_port = htons(port);
      return address;
    }
    case AF_INET6: {
      struct sockaddr_storage address = {};
      struct sockaddr_in6* addr =
          reinterpret_cast<struct sockaddr_in6*>(&address);
      addr->sin6_family = AF_INET6;
      addr->sin6_addr = in6addr_any;
      addr->sin6_port = htons(port);
      return address;
    }
    default:
      SB_DCHECK(false) << ": unknown address type=" << address_type;
      return {};
  }
}

bool GetLocalhostAddress(int address_type,
                         int port,
                         struct sockaddr_storage* address) {
  if (address_type != AF_INET && address_type != AF_INET6) {
    SB_LOG(ERROR) << __FUNCTION__ << ": unknown address type: " << address_type;
    return false;
  }
  *address = GetUnspecifiedAddress(address_type, port);
  switch (address_type) {
    case AF_INET: {
      struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(address);
      inet_pton(AF_INET, "127.0.0.1", &addr->sin_addr);
      return true;
    }
    case AF_INET6: {
      struct sockaddr_in6* addr =
          reinterpret_cast<struct sockaddr_in6*>(address);
      inet_pton(AF_INET6, "::1", &addr->sin6_addr);
      return true;
    }
    default:
      SB_NOTREACHED();
      return false;
  }
}

bool Socket::IsValid() {
  return socket_ != kInvalidSocketId;
}

int Socket::GetSocket() {
  return socket_;
}

Socket::Socket(int address_type, int protocol) {
  socket_ = ::socket(address_type, protocol, 0);
}

Socket::Socket() : socket_(kInvalidSocketId) {}

Socket::~Socket() {
  if (IsValid()) {
    close(socket_);
  }
}

int Socket::Connect(const struct sockaddr_storage* address) {
  return connect(socket_, reinterpret_cast<const struct sockaddr*>(address),
                 sizeof(*address));
}

int Socket::Bind(const struct sockaddr_storage* local_address) {
  return bind(socket_, reinterpret_cast<const struct sockaddr*>(local_address),
              sizeof(*local_address));
}

int Socket::Listen() {
  return listen(socket_, 1);
}

Socket* Socket::Accept() {
  struct sockaddr_storage address;
  socklen_t len = sizeof(address);
  int new_socket =
      accept(socket_, reinterpret_cast<struct sockaddr*>(&address), &len);
  if (new_socket == -1) {
    return NULL;
  }
  return new Socket(new_socket);
}

bool Socket::IsConnected() {
  // TODO: implement this
  return false;
}

bool Socket::IsConnectedAndIdle() {
  // TODO: implement this
  return false;
}

bool Socket::IsPending() {
  // TODO: implement this
  return false;
}

int Socket::GetLastError() {
  return errno;
}

void Socket::ClearLastError() {
  errno = 0;
}

int Socket::ReceiveFrom(char* out_data,
                        int data_size,
                        struct sockaddr_storage* out_source) {
  socklen_t len = sizeof(*out_source);
  return recvfrom(socket_, out_data, data_size, 0,
                  reinterpret_cast<struct sockaddr*>(out_source), &len);
}

int Socket::SendTo(const char* data,
                   int data_size,
                   const struct sockaddr_storage* destination) {
  return sendto(socket_, data, data_size, 0,
                reinterpret_cast<const struct sockaddr*>(destination),
                sizeof(*destination));
}

bool Socket::SetBroadcast(bool value) {
  int val = value;
  return setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) == 0;
}

bool Socket::SetReuseAddress(bool value) {
  int val = value;
  return setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == 0;
}

bool Socket::SetReceiveBufferSize(int32_t size) {
  return setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == 0;
}

bool Socket::SetSendBufferSize(int32_t size) {
  return setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == 0;
}

bool Socket::SetTcpKeepAlive(bool value, int64_t period) {
  // TODO: implement this
  return false;
}

bool Socket::SetTcpNoDelay(bool value) {
  // TODO: implement this
  return false;
}

bool Socket::SetTcpWindowScaling(bool value) {
  // TODO: implement this
  return false;
}

Socket::Socket(int socket) : socket_(socket) {}

}  // namespace starboard

std::ostream& operator<<(std::ostream& os,
                         const struct sockaddr_storage& address) {
  char host[NI_MAXHOST];
  char service[NI_MAXSERV];
  getnameinfo(reinterpret_cast<const struct sockaddr*>(&address),
              sizeof(address), host, sizeof(host), service, sizeof(service),
              NI_NUMERICHOST | NI_NUMERICSERV);
  if (address.ss_family == AF_INET6) {
    os << "[" << host << "]:" << service;
  } else {
    os << host << ":" << service;
  }
  return os;
}
