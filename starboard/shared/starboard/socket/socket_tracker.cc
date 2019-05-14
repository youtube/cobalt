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
#include <utility>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace socket {

// static
SocketTracker* SocketTracker::GetInstance() {
  static SocketTracker s_socket_tracker;
  return &s_socket_tracker;
}

void SocketTracker::OnCreate(SbSocket socket,
                             SbSocketAddressType address_type,
                             SbSocketProtocol protocol) {
  SocketRecord record;
  record.thread_id = SbThreadGetId();
  record.address_type = address_type;
  record.protocol = protocol;
  record.last_activity = SbTimeGetMonotonicNow();
  record.state = kCreated;

  ScopedLock scoped_lock(mutex_);
  sockets_[socket] = record;
}

void SocketTracker::OnDestroy(SbSocket socket) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  if (iter == sockets_.end()) {
    return;
  }
  sockets_.erase(iter);
}

void SocketTracker::OnConnect(SbSocket socket,
                              const SbSocketAddress& remote_address) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());
  iter->second.remote_address = remote_address;
  SB_DCHECK(iter->second.state == kCreated);
  iter->second.state = kConnected;
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

  iter->second.last_activity = SbTimeGetMonotonicNow();

  SocketRecord record;
  record.thread_id = SbThreadGetId();
  record.address_type = iter->second.address_type;
  record.protocol = iter->second.protocol;
  record.last_activity = iter->second.last_activity;
  record.local_address = iter->second.local_address;
  record.state = kConnected;

  sockets_[accepted_socket] = record;
}

void SocketTracker::OnReceiveFrom(SbSocket socket,
                                  const SbSocketAddress* source) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());

  iter->second.last_activity = SbTimeGetMonotonicNow();
  if (source) {
    iter->second.remote_address = *source;
  }
}

void SocketTracker::OnSendTo(SbSocket socket,
                             const SbSocketAddress* destination) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());

  iter->second.last_activity = SbTimeGetMonotonicNow();
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

bool SocketTracker::IsSocketTracked(SbSocket socket) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  return iter != sockets_.end();
}

const optional<SbSocketAddress>& SocketTracker::GetLocalAddress(
    SbSocket socket) const {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());
  return iter->second.local_address;
}

void SocketTracker::OnListen(SbSocket socket) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());

  iter->second.last_activity = SbTimeGetMonotonicNow();
  SB_DCHECK(iter->second.state == kCreated);
  iter->second.state = kListened;
}

SocketTracker::State SocketTracker::GetState(SbSocket socket) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());
  return iter->second.state;
}

void SocketTracker::OnAddWaiter(SbSocket socket, SbSocketWaiter waiter) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());

  iter->second.last_activity = SbTimeGetMonotonicNow();
  SB_DCHECK(!SbSocketWaiterIsValid(iter->second.waiter));
  SB_DCHECK(SbSocketWaiterIsValid(waiter));
  iter->second.waiter = waiter;
}

void SocketTracker::OnRemoveWaiter(SbSocket socket, SbSocketWaiter waiter) {
  ScopedLock scoped_lock(mutex_);
  auto iter = sockets_.find(socket);
  SB_DCHECK(iter != sockets_.end());

  iter->second.last_activity = SbTimeGetMonotonicNow();
  SB_DCHECK(waiter == iter->second.waiter);
  SB_DCHECK(SbSocketWaiterIsValid(waiter));
  iter->second.waiter = kSbSocketWaiterInvalid;
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

std::multimap<SbTimeMonotonic, SbSocket>
SocketTracker::ComputeIdleTimePerSocketForThreadId(SbThreadId thread_id) {
  std::multimap<SbTimeMonotonic, SbSocket> idle_times_to_sockets;
  for (auto it = sockets_.begin(); it != sockets_.end(); ++it) {
    if (it->second.thread_id != thread_id || it->second.state == kListened) {
      continue;
    }
    SbTimeMonotonic time_idle = ComputeTimeIdle(it->second);
    idle_times_to_sockets.insert(std::make_pair(time_idle, it->first));
  }
  return idle_times_to_sockets;
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
  switch (record.state) {
    case kCreated:
      ss << ", created,";
      break;
    case kConnected:
      ss << ", connect called,";
      break;
    case kListened:
      ss << ", listen called,";
      break;
  }
  ss << " has been idle for " << ComputeTimeIdle(record) * 1.0f / kSbTimeSecond
     << " seconds";
  return ss.str();
}

// static
SbTimeMonotonic SocketTracker::ComputeTimeIdle(const SocketRecord& record) {
  return SbTimeGetMonotonicNow() - record.last_activity;
}

}  // namespace socket
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
