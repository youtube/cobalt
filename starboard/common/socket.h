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

#include <ostream>

#include "starboard/socket.h"
#include "starboard/types.h"

namespace starboard {

// Returns an IP unspecified address with the given port.
SbSocketAddress GetUnspecifiedAddress(SbSocketAddressType address_type,
                                      int port);

// Gets an IP localhost address with the given port.
// Returns true if it was successful.
bool GetLocalhostAddress(SbSocketAddressType address_type,
                         int port,
                         SbSocketAddress* address);

class Socket {
 public:
  Socket(SbSocketAddressType address_type, SbSocketProtocol protocol);
  explicit Socket(SbSocketAddressType address_type);
  explicit Socket(SbSocketProtocol protocol);
  Socket();
  ~Socket();
  bool IsValid();

  SbSocketError Connect(const SbSocketAddress* address);
  SbSocketError Bind(const SbSocketAddress* local_address);
  SbSocketError Listen();
  Socket* Accept();

  bool IsConnected();
  bool IsConnectedAndIdle();
  bool IsPending();

  SbSocketError GetLastError();
  void ClearLastError();

  int ReceiveFrom(char* out_data, int data_size, SbSocketAddress* out_source);
  int SendTo(const char* data,
             int data_size,
             const SbSocketAddress* destination);

  bool GetLocalAddress(SbSocketAddress* out_address);
  bool SetBroadcast(bool value);
  bool SetReuseAddress(bool value);
  bool SetReceiveBufferSize(int32_t size);
  bool SetSendBufferSize(int32_t size);
  bool SetTcpKeepAlive(bool value, SbTime period);
  bool SetTcpNoDelay(bool value);
  bool SetTcpWindowScaling(bool value);
  bool JoinMulticastGroup(const SbSocketAddress* address);

  SbSocket socket();

 private:
  explicit Socket(SbSocket socket);

  SbSocket socket_;
};

}  // namespace starboard

// Let SbSocketAddresses be output to log streams.
std::ostream& operator<<(std::ostream& os, const SbSocketAddress& address);

#endif  // STARBOARD_COMMON_SOCKET_H_
