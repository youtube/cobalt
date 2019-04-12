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

// Module Overview: Starboard Socket module C++ convenience layer
//
// Implements a convenience class that builds on top of the core Starboard
// socket functions.

#ifndef STARBOARD_COMMON_SOCKET_H_
#define STARBOARD_COMMON_SOCKET_H_

#include "starboard/configuration.h"
#include "starboard/socket.h"
#include "starboard/types.h"

#ifdef __cplusplus

extern "C++" {

#include <iomanip>
#include <iostream>

// An inline C++ wrapper to SbSocket.
class Socket {
 public:
  Socket(SbSocketAddressType address_type, SbSocketProtocol protocol)
      : socket_(SbSocketCreate(address_type, protocol)) {}
  explicit Socket(SbSocketAddressType address_type)
      : socket_(SbSocketCreate(address_type, kSbSocketProtocolTcp)) {}
  explicit Socket(SbSocketProtocol protocol)
      : socket_(SbSocketCreate(kSbSocketAddressTypeIpv4, protocol)) {}
  Socket()
      : socket_(
            SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp)) {}
  ~Socket() { SbSocketDestroy(socket_); }
  bool IsValid() { return SbSocketIsValid(socket_); }

  SbSocketError Connect(const SbSocketAddress* address) {
    return SbSocketConnect(socket_, address);
  }
  SbSocketError Bind(const SbSocketAddress* local_address) {
    return SbSocketBind(socket_, local_address);
  }
  SbSocketError Listen() { return SbSocketListen(socket_); }
  Socket* Accept() {
    SbSocket accepted_socket = SbSocketAccept(socket_);
    if (SbSocketIsValid(accepted_socket)) {
      return new Socket(accepted_socket);
    }

    return NULL;
  }

  bool IsConnected() { return SbSocketIsConnected(socket_); }
  bool IsConnectedAndIdle() { return SbSocketIsConnectedAndIdle(socket_); }

  bool IsPending() { return GetLastError() == kSbSocketPending; }
  SbSocketError GetLastError() { return SbSocketGetLastError(socket_); }
  void ClearLastError() { SbSocketClearLastError(socket_); }

  int ReceiveFrom(char* out_data, int data_size, SbSocketAddress* out_source) {
    return SbSocketReceiveFrom(socket_, out_data, data_size, out_source);
  }

  int SendTo(const char* data,
             int data_size,
             const SbSocketAddress* destination) {
    return SbSocketSendTo(socket_, data, data_size, destination);
  }

  bool GetLocalAddress(SbSocketAddress* out_address) {
    return SbSocketGetLocalAddress(socket_, out_address);
  }

  bool SetBroadcast(bool value) { return SbSocketSetBroadcast(socket_, value); }

  bool SetReuseAddress(bool value) {
    return SbSocketSetReuseAddress(socket_, value);
  }

  bool SetReceiveBufferSize(int32_t size) {
    return SbSocketSetReceiveBufferSize(socket_, size);
  }

  bool SetSendBufferSize(int32_t size) {
    return SbSocketSetSendBufferSize(socket_, size);
  }

  bool SetTcpKeepAlive(bool value, SbTime period) {
    return SbSocketSetTcpKeepAlive(socket_, value, period);
  }

  bool SetTcpNoDelay(bool value) {
    return SbSocketSetTcpNoDelay(socket_, value);
  }

  bool SetTcpWindowScaling(bool value) {
    return SbSocketSetTcpWindowScaling(socket_, value);
  }

  bool JoinMulticastGroup(const SbSocketAddress* address) {
    return SbSocketJoinMulticastGroup(socket_, address);
  }

  SbSocket socket() { return socket_; }

 private:
  explicit Socket(SbSocket socket) : socket_(socket) {}

  SbSocket socket_;
};

// Let SbSocketAddresses be output to log streams.
inline std::ostream& operator<<(std::ostream& os,
                                const SbSocketAddress& address) {
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

}  // extern "C++"

#endif

#endif  // STARBOARD_COMMON_SOCKET_H_
