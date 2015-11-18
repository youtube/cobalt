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

// The basic Sunny Day test is a subset of other Sunny Day tests, so it is not
// repeated here.

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sbnplb = starboard::nplb;

namespace {

TEST(SbSocketConnectTest, RainyDayNullSocket) {
  SbSocketAddress address = sbnplb::GetIpv4Unspecified(2048);
  EXPECT_EQ(kSbSocketErrorFailed, SbSocketConnect(kSbSocketInvalid, &address));
}

TEST(SbSocketConnectTest, RainyDayNullAddress) {
  SbSocket socket = sbnplb::CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));
  EXPECT_EQ(kSbSocketErrorFailed, SbSocketConnect(socket, NULL));
}

TEST(SbSocketConnectTest, RainyDayNullNull) {
  EXPECT_EQ(kSbSocketErrorFailed, SbSocketConnect(kSbSocketInvalid, NULL));
}

}  // namespace
