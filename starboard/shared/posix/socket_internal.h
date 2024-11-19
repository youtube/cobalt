// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_POSIX_SOCKET_INTERNAL_H_
#define STARBOARD_SHARED_POSIX_SOCKET_INTERNAL_H_

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "starboard/common/socket.h"
#include "starboard/shared/internal_only.h"
#include "starboard/socket_waiter.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace posix {

// A helper class for converting back and forth from sockaddrs, ugh.
class SockAddr {
 public:
  SockAddr() : length(sizeof(storage_)) {}
  ~SockAddr() {}

  // Initializes this SockAddr with the given SbSocketAddress, overwriting
  // anything any address previously held.
  bool FromSbSocketAddress(const SbSocketAddress* address);

  // Initializes the given SbSocketAddress with this SockAddr, which must have
  // been previously initialized.
  bool ToSbSocketAddress(SbSocketAddress* out_address) const;

  // Initializes this SockAddr with |sock_addr|, assuming it is appropriately
  // sized for its type.
  bool FromSockaddr(const struct sockaddr* sock_addr);

  // The sockaddr family. We only support INET and INET6.
  sa_family_t family() const { return sockaddr()->sa_family; }

  struct sockaddr* sockaddr() {
    return reinterpret_cast<struct sockaddr*>(&storage_);
  }

  const struct sockaddr* sockaddr() const {
    return reinterpret_cast<const struct sockaddr*>(&storage_);
  }

  struct sockaddr_in* sockaddr_in() {
    return reinterpret_cast<struct sockaddr_in*>(&storage_);
  }

  const struct sockaddr_in* sockaddr_in() const {
    return reinterpret_cast<const struct sockaddr_in*>(&storage_);
  }

  struct sockaddr_in6* sockaddr_in6() {
    return reinterpret_cast<struct sockaddr_in6*>(&storage_);
  }

  const struct sockaddr_in6* sockaddr_in6() const {
    return reinterpret_cast<const struct sockaddr_in6*>(&storage_);
  }

  // Public on purpose, because it will be handy to be passed directly by
  // reference into other functions.
  socklen_t length;

 private:
  struct sockaddr_storage storage_;
};

}  // namespace posix
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_POSIX_SOCKET_INTERNAL_H_
