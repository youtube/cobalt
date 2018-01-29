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

#include <vector>

#include "starboard/common/scoped_ptr.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

const SbTime kSocketTimeout = kSbTimeSecond / 5;

// Returns true if the given address is the unspecified address (all zeros),
// supporting both address types.
bool IsUnspecified(const SbSocketAddress* address);

// Returns true if the given address is the localhost address, supporting both
// address types.
bool IsLocalhost(const SbSocketAddress* address);

// Returns a valid port number that can be bound to for use in nplb tests.
// This will always return the same port number.
int GetPortNumberForTests();

// Returns an IP localhost address with the given port.
SbSocketAddress GetLocalhostAddress(SbSocketAddressType address_type, int port);

// Returns an IP unspecified address with the given port.
SbSocketAddress GetUnspecifiedAddress(SbSocketAddressType address_type,
                                      int port);

// Creates a TCP/IP server socket (sets Reuse Address option).
SbSocket CreateServerTcpSocket(SbSocketAddressType address_type);
scoped_ptr<Socket> CreateServerTcpSocketWrapped(
    SbSocketAddressType address_type);

// Creates a TCP/IP socket bound to all interfaces on the given port.
SbSocket CreateBoundTcpSocket(SbSocketAddressType address_type, int port);
scoped_ptr<Socket> CreateBoundTcpSocketWrapped(SbSocketAddressType address_type,
                                               int port);

// Creates a TCP/IP socket listening on all interfaces on the given port.
SbSocket CreateListeningTcpSocket(SbSocketAddressType address_type, int port);
scoped_ptr<Socket> CreateListeningTcpSocketWrapped(
    SbSocketAddressType address_type,
    int port);

// Tries to accept a new connection from the given listening socket by checking,
// yielding, and retrying for up to timeout. Returns kSbSocketInvalid if no
// socket has been accepted in the given time.
SbSocket AcceptBySpinning(SbSocket listen_socket, SbTime timeout);
scoped_ptr<Socket> AcceptBySpinning(Socket* listen_socket, SbTime timeout);

// Writes the given data to socket, spinning until success or error.
bool WriteBySpinning(SbSocket socket,
                     const char* data,
                     int data_size,
                     SbTime timeout);
bool WriteBySpinning(Socket* socket,
                     const char* data,
                     int data_size,
                     SbTime timeout);

// Reads the given amount of data from socket, spinning until success or error.
bool ReadBySpinning(SbSocket socket,
                    char* out_data,
                    int data_size,
                    SbTime timeout);
bool ReadBySpinning(Socket* socket,
                    char* out_data,
                    int data_size,
                    SbTime timeout);

// Transfers data between the two connected local sockets, spinning until |size|
// has been transfered, or an error occurs.
int Transfer(SbSocket receive_socket,
             char* out_data,
             SbSocket send_socket,
             const char* send_data,
             int size);
int Transfer(Socket* receive_socket,
             char* out_data,
             Socket* send_socket,
             const char* send_data,
             int size);

struct ConnectedTrio {
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
};

struct ConnectedTrioWrapped {
  ConnectedTrioWrapped() {}
  ConnectedTrioWrapped(scoped_ptr<Socket> listen_socket,
                       scoped_ptr<Socket> client_socket,
                       scoped_ptr<Socket> server_socket)
      : listen_socket(listen_socket.Pass()),
        client_socket(client_socket.Pass()),
        server_socket(server_socket.Pass()) {}
  scoped_ptr<Socket> listen_socket;
  scoped_ptr<Socket> client_socket;
  scoped_ptr<Socket> server_socket;
};

// Creates and returns 3 TCP/IP sockets, a connected client and server, and a
// listener on the given port. If anything fails, adds a failure and returns
// three invalid sockets.
ConnectedTrio CreateAndConnect(SbSocketAddressType server_address_type,
                               SbSocketAddressType client_address_type,
                               int port,
                               SbTime timeout);
scoped_ptr<ConnectedTrioWrapped> CreateAndConnectWrapped(
    SbSocketAddressType server_address_type,
    SbSocketAddressType client_address_type,
    int port,
    SbTime timeout);

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
  WaitShouldBlockBetween(waiter, 0, kSocketTimeout);
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

// Socket operations may return specific (e.g. kSbSocketErrorConnectionReset) or
// general (e.g. kSbSocketErrorFailed) error codes, and while in some cases
// it may be important that we obtain a specific error message, in other cases
// it will just be used as a hint and so these methods are provided to make
// it easy to test against specific or general errors.
static inline bool SocketErrorIn(
    SbSocketError error,
    const std::vector<SbSocketError>& expected_set) {
  for (size_t i = 0; i < expected_set.size(); ++i) {
    if (expected_set[i] == error) {
      return true;
    }
  }
  return false;
}

#define EXPECT_SB_SOCKET_ERROR_IN(error, ...)        \
  do {                                               \
    EXPECT_TRUE(SocketErrorIn(error, {__VA_ARGS__})) \
        << "With " #error " = " << error;            \
  } while (false)

#define EXPECT_SB_SOCKET_ERROR_IS_ERROR(error)                          \
  do {                                                                  \
    EXPECT_FALSE(SocketErrorIn(error, {kSbSocketOk, kSbSocketPending})) \
        << "With " #error " = " << error;                               \
  } while (false)

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_SOCKET_HELPERS_H_
