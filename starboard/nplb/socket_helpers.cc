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

#include "starboard/nplb/socket_helpers.h"

#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

bool IsUnspecified(const SbSocketAddress* address) {
  // Look at each piece of memory and make sure too many of them aren't zero.
  int components = (address->type == kSbSocketAddressTypeIpv4 ? 4 : 16);
  int zero_count = 0;
  for (int i = 0; i < components; ++i) {
    if (address->address[i] == 0) {
      ++zero_count;
    }
  }
  return components == zero_count;
}

bool IsLocalhost(const SbSocketAddress* address) {
  if (address->type == kSbSocketAddressTypeIpv4) {
    return (address->address[0] == 127 && address->address[1] == 0 &&
            address->address[2] == 0 && address->address[3] == 1);
  }

  if (address->type == kSbSocketAddressTypeIpv6) {
    bool may_be_localhost = true;
    for (int i = 0; i < 15; ++i) {
      may_be_localhost &= (address->address[i] == 0);
    }

    return (may_be_localhost && address->address[15] == 1);
  }

  return false;
}

SbSocketAddress GetIpv4Localhost(int port) {
  SbSocketAddress address = GetIpv4Unspecified(port);
  address.address[0] = 127;
  address.address[3] = 1;
  return address;
}

SbSocketAddress GetIpv4Unspecified(int port) {
  SbSocketAddress address = {0};
  address.type = kSbSocketAddressTypeIpv4;
  address.port = port;
  return address;
}

SbSocketAddress GetIpv6Localhost(int port) {
  SbSocketAddress address = GetIpv6Unspecified(port);
  address.address[15] = 1;
  return address;
}

SbSocketAddress GetIpv6Unspecified(int port) {
  SbSocketAddress address = {0};
  address.type = kSbSocketAddressTypeIpv6;
  address.port = port;
  return address;
}

SbSocket CreateServerTcpIpv4Socket() {
  SbSocket server_socket = CreateTcpIpv4Socket();
  if (!SbSocketIsValid(server_socket)) {
    ADD_FAILURE() << "SbSocketCreate failed";
    return kSbSocketInvalid;
  }

  if (!SbSocketSetReuseAddress(server_socket, true)) {
    ADD_FAILURE() << "SbSocketSetReuseAddress failed";
    SbSocketDestroy(server_socket);
    return kSbSocketInvalid;
  }

  return server_socket;
}

SbSocket CreateBoundTcpIpv4Socket(int port) {
  SbSocket server_socket = CreateServerTcpIpv4Socket();
  if (!SbSocketIsValid(server_socket)) {
    return kSbSocketInvalid;
  }

  SbSocketAddress address = GetIpv4Unspecified(port);
  SbSocketError result = SbSocketBind(server_socket, &address);
  if (result != kSbSocketOk) {
    ADD_FAILURE() << "SbSocketBind to " << port << " failed: " << result;
    SbSocketDestroy(server_socket);
    return kSbSocketInvalid;
  }

  return server_socket;
}

SbSocket CreateListeningTcpIpv4Socket(int port) {
  SbSocket server_socket = CreateBoundTcpIpv4Socket(port);
  if (!SbSocketIsValid(server_socket)) {
    return kSbSocketInvalid;
  }

  SbSocketError result = SbSocketListen(server_socket);
  if (result != kSbSocketOk) {
    ADD_FAILURE() << "SbSocketListen failed: " << result;
    SbSocketDestroy(server_socket);
    return kSbSocketInvalid;
  }

  return server_socket;
}

