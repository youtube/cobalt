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

#include "starboard/log.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const int kPort = 2048;

TEST(SbSocketIsConnectedTest, RainyDayInvalidSocket) {
  EXPECT_FALSE(SbSocketIsConnected(kSbSocketInvalid));
}

TEST(SbSocketIsConnectedTest, SunnyDay) {
  ConnectedTrio trio = CreateAndConnect(kPort, kSocketTimeout);
  if (!SbSocketIsValid(trio.server_socket)) {
    return;
  }

  EXPECT_FALSE(SbSocketIsConnected(trio.listen_socket));
  EXPECT_TRUE(SbSocketIsConnected(trio.server_socket));
  EXPECT_TRUE(SbSocketIsConnected(trio.client_socket));

  // This is just to contrast the behavior with SbSocketIsConnectedAndIdle.
  char buf[512] = {0};
  SbSocketSendTo(trio.server_socket, buf, sizeof(buf), NULL);
  EXPECT_TRUE(SbSocketIsConnected(trio.client_socket));
  SbSocketSendTo(trio.client_socket, buf, sizeof(buf), NULL);
  EXPECT_TRUE(SbSocketIsConnected(trio.server_socket));

  EXPECT_TRUE(SbSocketDestroy(trio.server_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.client_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.listen_socket));
}

TEST(SbSocketIsConnectedTest, SunnyDayNotConnected) {
  SbSocket socket = CreateTcpIpv4Socket();
  if (!SbSocketIsValid(socket)) {
    return;
  }

  EXPECT_FALSE(SbSocketIsConnected(socket));

  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST(SbSocketIsConnectedTest, SunnyDayListeningNotConnected) {
  SbSocket server_socket = CreateListeningTcpIpv4Socket(kPort);
  if (!SbSocketIsValid(server_socket)) {
    return;
  }

  EXPECT_FALSE(SbSocketIsConnected(server_socket));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
