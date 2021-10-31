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

#include "starboard/nplb/socket_helpers.h"

#include "starboard/common/scoped_ptr.h"
#include "starboard/common/socket.h"
#include "starboard/once.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

int port_number_for_tests = 0;
SbOnceControl valid_port_once_control = SB_ONCE_INITIALIZER;

void InitializePortNumberForTests() {
  // Create a listening socket. Let the system choose a port for us.
  SbSocket socket = CreateListeningTcpSocket(kSbSocketAddressTypeIpv4, 0);
  SB_DCHECK(socket != kSbSocketInvalid);

  // Query which port this socket was bound to and save it to valid_port_number.
  SbSocketAddress socket_address = {0};
  bool result = SbSocketGetLocalAddress(socket, &socket_address);
  SB_DCHECK(result);
  port_number_for_tests = socket_address.port;

  // Clean up the socket.
  result = SbSocketDestroy(socket);
  SB_DCHECK(result);
}
}  // namespace

int GetPortNumberForTests() {
#if defined(SB_SOCKET_OVERRIDE_PORT_FOR_TESTS)
  return SB_SOCKET_OVERRIDE_PORT_FOR_TESTS;
#else
  SbOnce(&valid_port_once_control, &InitializePortNumberForTests);
  return port_number_for_tests;
#endif
}

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
    return address->address[0] == 127;
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

