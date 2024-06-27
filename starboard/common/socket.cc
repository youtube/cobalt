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

#include "sys/socket.h"
#undef socket
#include <string.h>
#include <unistd.h>
#include <iomanip>
#include <map>
#include "netinet/in.h"
#include "netinet/tcp.h"
#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/configuration.h"
#include "starboard/socket.h"

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

Socket::Socket(SbSocketAddressType address_type, SbSocketProtocol protocol) {
#if SB_API_VERSION >= 16
  int family, type, proto;
  if (address_type == kSbSocketAddressTypeIpv4) {
    family = AF_INET;
  } else {
    family = AF_INET6;
  }
  if (protocol == kSbSocketProtocolTcp) {
    type = SOCK_STREAM;
    proto = IPPROTO_TCP;
  } else {
    type = SOCK_DGRAM;
    proto = IPPROTO_UDP;
  }
  socket_ = ::socket(family, type, proto);
#else
  socket_ = SbSocketCreate(address_type, protocol);
#endif  // SB_API_VERSION >= 16
}

Socket::Socket(SbSocketAddressType address_type) {
#if SB_API_VERSION >= 16
  int family, type, proto;
  if (address_type == kSbSocketAddressTypeIpv4) {
    family = AF_INET;
  } else {
    family = AF_INET6;
  }
  type = SOCK_STREAM;
  proto = IPPROTO_TCP;

  socket_ = ::socket(family, type, proto);
#else
  socket_ = SbSocketCreate(address_type, kSbSocketProtocolTcp);
#endif  // SB_API_VERSION >= 16
}

Socket::Socket(SbSocketProtocol protocol) {
#if SB_API_VERSION >= 16
  int family, type, proto;
  family = AF_INET;
  if (protocol == kSbSocketProtocolTcp) {
    type = SOCK_STREAM;
    proto = IPPROTO_TCP;
  } else {
    type = SOCK_DGRAM;
    proto = IPPROTO_UDP;
  }
  socket_ = ::socket(family, type, proto);
#else
  socket_ = SbSocketCreate(kSbSocketAddressTypeIpv4, protocol);
#endif  // SB_API_VERSION >= 16
}

Socket::Socket() {
#if SB_API_VERSION >= 16
  int family, type, proto;
  family = AF_INET;
  type = SOCK_STREAM;
  proto = IPPROTO_TCP;

  socket_ = ::socket(family, type, proto);
#else
  socket_ = SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
#endif  // SB_API_VERSION >= 16
}

Socket::~Socket() {
#if SB_API_VERSION >= 16
  ::close(socket_);
#else
  SbSocketDestroy(socket_);
#endif  // SB_API_VERSION >= 16
}

bool Socket::IsValid() {
#if SB_API_VERSION >= 16
  return socket_ >= 0;
#else
  return SbSocketIsValid(socket_);
#endif  // SB_API_VERSION >= 16
}

SbSocketError Socket::Connect(const SbSocketAddress* address) {
#if SB_API_VERSION >= 16
  // Convert SbSocketAddress tp POSIX sockaddr format
  if (address == nullptr) {
    return kSbSocketErrorFailed;
  }
  // sizeof(sockaddr_in6) = 28
  char saddr[28] = {0};
  socklen_t length;

#if SB_HAS(IPV6)
  if (address->type == kSbSocketAddressTypeIpv6) {
    struct sockaddr_in6* saddr_in6 =
        reinterpret_cast<struct sockaddr_in6*>(&saddr);
    length = sizeof(struct sockaddr_in6);
    saddr_in6->sin6_family = AF_INET6;
    saddr_in6->sin6_port = htons(address->port);
    memcpy(&saddr_in6->sin6_addr, address->address, sizeof(struct in6_addr));
  }
#endif

  if (address->type == kSbSocketAddressTypeIpv4) {
    struct sockaddr_in* saddr_in =
        reinterpret_cast<struct sockaddr_in*>(&saddr);
    length = sizeof(struct sockaddr_in);
    saddr_in->sin_family = AF_INET;
    saddr_in->sin_port = htons(address->port);
    memcpy(&saddr_in->sin_addr, address->address, sizeof(struct in_addr));
  }

  if (::connect(socket_, reinterpret_cast<const struct sockaddr*>(&saddr),
                length) == 0) {
    return kSbSocketOk;
  }
  return kSbSocketErrorFailed;
#else
  return SbSocketConnect(socket_, address);
#endif  // SB_API_VERSION >= 16
}