SbSocket CreateConnectingTcpIpv4Socket(int port) {
  SbSocket client_socket = CreateTcpIpv4Socket();
  if (!SbSocketIsValid(client_socket)) {
    ADD_FAILURE() << "SbSocketCreate failed";
    return kSbSocketInvalid;
  }

  // Connect to localhost:<port>.
  SbSocketAddress address = GetIpv4Localhost(port);

  // This connect will probably return pending, but we'll assume it will connect
  // eventually.
  SbSocketError result = SbSocketConnect(client_socket, &address);
  if (result != kSbSocketOk && result != kSbSocketPending) {
    ADD_FAILURE() << "SbSocketConnect failed: " << result;
    SbSocketDestroy(client_socket);
    return kSbSocketInvalid;
  }

  return client_socket;
}

SbSocket AcceptBySpinning(SbSocket server_socket, SbTime timeout) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  while (true) {
    SbSocket accepted_socket = SbSocketAccept(server_socket);
    if (SbSocketIsValid(accepted_socket)) {
      return accepted_socket;
    }

    // If we didn't get a socket, it should be pending.
    EXPECT_EQ(kSbSocketPending, SbSocketGetLastError(server_socket));

    // Check if we have passed our timeout.
    if (SbTimeGetMonotonicNow() - start >= timeout) {
      break;
    }

    // Just being polite.
    SbThreadYield();
  }

  return kSbSocketInvalid;
}

bool WriteBySpinning(SbSocket socket,
                     const char* data,
                     int data_size,
                     SbTime timeout) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  int total = 0;
  while (total < data_size) {
    int sent = SbSocketSendTo(socket, data + total, data_size - total, NULL);
    if (sent >= 0) {
      total += sent;
      continue;
    }

    if (SbSocketGetLastError(socket) != kSbSocketPending) {
      return false;
    }

    if (SbTimeGetMonotonicNow() - start >= timeout) {
      return false;
    }

    SbThreadYield();
  }

  return true;
}

bool ReadBySpinning(SbSocket socket,
                    char* out_data,
                    int data_size,
                    SbTime timeout) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  int total = 0;
  while (total < data_size) {
    int received =
        SbSocketReceiveFrom(socket, out_data + total, data_size - total, NULL);
    if (received >= 0) {
      total += received;
      continue;
    }

    if (SbSocketGetLastError(socket) != kSbSocketPending) {
      return false;
    }

    if (SbTimeGetMonotonicNow() - start >= timeout) {
      return false;
    }

    SbThreadYield();
  }

  return true;
}

ConnectedTrio CreateAndConnect(int port, SbTime timeout) {
  // Set up a listening socket.
  SbSocket listen_socket = CreateListeningTcpIpv4Socket(port);
  if (!SbSocketIsValid(listen_socket)) {
    ADD_FAILURE() << "Could not create listen socket.";
    return ConnectedTrio();
  }

  // Create a new socket to connect to the listening socket.
  SbSocket client_socket = CreateConnectingTcpIpv4Socket(port);
  if (!SbSocketIsValid(client_socket)) {
    ADD_FAILURE() << "Could not create client socket.";
    EXPECT_TRUE(SbSocketDestroy(listen_socket));
    return ConnectedTrio();
  }

  // Spin until the accept happens (or we get impatient).
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  SbSocket server_socket = AcceptBySpinning(listen_socket, timeout);
  if (!SbSocketIsValid(server_socket)) {
    ADD_FAILURE() << "Failed to accept within " << timeout;
    EXPECT_TRUE(SbSocketDestroy(listen_socket));
    EXPECT_TRUE(SbSocketDestroy(client_socket));
    return ConnectedTrio();
  }

  return ConnectedTrio(listen_socket, client_socket, server_socket);
}

SbTimeMonotonic TimedWait(SbSocketWaiter waiter) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  SbSocketWaiterWait(waiter);
  return SbTimeGetMonotonicNow() - start;
}

// Waits on the given waiter, and returns the elapsed time.
SbTimeMonotonic TimedWaitTimed(SbSocketWaiter waiter, SbTime timeout) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  SbSocketWaiterWaitTimed(waiter, timeout);
  return SbTimeGetMonotonicNow() - start;
}

}  // namespace nplb
}  // namespace starboard