SbSocket CreateServerTcpSocket(SbSocketAddressType address_type) {
  SbSocket server_socket = SbSocketCreate(address_type, kSbSocketProtocolTcp);
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

scoped_ptr<Socket> CreateServerTcpSocketWrapped(
    SbSocketAddressType address_type) {
  scoped_ptr<Socket> server_socket =
      make_scoped_ptr(new Socket(address_type, kSbSocketProtocolTcp));
  if (!server_socket->IsValid()) {
    ADD_FAILURE() << "SbSocketCreate failed";
    return scoped_ptr<Socket>().Pass();
  }

  if (!server_socket->SetReuseAddress(true)) {
    ADD_FAILURE() << "SbSocketSetReuseAddress failed";
    return scoped_ptr<Socket>().Pass();
  }

  return server_socket.Pass();
}

SbSocket CreateBoundTcpSocket(SbSocketAddressType address_type, int port) {
  SbSocket server_socket = CreateServerTcpSocket(address_type);
  if (!SbSocketIsValid(server_socket)) {
    return kSbSocketInvalid;
  }

  SbSocketAddress address = GetUnspecifiedAddress(address_type, port);
  SbSocketError result = SbSocketBind(server_socket, &address);
  if (result != kSbSocketOk) {
    ADD_FAILURE() << "SbSocketBind to " << port << " failed: " << result;
    SbSocketDestroy(server_socket);
    return kSbSocketInvalid;
  }

  return server_socket;
}

scoped_ptr<Socket> CreateBoundTcpSocketWrapped(SbSocketAddressType address_type,
                                               int port) {
  scoped_ptr<Socket> server_socket = CreateServerTcpSocketWrapped(address_type);
  if (!server_socket) {
    return scoped_ptr<Socket>().Pass();
  }

  SbSocketAddress address = GetUnspecifiedAddress(address_type, port);
  SbSocketError result = server_socket->Bind(&address);
  if (result != kSbSocketOk) {
    ADD_FAILURE() << "SbSocketBind to " << port << " failed: " << result;
    return scoped_ptr<Socket>().Pass();
  }

  return server_socket.Pass();
}

SbSocket CreateListeningTcpSocket(SbSocketAddressType address_type, int port) {
  SbSocket server_socket = CreateBoundTcpSocket(address_type, port);
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

scoped_ptr<Socket> CreateListeningTcpSocketWrapped(
    SbSocketAddressType address_type,
    int port) {
  scoped_ptr<Socket> server_socket =
      CreateBoundTcpSocketWrapped(address_type, port);
  if (!server_socket) {
    return scoped_ptr<Socket>().Pass();
  }

  SbSocketError result = server_socket->Listen();
  if (result != kSbSocketOk) {
    ADD_FAILURE() << "SbSocketListen failed: " << result;
    return scoped_ptr<Socket>().Pass();
  }

  return server_socket.Pass();
}

namespace {
SbSocket CreateConnectingTcpSocket(SbSocketAddressType address_type, int port) {
  SbSocket client_socket = SbSocketCreate(address_type, kSbSocketProtocolTcp);
  if (!SbSocketIsValid(client_socket)) {
    ADD_FAILURE() << "SbSocketCreate failed";
    return kSbSocketInvalid;
  }

  // Connect to localhost:<port>.
  SbSocketAddress address = {};
  bool success = GetLocalhostAddress(address_type, port, &address);
  if (!success) {
    ADD_FAILURE() << "GetLocalhostAddress failed";
    return kSbSocketInvalid;
  }

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

scoped_ptr<Socket> CreateConnectingTcpSocketWrapped(
    SbSocketAddressType address_type,
    int port) {
  scoped_ptr<Socket> client_socket =
      make_scoped_ptr(new Socket(address_type, kSbSocketProtocolTcp));
  if (!client_socket->IsValid()) {
    ADD_FAILURE() << "SbSocketCreate failed";
    return scoped_ptr<Socket>().Pass();
  }

  // Connect to localhost:<port>.
  SbSocketAddress address = {};
  bool success = GetLocalhostAddress(address_type, port, &address);
  if (!success) {
    ADD_FAILURE() << "GetLocalhostAddress failed";
    return scoped_ptr<Socket>().Pass();
  }

  // This connect will probably return pending, but we'll assume it will connect
  // eventually.
  SbSocketError result = client_socket->Connect(&address);
  if (result != kSbSocketOk && result != kSbSocketPending) {
    ADD_FAILURE() << "SbSocketConnect failed: " << result;
    return scoped_ptr<Socket>().Pass();
  }

  return client_socket.Pass();
}
}  // namespace

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

scoped_ptr<Socket> AcceptBySpinning(Socket* server_socket, SbTime timeout) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  while (true) {
    Socket* accepted_socket = server_socket->Accept();
    if (accepted_socket && accepted_socket->IsValid()) {
      return make_scoped_ptr(accepted_socket);
    }

    // If we didn't get a socket, it should be pending.
    EXPECT_TRUE(server_socket->IsPending());

    // Check if we have passed our timeout.
    if (SbTimeGetMonotonicNow() - start >= timeout) {
      break;
    }

    // Just being polite.
    SbThreadYield();
  }

  return scoped_ptr<Socket>().Pass();
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

bool WriteBySpinning(Socket* socket,
                     const char* data,
                     int data_size,
                     SbTime timeout) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  int total = 0;
  while (total < data_size) {
    int sent = socket->SendTo(data + total, data_size - total, NULL);
    if (sent >= 0) {
      total += sent;
      continue;
    }

    if (!socket->IsPending()) {
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

bool ReadBySpinning(Socket* socket,
                    char* out_data,
                    int data_size,
                    SbTime timeout) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  int total = 0;
  while (total < data_size) {
    int received =
        socket->ReceiveFrom(out_data + total, data_size - total, NULL);
    if (received >= 0) {
      total += received;
      continue;
    }

    if (!socket->IsPending()) {
      return false;
    }

    if (SbTimeGetMonotonicNow() - start >= timeout) {
      return false;
    }

    SbThreadYield();
  }

  return true;
}

int Transfer(SbSocket receive_socket,
             char* out_data,
             SbSocket send_socket,
             const char* send_data,
             int size) {
  int send_total = 0;
  int receive_total = 0;
  while (receive_total < size) {
    if (send_total < size) {
      int bytes_sent = SbSocketSendTo(send_socket, send_data + send_total,
                                      size - send_total, NULL);
      if (bytes_sent < 0) {
        if (SbSocketGetLastError(send_socket) != kSbSocketPending) {
          return -1;
        }
        bytes_sent = 0;
      }

      send_total += bytes_sent;
    }

    int bytes_received = SbSocketReceiveFrom(
        receive_socket, out_data + receive_total, size - receive_total, NULL);
    if (bytes_received < 0) {
      if (SbSocketGetLastError(receive_socket) != kSbSocketPending) {
        return -1;
      }
      bytes_received = 0;
    }

    receive_total += bytes_received;
  }

  return size;
}

int Transfer(Socket* receive_socket,
             char* out_data,
             Socket* send_socket,
             const char* send_data,
             int size) {
  int send_total = 0;
  int receive_total = 0;
  while (receive_total < size) {
    if (send_total < size) {
      int bytes_sent =
          send_socket->SendTo(send_data + send_total, size - send_total, NULL);
      if (bytes_sent < 0) {
        if (!send_socket->IsPending()) {
          return -1;
        }
        bytes_sent = 0;
      }

      send_total += bytes_sent;
    }

    int bytes_received = receive_socket->ReceiveFrom(
        out_data + receive_total, size - receive_total, NULL);
    if (bytes_received < 0) {
      if (!receive_socket->IsPending()) {
        return -1;
      }
      bytes_received = 0;
    }

    receive_total += bytes_received;
  }

  return size;
}

ConnectedTrio CreateAndConnect(SbSocketAddressType server_address_type,
                               SbSocketAddressType client_address_type,
                               int port,
                               SbTime timeout) {
  // Verify the listening socket.
  SbSocket listen_socket = CreateListeningTcpSocket(server_address_type, port);
  if (!SbSocketIsValid(listen_socket)) {
    ADD_FAILURE() << "Could not create listen socket.";
    return ConnectedTrio();
  }

  // Verify the socket to connect to the listening socket.
  SbSocket client_socket = CreateConnectingTcpSocket(client_address_type, port);
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

scoped_ptr<ConnectedTrioWrapped> CreateAndConnectWrapped(
    SbSocketAddressType server_address_type,
    SbSocketAddressType client_address_type,
    int port,
    SbTime timeout) {
  // Verify the listening socket.
  scoped_ptr<Socket> listen_socket =
      CreateListeningTcpSocketWrapped(server_address_type, port);
  if (!listen_socket || !listen_socket->IsValid()) {
    ADD_FAILURE() << "Could not create listen socket.";
    return scoped_ptr<ConnectedTrioWrapped>().Pass();
  }

  // Verify the socket to connect to the listening socket.
  scoped_ptr<Socket> client_socket =
      CreateConnectingTcpSocketWrapped(client_address_type, port);
  if (!client_socket || !client_socket->IsValid()) {
    ADD_FAILURE() << "Could not create client socket.";
    return scoped_ptr<ConnectedTrioWrapped>().Pass();
  }

  // Spin until the accept happens (or we get impatient).
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  scoped_ptr<Socket> server_socket =
      AcceptBySpinning(listen_socket.get(), timeout);
  if (!server_socket || !server_socket->IsValid()) {
    ADD_FAILURE() << "Failed to accept within " << timeout;
    return scoped_ptr<ConnectedTrioWrapped>().Pass();
  }

  return make_scoped_ptr(new ConnectedTrioWrapped(
      listen_socket.Pass(), client_socket.Pass(), server_socket.Pass()));
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