SbSocketError Socket::Bind(const SbSocketAddress* address) {
#if SB_API_VERSION >= 16
  // Convert SbSocketAddress tp POSIX sockaddr format
  if (address == nullptr) {
    return kSbSocketErrorFailed;
  }
  // sizeof(sockaddr_in6) = 28
  char saddr[28] = {0};
  socklen_t length;

#if SB_HAS(IPV6)
  if (address->type == kSbSocketAddressTypeIpv6) {
    struct sockaddr_in6* saddr_in6 =
        reinterpret_cast<struct sockaddr_in6*>(&saddr);
    length = sizeof(struct sockaddr_in6);
    saddr_in6->sin6_family = AF_INET6;
    saddr_in6->sin6_port = htons(address->port);
    memcpy(&saddr_in6->sin6_addr, address->address, sizeof(struct in6_addr));
  }
#endif  // SB_HAS(IPV6)

  if (address->type == kSbSocketAddressTypeIpv4) {
    struct sockaddr_in* saddr_in =
        reinterpret_cast<struct sockaddr_in*>(&saddr);
    length = sizeof(struct sockaddr_in);
    saddr_in->sin_family = AF_INET;
    saddr_in->sin_port = htons(address->port);
    memcpy(&saddr_in->sin_addr, address->address, sizeof(struct in_addr));
  }

  if (::bind(socket_, reinterpret_cast<const struct sockaddr*>(&saddr),
             length) == 0) {
    return kSbSocketOk;
  }
  return kSbSocketErrorFailed;
#else
  return SbSocketBind(socket_, address);
#endif  // SB_API_VERSION >= 16
}

SbSocketError Socket::Listen() {
#if SB_API_VERSION >= 16
#if defined(SOMAXCONN)
  const int kMaxConn = SOMAXCONN;
#else
  const int kMaxConn = 128;
#endif

  if (::listen(socket_, kMaxConn) == 0) {
    return kSbSocketOk;
  }
  return kSbSocketErrorFailed;
#else
  return SbSocketListen(socket_);
#endif  // SB_API_VERSION >= 16
}

Socket* Socket::Accept() {
#if SB_API_VERSION >= 16
  return new Socket(::accept(socket_, NULL, NULL));
#else
  if (SbSocketIsValid(socket_)) {
    return new Socket(socket_);
  }
  return NULL;
#endif  // SB_API_VERSION >= 16
}

bool Socket::IsConnected() {
#if SB_API_VERSION >= 16
  if (socket_ < 0) {
    return false;
  }
  char c;
  int result = recv(socket_, &c, 1, MSG_PEEK);
  if (result == 0) {
    return false;
  }
  return (result > 0 || errno == EAGAIN || errno == EWOULDBLOCK);
#else
  return SbSocketIsConnected(socket_);
#endif  // SB_API_VERSION >= 16
}

bool Socket::IsConnectedAndIdle() {
#if SB_API_VERSION >= 16
  if (socket_ < 0) {
    return false;
  }
  char c;
  int result = recv(socket_, &c, 1, MSG_PEEK);
  if (result >= 0) {
    return false;
  }
  return (errno == EAGAIN || errno == EWOULDBLOCK);
#else
  return SbSocketIsConnectedAndIdle(socket_);
#endif  // SB_API_VERSION >= 16
}

bool Socket::IsPending() {
#if SB_API_VERSION >= 16
  return errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK;
#else
  return GetLastError() == kSbSocketPending;
#endif  // SB_API_VERSION >= 16
}

#if SB_API_VERSION >= 16
int Socket::GetLastError() {
  return errno;
}
#else
SbSocketError Socket::GetLastError() {
  return SbSocketGetLastError(socket_);
}
#endif  // SB_API_VERSION >= 16

