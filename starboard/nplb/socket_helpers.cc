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

#include <ifaddrs.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "starboard/nplb/socket_helpers.h"

#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

int port_number_for_tests = 0;
pthread_once_t valid_port_once_control = PTHREAD_ONCE_INIT;

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
  static int incremental = 0;
  if (incremental + SB_SOCKET_OVERRIDE_PORT_FOR_TESTS == 65535) {
    incremental = 0;
  }
  return SB_SOCKET_OVERRIDE_PORT_FOR_TESTS + ++incremental;
#else
  pthread_once(&valid_port_once_control, &InitializePortNumberForTests);
  return port_number_for_tests;
#endif
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

int64_t TimedWait(SbSocketWaiter waiter) {
  int64_t start = CurrentMonotonicTime();
  SbSocketWaiterWait(waiter);
  return CurrentMonotonicTime() - start;
}

// Waits on the given waiter, and returns the elapsed time.
int64_t TimedWaitTimed(SbSocketWaiter waiter, int64_t timeout) {
  int64_t start = CurrentMonotonicTime();
  SbSocketWaiterWaitTimed(waiter, timeout);
  return CurrentMonotonicTime() - start;
}

#if !defined(COBALT_BUILD_TYPE_GOLD)
namespace {
const char* SbSocketAddressTypeName(SbSocketAddressType type) {
  const char* name = "unknown";
  switch (type) {
    case kSbSocketAddressTypeIpv4:
      name = "ipv4";
      break;
    case kSbSocketAddressTypeIpv6:
      name = "ipv6";
      break;
  }
  return name;
}

const char* SbSocketAddressFilterName(SbSocketResolveFilter filter) {
  const char* name = "unknown";
  switch (filter) {
    case kSbSocketResolveFilterNone:
      name = "none";
      break;
    case kSbSocketResolveFilterIpv4:
      name = "ipv4";
      break;
    case kSbSocketResolveFilterIpv6:
      name = "ipv6";
      break;
  }
  return name;
}

const char* SbSocketProtocolName(SbSocketProtocol protocol) {
  const char* name = "unknown";
  switch (protocol) {
    case kSbSocketProtocolTcp:
      name = "tcp";
      break;
    case kSbSocketProtocolUdp:
      name = "udp";
      break;
  }
  return name;
}
}  // namespace

std::string GetSbSocketAddressTypeName(
    ::testing::TestParamInfo<SbSocketAddressType> info) {
  return FormatString("type_%s", SbSocketAddressTypeName(info.param));
}
std::string GetSbSocketAddressTypePairName(
    ::testing::TestParamInfo<
        std::pair<SbSocketAddressType, SbSocketAddressType>> info) {
  return FormatString("type_%s_type_%s",
                      SbSocketAddressTypeName(info.param.first),
                      SbSocketAddressTypeName(info.param.second));
}

std::string GetSbSocketAddressTypeFilterPairName(
    ::testing::TestParamInfo<
        std::pair<SbSocketAddressType, SbSocketResolveFilter>> info) {
  return FormatString("type_%s_filter_%s",
                      SbSocketAddressTypeName(info.param.first),
                      SbSocketAddressFilterName(info.param.second));
}

std::string GetSbSocketFilterAddressTypePairName(
    ::testing::TestParamInfo<
        std::pair<SbSocketResolveFilter, SbSocketAddressType>> info) {
  return FormatString("filter_%s_type_%s",
                      SbSocketAddressFilterName(info.param.first),
                      SbSocketAddressTypeName(info.param.second));
}

std::string GetSbSocketAddressTypeProtocolPairName(
    ::testing::TestParamInfo<std::pair<SbSocketAddressType, SbSocketProtocol>>
        info) {
  return FormatString("type_%s_%s", SbSocketAddressTypeName(info.param.first),
                      SbSocketProtocolName(info.param.second));
}

#endif  // #if !defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace nplb
}  // namespace starboard
