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

#include "starboard/common/mutex.h"
#include "starboard/common/optional.h"
#include "starboard/common/socket.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "starboard/time.h"

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
  int result = memcmp(left.address, right.address, bytes_to_compare);
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
  bool IsSocketTracked(SbSocket socket);
  const optional<SbSocketAddress>& GetLocalAddress(SbSocket socket) const;
  void OnListen(SbSocket socket);
  void OnAddWaiter(SbSocket socket, SbSocketWaiter waiter);
  void OnRemoveWaiter(SbSocket socket, SbSocketWaiter waiter);

  enum State {
    kCreated,
    kConnected,
    kListened,
  };

  State GetState(SbSocket socket);

  void LogTrackedSockets();
  std::multimap<SbTimeMonotonic, SbSocket> ComputeIdleTimePerSocketForThreadId(
      SbThreadId thread_id);

 private:
  struct SocketRecord {
    SbSocketAddressType address_type;
    SbSocketProtocol protocol;
    optional<SbSocketAddress> local_address;
    optional<SbSocketAddress> remote_address;
    SbTime last_activity;
    SbSocketWaiter waiter = kSbSocketWaiterInvalid;
    SbThreadId thread_id;
    State state;
  };

  std::string ConvertToString_Locked(SbSocketAddress address) const;
  std::string ConvertToString_Locked(const SocketRecord& record) const;
  static SbTimeMonotonic ComputeTimeIdle(const SocketRecord& record);

  Mutex mutex_;
  std::map<SbSocket, SocketRecord> sockets_;
  std::map<SbSocketAddress, std::string> socket_resolutions_;
};

}  // namespace socket
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_SOCKET_SOCKET_TRACKER_H_
