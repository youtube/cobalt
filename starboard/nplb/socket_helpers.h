// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_SOCKET_HELPERS_H_
#define STARBOARD_NPLB_SOCKET_HELPERS_H_

#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

const SbTime kTimeout = kSbTimeSecond / 8;

// Creates a plain TCP/IPv4 socket.
static inline SbSocket CreateTcpIpv4Socket() {
  return SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
}

// Creates a plain UDP/IPv4 socket.
static inline SbSocket CreateUdpIpv4Socket() {
  return SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolUdp);
}

// Returns true if the given address is the unspecified address (all zeros),
// supporting both address types.
bool IsUnspecified(const SbSocketAddress* address);

// Returns true if the given address is the localhost address, supporting both
// address types.
bool IsLocalhost(const SbSocketAddress* address);

// Returns an IPv4 localhost address with the given port.
SbSocketAddress GetIpv4Localhost(int port);

// Returns an IPv4 unspecified address with the given port.
SbSocketAddress GetIpv4Unspecified(int port);

// Returns an IPv6 localhost address with the given port.
SbSocketAddress GetIpv6Localhost(int port);

// Returns an IPv6 unspecified address with the given port.
SbSocketAddress GetIpv6Unspecified(int port);

// Creates a TCP/IPv4 server socket (sets Reuse Address option).
SbSocket CreateServerTcpIpv4Socket();

// Creates a TCP/IPv4 socket bound to all interfaces on the given port.
SbSocket CreateBoundTcpIpv4Socket(int port);

// Creates a TCP/IPv4 socket listening on all interfaces on the given port.
SbSocket CreateListeningTcpIpv4Socket(int port);

// Creates a TCP/IPv4 socket connecting to the given port on localhost.
SbSocket CreateConnectingTcpIpv4Socket(int port);

// Tries to accept a new connection from the given listening socket by checking,
// yielding, and retrying for up to timeout. Returns kSbSocketInvalid if no
// socket has been accepted in the given time.
SbSocket AcceptBySpinning(SbSocket listen_socket, SbTime timeout);

// Writes the given data to socket, spinning until success or error.
bool WriteBySpinning(SbSocket socket,
                     const char* data,
                     int data_size,
                     SbTime timeout);

// Reads the given amount of data from socket, spinning until success or error.
bool ReadBySpinning(SbSocket socket,
                    char* out_data,
                    int data_size,
                    SbTime timeout);

typedef struct ConnectedTrio {
  ConnectedTrio()
      : listen_socket(kSbSocketInvalid),
        client_socket(kSbSocketInvalid),
        server_socket(kSbSocketInvalid) {}
  ConnectedTrio(SbSocket listen_socket,
                SbSocket client_socket,
                SbSocket server_socket)
      : listen_socket(listen_socket),
        client_socket(client_socket),
        server_socket(server_socket) {}
  SbSocket listen_socket;
  SbSocket client_socket;
  SbSocket server_socket;
} ConnectedTrio;

// Creates and returns 3 sockets, a connected client and server, and a listener
// on the given port. If anything fails, adds a failure and returns three
// invalid sockets.
ConnectedTrio CreateAndConnect(int port, SbTime timeout);

// Waits on the given waiter, and returns the elapsed time.
SbTimeMonotonic TimedWait(SbSocketWaiter waiter);

// Waits on the given waiter, and returns the elapsed time.
SbTimeMonotonic TimedWaitTimed(SbSocketWaiter waiter, SbTime timeout);

// Waits on the given waiter, and checks that it blocked between [lower, upper).
static inline void WaitShouldBlockBetween(SbSocketWaiter waiter,
                                          SbTime lower,
                                          SbTime upper) {
  SbTime time = TimedWait(waiter);
  EXPECT_GT(upper, time);
  EXPECT_LE(lower, time);
}

// Waits on the given waiter, and checks that it did not block for very long.
static inline void WaitShouldNotBlock(SbSocketWaiter waiter) {
  WaitShouldBlockBetween(waiter, 0, kTimeout);
}

// Waits on the given waiter, and checks that it did not block for the given
// timeout.
static inline void TimedWaitShouldNotBlock(SbSocketWaiter waiter,
                                           SbTime timeout) {
  EXPECT_GT(timeout, TimedWaitTimed(waiter, timeout));
}

// Waits on the given waiter, and checks that it did block for at least the
// given timeout.
static inline void TimedWaitShouldBlock(SbSocketWaiter waiter, SbTime timeout) {
  EXPECT_LE(timeout, TimedWaitTimed(waiter, timeout));
}

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_SOCKET_HELPERS_H_
