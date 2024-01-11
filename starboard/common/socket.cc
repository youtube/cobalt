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

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iomanip>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/shared/modular/posix_socket_wrappers.h"

namespace starboard {

SbSocketAddress GetUnspecifiedAddress(SbSocketAddressType address_type,
                                      int port) {
  SbSocketAddress address = {};
  address.type = address_type;
  address.port = port;
  return address;
}

bool GetLocalhostAddress(SbSocketAddressType address_type,
                         int port,
                         SbSocketAddress* address) {
  if (address_type != kSbSocketAddressTypeIpv4 &&
      address_type != kSbSocketAddressTypeIpv6) {
    SB_LOG(ERROR) << __FUNCTION__ << ": unknown address type: " << address_type;
    return false;
  }
  *address = GetUnspecifiedAddress(address_type, port);
  switch (address_type) {
    case kSbSocketAddressTypeIpv4:
      address->address[0] = 127;
      address->address[3] = 1;
      break;
    case kSbSocketAddressTypeIpv6:
      address->address[15] = 1;
      break;
  }

  return true;
}

Socket::Socket(SbSocketAddressType address_type, SbSocketProtocol protocol)
#if SB_API_VERSION < 16
    : socket_(SbSocketCreate(address_type, protocol)) {
}
#else
{
  int socket_fd;
  int socket_type;
  int socket_protocol;
  if (protocol == kSbSocketProtocolTcp) {
    socket_type = SOCK_STREAM;
    socket_protocol = IPPROTO_TCP;
  } else {
    socket_type = SOCK_DGRAM;
    socket_protocol = IPPROTO_UDP;
  }
  socket_fd = ::socket(ConvertSocketAddressTypeToDomain(address_type),
                       socket_type, socket_protocol);

  if (socket_fd < 0) {
    socket_ = kSbSocketInvalid;
  } else if (!SocketSetNonBlocking(socket_fd)) {
    // Something went wrong, we'll clean up (preserving errno) and return
    // failure.
    int save_errno = errno;
    HANDLE_EINTR_WRAPPER(close(socket_fd));
    errno = save_errno;
    socket_ = kSbSocketInvalid;
  } else {
    socket_ = new SbSocketWrapper(address_type, protocol, socket_fd);
  }
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  // Use SO_NOSIGPIPE to mute SIGPIPE on darwin systems.
  int optval_set = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE,
             reinterpret_cast<void*>(&optval_set), sizeof(int));
#endif
}
#endif  // SB_API_VERSION < 16

Socket::Socket(SbSocketAddressType address_type)
#if SB_API_VERSION < 16
    : socket_(SbSocketCreate(address_type, kSbSocketProtocolTcp)) {
}
#else
{
  int socket_fd;
  int socket_type;
  int socket_protocol;
  socket_type = SOCK_STREAM;
  socket_protocol = IPPROTO_TCP;

  socket_fd = ::socket(ConvertSocketAddressTypeToDomain(address_type),
                       socket_type, socket_protocol);

  if (socket_fd < 0) {
    socket_ = kSbSocketInvalid;
  } else if (!SocketSetNonBlocking(socket_fd)) {
    // Something went wrong, we'll clean up (preserving errno) and return
    // failure.
    int save_errno = errno;
    HANDLE_EINTR_WRAPPER(close(socket_fd));
    errno = save_errno;
    socket_ = kSbSocketInvalid;
  } else {
    socket_ =
        new SbSocketWrapper(address_type, kSbSocketProtocolTcp, socket_fd);
  }
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  // Use SO_NOSIGPIPE to mute SIGPIPE on darwin systems.
  int optval_set = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE,
             reinterpret_cast<void*>(&optval_set), sizeof(int));
#endif
}
#endif  // SB_API_VERSION < 16

Socket::Socket(SbSocketProtocol protocol)
#if SB_API_VERSION < 16
    : socket_(SbSocketCreate(kSbSocketAddressTypeIpv4, protocol)) {
}
#else
{
  int socket_fd;
  int socket_type;
  int socket_protocol;
  if (protocol == kSbSocketProtocolTcp) {
    socket_type = SOCK_STREAM;
    socket_protocol = IPPROTO_TCP;
  } else {
    socket_type = SOCK_DGRAM;
    socket_protocol = IPPROTO_UDP;
  }

  socket_fd = ::socket(AF_INET, socket_type, socket_protocol);

  if (socket_fd < 0) {
    socket_ = kSbSocketInvalid;
  } else if (!SocketSetNonBlocking(socket_fd)) {
    // Something went wrong, we'll clean up (preserving errno) and return
    // failure.
    int save_errno = errno;
    HANDLE_EINTR_WRAPPER(close(socket_fd));
    errno = save_errno;
    socket_ = kSbSocketInvalid;
  } else {
    socket_ = new SbSocketWrapper(kSbSocketAddressTypeIpv4,
                                  kSbSocketProtocolTcp, socket_fd);
  }
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  // Use SO_NOSIGPIPE to mute SIGPIPE on darwin systems.
  int optval_set = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE,
             reinterpret_cast<void*>(&optval_set), sizeof(int));
#endif
}
#endif  // SB_API_VERSION < 16

