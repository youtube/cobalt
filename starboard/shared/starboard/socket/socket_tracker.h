// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_SOCKET_SOCKET_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_SOCKET_SOCKET_TRACKER_H_

#include <map>
#include <string>
#include <vector>

#include "starboard/common/optional.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/socket.h"
#include "starboard/time.h"

#if !defined(COBALT_BUILD_TYPE_GOLD)
#define SB_ENABLE_SOCKET_TRACKER 1
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

// TODO: Move this to starboard/socket.h.
inline bool operator<(const SbSocketAddress& left,
                      const SbSocketAddress& right) {
  if (left.type < right.type) {
    return true;
  }
  if (left.type > right.type) {
    return false;
  }

  int bytes_to_compare = left.type == kSbSocketAddressTypeIpv4 ? 4 : 16;
  int result = SbMemoryCompare(left.address, right.address, bytes_to_compare);
  if (result < 0) {
    return true;
  }
  if (result > 0) {
    return false;
  }

  return left.port < right.port;
}

namespace starboard {
namespace shared {
namespace starboard {
namespace socket {

#if SB_ENABLE_SOCKET_TRACKER

class SocketTracker {
 public:
  static SocketTracker* GetInstance();

  void OnCreate(SbSocket socket,
                SbSocketAddressType address_type,
                SbSocketProtocol protocol);
  void OnDestroy(SbSocket socket);
  void OnConnect(SbSocket socket, const SbSocketAddress& remote_address);
  void OnBind(SbSocket socket, const SbSocketAddress& local_address);
  void OnAccept(SbSocket listening_socket, SbSocket accepted_socket);
  void OnReceiveFrom(SbSocket socket, const SbSocketAddress* source);
  void OnSendTo(SbSocket socket, const SbSocketAddress* destination);
  void OnResolve(const char* hostname, const SbSocketResolution& resolution);

  void LogTrackedSockets();

 private:
  struct SocketRecord {
    SbSocketAddressType address_type;
    SbSocketProtocol protocol;
    optional<SbSocketAddress> local_address;
    optional<SbSocketAddress> remote_address;
    SbTime last_activity_;
  };

  std::string ConvertToString_Locked(SbSocketAddress address) const;
  std::string ConvertToString_Locked(const SocketRecord& record) const;

  Mutex mutex_;
  std::map<SbSocket, SocketRecord> sockets_;
  std::map<SbSocketAddress, std::string> socket_resolutions_;
};

#else  // SB_ENABLE_SOCKET_TRACKER

class SocketTracker {
 public:
  static SocketTracker* GetInstance() {
    static SocketTracker s_socket_tracker;
    return &s_socket_tracker;
  }
  void OnCreate(SbSocket socket,
                SbSocketAddressType address_type,
                SbSocketProtocol protocol) {
    SB_UNREFERENCED_PARAMETER(socket);
    SB_UNREFERENCED_PARAMETER(address_type);
    SB_UNREFERENCED_PARAMETER(protocol);
  }
  void OnDestroy(SbSocket socket) { SB_UNREFERENCED_PARAMETER(socket); }
  void OnConnect(SbSocket socket, const SbSocketAddress& remote_address) {
    SB_UNREFERENCED_PARAMETER(socket);
    SB_UNREFERENCED_PARAMETER(remote_address);
  }
  void OnBind(SbSocket socket, const SbSocketAddress& local_address) {
    SB_UNREFERENCED_PARAMETER(socket);
    SB_UNREFERENCED_PARAMETER(local_address);
  }
  void OnAccept(SbSocket listening_socket, SbSocket accepted_socket) {
    SB_UNREFERENCED_PARAMETER(listening_socket);
    SB_UNREFERENCED_PARAMETER(accepted_socket);
  }
  void OnReceiveFrom(SbSocket socket, const SbSocketAddress* source) {
    SB_UNREFERENCED_PARAMETER(socket);
    SB_UNREFERENCED_PARAMETER(source);
  }
  void OnSendTo(SbSocket socket, const SbSocketAddress* destination) {
    SB_UNREFERENCED_PARAMETER(socket);
    SB_UNREFERENCED_PARAMETER(destination);
  }
  void OnResolve(const char* hostname, const SbSocketResolution& resolution) {
    SB_UNREFERENCED_PARAMETER(hostname);
    SB_UNREFERENCED_PARAMETER(resolution);
  }

  void LogTrackedSockets() {}
};

#endif  // SB_ENABLE_SOCKET_TRACKER

}  // namespace socket
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_SOCKET_SOCKET_TRACKER_H_
