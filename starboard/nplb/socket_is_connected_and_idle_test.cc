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
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSocketIsConnectedAndIdleTest, RainyDayInvalidSocket) {
  EXPECT_FALSE(SbSocketIsConnectedAndIdle(kSbSocketInvalid));
}

TEST(SbSocketIsConnectedAndIdleTest, SunnyDay) {
  ConnectedTrio trio =
      CreateAndConnect(GetPortNumberForTests(), kSocketTimeout);
  if (!SbSocketIsValid(trio.server_socket)) {
    return;
  }

  EXPECT_FALSE(SbSocketIsConnectedAndIdle(trio.listen_socket));
  EXPECT_TRUE(SbSocketIsConnectedAndIdle(trio.server_socket));
  EXPECT_TRUE(SbSocketIsConnectedAndIdle(trio.client_socket));

  char buf[512] = {0};
  SbSocketSendTo(trio.server_socket, buf, sizeof(buf), NULL);
  // Because I haven't called ReceiveFrom yet, this should stay false. The wait
  // before the checking is for the delay on some platforms.
  SbThreadSleep(kSocketTimeout);
  EXPECT_FALSE(SbSocketIsConnectedAndIdle(trio.client_socket));

  // Should be the same in the other direction.
  SbSocketSendTo(trio.client_socket, buf, sizeof(buf), NULL);
  SbThreadSleep(kSocketTimeout);
  EXPECT_FALSE(SbSocketIsConnectedAndIdle(trio.server_socket));

  EXPECT_TRUE(SbSocketDestroy(trio.server_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.client_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.listen_socket));
}

TEST(SbSocketIsConnectedAndIdleTest, SunnyDayNotConnected) {
  SbSocket socket = CreateTcpIpv4Socket();
  if (!SbSocketIsValid(socket)) {
    return;
  }

  SbThreadSleep(kSocketTimeout);
  EXPECT_FALSE(SbSocketIsConnectedAndIdle(socket));

  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST(SbSocketIsConnectedAndIdleTest, SunnyDayListeningNotConnected) {
  SbSocket server_socket =
      CreateListeningTcpIpv4Socket(GetPortNumberForTests());
  if (!SbSocketIsValid(server_socket)) {
    return;
  }

  EXPECT_FALSE(SbSocketIsConnectedAndIdle(server_socket));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
