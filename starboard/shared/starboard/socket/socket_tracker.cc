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

#include "starboard/shared/starboard/socket/socket_tracker.h"

#include <sstream>

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace socket {

#if SB_ENABLE_SOCKET_TRACKER

// static
SocketTracker* SocketTracker::GetInstance() {
  static SocketTracker s_socket_tracker;
  return &s_socket_tracker;
}

void SocketTracker::OnCreate(SbSocket socket,
                             SbSocketAddressType address_type,
                             SbSocketProtocol protocol) {
  SocketRecord record;
  record.address_type = address_type;
  record.protocol = protocol;
  record.last_activity_ = SbTimeGetMonotonicNow();

  ScopedLock scoped_lock(mutex_);
  sockets_[socket] = record;
}

void SocketTracker::OnDestroy(SbSocket socket) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());
  sockets_.erase(iter);
}

void SocketTracker::OnConnect(SbSocket socket,
                              const SbSocketAddress& remote_address) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());
  iter->second.remote_address = remote_address;
}

void SocketTracker::OnBind(SbSocket socket,
                           const SbSocketAddress& local_address) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());
  iter->second.local_address = local_address;
}

void SocketTracker::OnAccept(SbSocket listening_socket,
                             SbSocket accepted_socket) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(listening_socket);
  SB_DCHECK(iter != sockets_.end());

  iter->second.last_activity_ = SbTimeGetMonotonicNow();

  SocketRecord record;
  record.address_type = iter->second.address_type;
  record.protocol = iter->second.protocol;
  record.last_activity_ = iter->second.last_activity_;
  record.local_address = iter->second.local_address;

  sockets_[accepted_socket] = record;
}

void SocketTracker::OnReceiveFrom(SbSocket socket,
                                  const SbSocketAddress* source) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());

  iter->second.last_activity_ = SbTimeGetMonotonicNow();
  if (source) {
    iter->second.remote_address = *source;
  }
}

void SocketTracker::OnSendTo(SbSocket socket,
                             const SbSocketAddress* destination) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());

  iter->second.last_activity_ = SbTimeGetMonotonicNow();
  if (destination) {
    iter->second.remote_address = *destination;
  }
}

void SocketTracker::OnResolve(const char* hostname,
                              const SbSocketResolution& resolution) {
  ScopedLock scoped_lock(mutex_);
  for (int i = 0; i < resolution.address_count; ++i) {
    socket_resolutions_[resolution.addresses[i]] = hostname;
  }
}

void SocketTracker::LogTrackedSockets() {
  ScopedLock scoped_lock(mutex_);
  SB_LOG(INFO) << "Total tracked sockets: " << sockets_.size();
  for (auto iter : sockets_) {
    SB_LOG(INFO) << ConvertToString_Locked(iter.second);
  }
}

std::string SocketTracker::ConvertToString_Locked(
    SbSocketAddress address) const {
  mutex_.DCheckAcquired();

  std::stringstream ss;
  ss << address;

  address.port = 0;
  auto iter = socket_resolutions_.find(address);
  if (iter != socket_resolutions_.end()) {
    ss << "(" << iter->second << ")";
  }
  return ss.str();
}

std::string SocketTracker::ConvertToString_Locked(
    const SocketRecord& record) const {
  mutex_.DCheckAcquired();

  std::stringstream ss;
  ss << (record.protocol == kSbSocketProtocolTcp ? "TCP/" : "UDP/");
  ss << (record.address_type == kSbSocketAddressTypeIpv4 ? "Ipv4" : "Ipv6");
  if (record.local_address) {
    ss << " local addr: " << ConvertToString_Locked(*record.local_address);
  }
  if (record.remote_address) {
    ss << " remote addr: " << ConvertToString_Locked(*record.remote_address);
  }
  ss << " has been idle for "
     << (SbTimeGetMonotonicNow() - record.last_activity_) / kSbTimeSecond
     << " seconds";
  return ss.str();
}

#endif  // SB_ENABLE_SOCKET_TRACKER

}  // namespace socket
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
