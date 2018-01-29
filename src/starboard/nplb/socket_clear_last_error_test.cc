// Copyright 2016 Google Inc. All Rights Reserved.
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

// The Sunny Day test is used in several other tests as the way to detect if the
// error is correct, so it is not repeated here.

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSocketClearLastErrorTest, SunnyDay) {
  // Set up a socket, but don't Bind or Listen.
  SbSocket server_socket = CreateServerTcpSocket(kSbSocketAddressTypeIpv4);
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  // Accept on the unbound socket should result in an error.
  EXPECT_EQ(kSbSocketInvalid, SbSocketAccept(server_socket));
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketGetLastError(server_socket));

  // After we clear the error, it should be OK.
  EXPECT_TRUE(SbSocketClearLastError(server_socket));
  EXPECT_EQ(kSbSocketOk, SbSocketGetLastError(server_socket));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST(SbSocketClearLastErrorTest, RainyDayInvalidSocket) {
  EXPECT_FALSE(SbSocketClearLastError(kSbSocketInvalid));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