void Socket::ClearLastError() {
#if SB_API_VERSION >= 16
  errno = 0;
#else
  SbSocketClearLastError(socket_);
#endif  // SB_API_VERSION >= 16
}

int Socket::ReceiveFrom(char* out_data,
                        int data_size,
                        SbSocketAddress* out_source) {
#if SB_API_VERSION >= 16
  if (socket_ < 0) {
    errno = EBADF;
    return -1;
  }
  if (out_source) {
    struct sockaddr saddr = {0};
    socklen_t length = sizeof(saddr);
    int result = getpeername(socket_, &saddr, &length);
    if (result < 0) {
      SB_LOG(ERROR) << __FUNCTION__
                    << ": getpeername failed, errno = " << errno;
      return -1;
    }
    // Transfer address from sockaddr to SbSocketAddress
    if (saddr.sa_family == AF_INET) {
      struct sockaddr_in* addr_in = (struct sockaddr_in*)&saddr;
      out_source->type = kSbSocketAddressTypeIpv4;
      out_source->port = addr_in->sin_port;
      memcpy(&out_source->address, &addr_in->sin_addr, sizeof(struct in_addr));
    }
#if SB_HAS(IPV6)
    if (saddr.sa_family == AF_INET6) {
      struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)&saddr;
      out_source->type = kSbSocketAddressTypeIpv6;
      out_source->port = addr_in6->sin6_port;
      memcpy(&out_source->address, &addr_in6->sin6_addr,
             sizeof(struct in6_addr));
    }
#endif
  }

  int kRecvFlags = 0;
  ssize_t bytes_read = recv(socket_, out_data, data_size, kRecvFlags);
  if (bytes_read >= 0) {
    return static_cast<int>(bytes_read);
  }

  return -1;
#else
  return SbSocketReceiveFrom(socket_, out_data, data_size, out_source);
#endif  // SB_API_VERSION >= 16
}

int Socket::SendTo(const char* data,
                   int data_size,
                   const SbSocketAddress* destination) {
#if SB_API_VERSION >= 16

#if defined(MSG_NOSIGNAL)
  const int kSendFlags = MSG_NOSIGNAL;
#else
  const int kSendFlags = 0;
#endif
  if ((socket_ < 0) || (destination)) {
    errno = EBADF;
    return -1;
  }
  ssize_t bytes_written = send(socket_, data, data_size, kSendFlags);
  if (bytes_written >= 0) {
    return static_cast<int>(bytes_written);
  }
  return -1;
#else
  return SbSocketSendTo(socket_, data, data_size, destination);
#endif  // SB_API_VERSION >= 16
}

bool Socket::GetLocalAddress(SbSocketAddress* out_address) {
#if SB_API_VERSION >= 16
  if (socket_ < 0) {
    errno = EBADF;
    return false;
  }
  struct sockaddr saddr = {0};
  socklen_t length;
  if (getsockname(socket_, &saddr, &length) != 0) {
    return false;
  }
  if (out_address) {
    // Transfer address from sockaddr to SbSocketAddress
    if (saddr.sa_family == AF_INET) {
      struct sockaddr_in* addr_in = (struct sockaddr_in*)&saddr;
      out_address->type = kSbSocketAddressTypeIpv4;
      out_address->port = addr_in->sin_port;
      memcpy(&out_address->address, &addr_in->sin_addr, sizeof(struct in_addr));
    }
#if SB_HAS(IPV6)
    if (saddr.sa_family == AF_INET6) {
      struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)&saddr;
      out_address->type = kSbSocketAddressTypeIpv6;
      out_address->port = addr_in6->sin6_port;
      memcpy(&out_address->address, &addr_in6->sin6_addr,
             sizeof(struct in6_addr));
    }
#endif
  }
#else
  return SbSocketGetLocalAddress(socket_, out_address);
#endif  // SB_API_VERSION >= 16
}

bool Socket::SetBroadcast(bool value) {
#if SB_API_VERSION >= 16
  int true_val = value ? 1 : 0;
  if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &true_val, value) == 0) {
    return true;
  }
  return false;