Socket::Socket()
#if SB_API_VERSION < 16
    : socket_(SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp)) {
}
#else
{
  int socket_fd;
  int socket_type;
  int socket_protocol;
  socket_type = SOCK_STREAM;
  socket_protocol = IPPROTO_TCP;

  socket_fd = ::socket(AF_INET, socket_type, socket_protocol);

  if (socket_fd < 0) {
    socket_ = kSbSocketInvalid;
  } else if (!SocketSetNonBlocking(socket_fd)) {
    // Something went wrong, we'll clean up (preserving errno) and return
    // failure.
    int save_errno = errno;
    HANDLE_EINTR_WRAPPER(close(socket_fd));
    errno = save_errno;
    socket_ = kSbSocketInvalid;
  } else {
    socket_ = new SbSocketWrapper(kSbSocketAddressTypeIpv4,
                                  kSbSocketProtocolTcp, socket_fd);
  }
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  // Use SO_NOSIGPIPE to mute SIGPIPE on darwin systems.
  int optval_set = 1;
  setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE,
             reinterpret_cast<void*>(&optval_set), sizeof(int));
#endif
}
#endif  // SB_API_VERSION < 16

Socket::~Socket() {
  SbSocketDestroy(socket_);
}

bool Socket::IsValid() {
  return SbSocketIsValid(socket_);
}

SbSocketError Socket::Connect(const SbSocketAddress* address) {
  return SbSocketConnect(socket_, address);
}

SbSocketError Socket::Bind(const SbSocketAddress* local_address) {
  return SbSocketBind(socket_, local_address);
}

SbSocketError Socket::Listen() {
  return SbSocketListen(socket_);
}

Socket* Socket::Accept() {
  SbSocket accepted_socket = SbSocketAccept(socket_);
  if (SbSocketIsValid(accepted_socket)) {
    return new Socket(accepted_socket);
  }

  return NULL;
}

bool Socket::IsConnected() {
  return SbSocketIsConnected(socket_);
}

bool Socket::IsConnectedAndIdle() {
  return SbSocketIsConnectedAndIdle(socket_);
}

bool Socket::IsPending() {
  return GetLastError() == kSbSocketPending;
}

SbSocketError Socket::GetLastError() {
#if SB_API_VERSION < 16
  return SbSocketGetLastError(socket_);
#else
  return socket_->error;
#endif
}

void Socket::ClearLastError() {
#if SB_API_VERSION < 16
  SbSocketClearLastError(socket_);
#else
  socket_->socket_fd = kSbSocketOk;
#endif
}

int Socket::ReceiveFrom(char* out_data,
                        int data_size,
                        SbSocketAddress* out_source) {
  return SbSocketReceiveFrom(socket_, out_data, data_size, out_source);
}

int Socket::SendTo(const char* data,
                   int data_size,
                   const SbSocketAddress* destination) {
  return SbSocketSendTo(socket_, data, data_size, destination);
}

bool Socket::GetLocalAddress(SbSocketAddress* out_address) {
  return SbSocketGetLocalAddress(socket_, out_address);
}

bool Socket::SetBroadcast(bool value) {
  return SbSocketSetBroadcast(socket_, value);
}

bool Socket::SetReuseAddress(bool value) {
  return SbSocketSetReuseAddress(socket_, value);
}

bool Socket::SetReceiveBufferSize(int32_t size) {
  return SbSocketSetReceiveBufferSize(socket_, size);
}

bool Socket::SetSendBufferSize(int32_t size) {
  return SbSocketSetSendBufferSize(socket_, size);
}

bool Socket::SetTcpKeepAlive(bool value, SbTime period) {
  return SbSocketSetTcpKeepAlive(socket_, value, period);
}

bool Socket::SetTcpNoDelay(bool value) {
  return SbSocketSetTcpNoDelay(socket_, value);
}

bool Socket::SetTcpWindowScaling(bool value) {
  return SbSocketSetTcpWindowScaling(socket_, value);
}

bool Socket::JoinMulticastGroup(const SbSocketAddress* address) {
  return SbSocketJoinMulticastGroup(socket_, address);
}

SbSocket Socket::socket() {
  return socket_;
}

Socket::Socket(SbSocket socket) : socket_(socket) {}

}  // namespace starboard

std::ostream& operator<<(std::ostream& os, const SbSocketAddress& address) {
  if (address.type == kSbSocketAddressTypeIpv6) {
    os << std::hex << "[";
    const uint16_t* fields = reinterpret_cast<const uint16_t*>(address.address);
    int i = 0;
    while (fields[i] == 0) {
      if (i == 0) {
        os << ":";
      }
      ++i;
      if (i == 8) {
        os << ":";
      }
    }
    for (; i < 8; ++i) {
      if (i != 0) {
        os << ":";
      }
#if SB_IS(LITTLE_ENDIAN)
      const uint8_t* fields8 = reinterpret_cast<const uint8_t*>(&fields[i]);
      const uint16_t value = static_cast<uint16_t>(fields8[0]) * 256 |
                             static_cast<uint16_t>(fields8[1]);
      os << value;
#else
      os << fields[i];
#endif
    }
    os << "]" << std::dec;
  } else {
    for (int i = 0; i < 4; ++i) {
      if (i != 0) {
        os << ".";
      }
      os << static_cast<int>(address.address[i]);
    }
  }
  os << ":" << address.port;
  return os;
}
