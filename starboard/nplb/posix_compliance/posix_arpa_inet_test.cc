// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <arpa/inet.h>
#include <stdint.h>

#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const uint16_t kTestU16 = 0xABCD;
const uint16_t kExpectedU16 = 0xCDAB;
const uint32_t kTestU32 = 0xFEDCBA98;
const uint32_t kExpectedU32 = 0x98BADCFE;

#if SB_IS(BIG_ENDIAN)
TEST(PosixArpaInetTest, BigEndian) {
  EXPECT_EQ(kTestU16, htons(kTestU16));
  EXPECT_EQ(kTestU16, ntohs(kTestU16));

  EXPECT_EQ(kTestU32, htonl(kTestU32));
  EXPECT_EQ(kTestU32, ntohl(kTestU32));
}
#else
TEST(PosixArpaInetTest, LittleEndian) {
  EXPECT_EQ(kExpectedU16, htons(kTestU16));
  EXPECT_EQ(kExpectedU16, ntohs(kTestU16));

  EXPECT_EQ(kExpectedU32, htonl(kTestU32));
  EXPECT_EQ(kExpectedU32, ntohl(kTestU32));
}
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