#else
  return SbSocketSetBroadcast(socket_, value);
#endif  // SB_API_VERSION >= 16
}

bool Socket::SetReuseAddress(bool value) {
#if SB_API_VERSION >= 16
  const int on = value ? 1 : 0;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == 0) {
    return true;
  }
  return false;
#else
  return SbSocketSetReuseAddress(socket_, value);
#endif  // SB_API_VERSION >= 16
}

bool Socket::SetReceiveBufferSize(int32_t size) {
#if SB_API_VERSION >= 16
  if (setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                 reinterpret_cast<const char*>(&size), sizeof(size)) == 0) {
    return true;
  }
  return false;
#else
  return SbSocketSetReceiveBufferSize(socket_, size);
#endif  // SB_API_VERSION >= 16
}

bool Socket::SetSendBufferSize(int32_t size) {
#if SB_API_VERSION >= 16
  if (setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                 reinterpret_cast<const char*>(&size), sizeof(size)) == 0) {
    return true;
  }
  return false;
#else
  return SbSocketSetSendBufferSize(socket_, size);
#endif  // SB_API_VERSION >= 16
}

bool Socket::SetTcpKeepAlive(bool value, int64_t period) {
#if SB_API_VERSION >= 16
  const int on = value ? 1 : 0;
  if (setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&on), sizeof(on)) != 0) {
    return false;
  }
  if (value) {
    int result = 0;
    result |= setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

#if defined(__APPLE__)
    // In tvOS, TCP_KEEPIDLE and SOL_TCP are not available.
    // For reference:
    // https://stackoverflow.com/questions/15860127/how-to-configure-tcp-keepalive-under-mac-os-x
    result |= setsockopt(socket_, IPPROTO_TCP, TCP_KEEPALIVE, &period,
                         sizeof(period));
#elif !defined(_WIN32)
    // In Windows, the SOL_TCP and TCP_KEEPIDLE options are not available.
    // For reference:
    // https://stackoverflow.com/questions/8176821/how-to-set-the-keep-alive-interval-for-winsock
    result |=
        setsockopt(socket_, SOL_TCP, TCP_KEEPIDLE, &period, sizeof(period));
    result |=
        setsockopt(socket_, SOL_TCP, TCP_KEEPINTVL, &period, sizeof(period));
#endif
    if (result != 0) {
      return false;
    }
  }
  return true;
#else
  return SbSocketSetTcpKeepAlive(socket_, value, period);
#endif  // SB_API_VERSION >= 16
}

bool Socket::SetTcpNoDelay(bool value) {
#if SB_API_VERSION >= 16
  const int on = value ? 1 : 0;
  if (setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&on), sizeof(on)) == 0) {
    return true;
  }
  return false;
#else
  return SbSocketSetTcpNoDelay(socket_, value);
#endif  // SB_API_VERSION >= 16
}

bool Socket::SetTcpWindowScaling(bool value) {
#if SB_API_VERSION >= 16
  // This API is not supported in standard POSIX.
  return true;
#else
  return SbSocketSetTcpWindowScaling(socket_, value);
#endif  // SB_API_VERSION >= 16
}

bool Socket::JoinMulticastGroup(const SbSocketAddress* address) {
#if SB_API_VERSION >= 16
  struct ip_mreq imreq = {0};
  struct sockaddr saddr = {0};
  socklen_t length = sizeof(saddr);
  in_addr_t in_addr = *reinterpret_cast<const in_addr_t*>(&saddr);
  imreq.imr_multiaddr.s_addr = in_addr;
  imreq.imr_interface.s_addr = INADDR_ANY;

  if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq,
                 sizeof(imreq))) {
    return false;
  }
  return true;
#else
  return SbSocketJoinMulticastGroup(socket_, address);
#endif  // SB_API_VERSION >= 16
}

#if SB_API_VERSION >= 16

int Socket::socket() {
  return socket_;
}
Socket::Socket(int socket) : socket_(socket) {}

#else

SbSocket Socket::socket() {
  return socket_;
}

Socket::Socket(SbSocket socket) : socket_(socket) {}

#endif  // SB_API_VERSION >= 16

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
