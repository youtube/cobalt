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

#include <netinet/in.h>
#include <sys/socket.h>
#include "starboard/types.h"

namespace starboard {

// Returns an IP unspecified address with the given port.
struct sockaddr_storage GetUnspecifiedAddress(int address_type, int port);

// Gets an IP localhost address with the given port.
// Returns true if it was successful.
bool GetLocalhostAddress(int address_type,
                         int port,
                         struct sockaddr_storage* address);

class Socket {
 public:
  Socket(int address_type, int protocol);
  Socket();
  ~Socket();
  bool IsValid();

  int Connect(const struct sockaddr_storage* address);
  int Bind(const struct sockaddr_storage* local_address);
  int Listen();
  Socket* Accept();

  bool IsConnected();
  bool IsConnectedAndIdle();
  bool IsPending();

  int GetLastError();
  void ClearLastError();

  int ReceiveFrom(char* out_data,
                  int data_size,
                  struct sockaddr_storage* out_source);
  int SendTo(const char* data,
             int data_size,
             const struct sockaddr_storage* destination);

  bool SetBroadcast(bool value);
  bool SetReuseAddress(bool value);
  bool SetReceiveBufferSize(int32_t size);
  bool SetSendBufferSize(int32_t size);
  bool SetTcpKeepAlive(bool value, int64_t period);  // period in microseconds.
  bool SetTcpNoDelay(bool value);
  bool SetTcpWindowScaling(bool value);

  int GetSocket();

 private:
  explicit Socket(int socket);

  int socket_;
};

}  // namespace starboard

// Let struct sockaddr_storages be output to log streams.
std::ostream& operator<<(std::ostream& os,
                         const struct sockaddr_storage& address);

#endif  // STARBOARD_COMMON_SOCKET_H_
