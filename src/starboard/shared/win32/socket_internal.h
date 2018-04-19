// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_SOCKET_INTERNAL_H_
#define STARBOARD_SHARED_WIN32_SOCKET_INTERNAL_H_

#include <winsock2.h>
#include <WS2tcpip.h>

#include "starboard/atomic.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/win32/auto_event_handle.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/types.h"

namespace sbwin32 = starboard::shared::win32;

struct SbSocketPrivate {
  enum struct BindTarget {
    kUnbound = 0,
    kAny = 1,
    kOther = 2,
    kAccepted = 3,
  };

  SbSocketPrivate(SbSocketAddressType address_type,
                  SbSocketProtocol protocol,
                  SOCKET handle,
                  BindTarget bound_to)
      : address_type(address_type),
        protocol(protocol),
        socket_handle(handle),
        socket_event(WSA_INVALID_EVENT),
        writable(0),
        error(kSbSocketOk),
        waiter(kSbSocketWaiterInvalid),
        bound_to(bound_to) {}
  ~SbSocketPrivate() {}

  // The address domain of this socket, IPv4 or IPv6.
  SbSocketAddressType address_type;

  // The protocol of this socket, UDP or TCP.
  SbSocketProtocol protocol;

  // The handle for this socket.
  SOCKET socket_handle;

  // The event related to the socket_handle.  Used for SbSocketWaiter.
  sbwin32::AutoEventHandle socket_event;

  // Set to true between when socket is shown as writable via WSAEventSelect/
  // WSAWaitForMultipleEvents and when writing to the socket returns
  // fails with WSAEWOULDBLOCK.
  //
  // Used to work around the fact that WSAEventSelect for FD_WRITE is
  // edge-triggered, unlike other events.
  //
  // See MSDN documentato n for WSAEventSelect FD_WRITE for more info.
  starboard::atomic_bool writable;

  // The last error that occurred on this socket, or kSbSocketOk.
  SbSocketError error;

  // The waiter this socket is registered with, or kSbSocketWaiterInvalid.
  SbSocketWaiter waiter;

  BindTarget bound_to;
};

namespace starboard {
namespace shared {
namespace win32 {

const socklen_t kAddressLengthIpv4 = 4;
const socklen_t kAddressLengthIpv6 = 16;

// Translates an last_error from a socket call into an SbSocketError.
SbSocketError TranslateSocketErrorStatus(int error);

// Sets a boolean socket option, doing all appropriate checks.
bool SetBooleanSocketOption(SbSocket socket,
                            int level,
                            int option_code,
                            const char* option_name,
                            bool value);

// Sets an integer socket option, doing all appropriate checks.
bool SetIntegerSocketOption(SbSocket socket,
                            int level,
                            int option_code,
                            const char* option_name,
                            int value);

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
  u_short family() const { return sockaddr()->sa_family; }

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
  sockaddr_storage storage_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_SOCKET_INTERNAL_H_
