// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/byte_swap.h"

#include <iomanip>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const uint16_t kTestU16 = 0xABCD;
const uint16_t kExpectedU16 = 0xCDAB;
const int16_t kTestS16 = 0x0123;
const int16_t kExpectedS16 = 0x2301;
const uint32_t kTestU32 = 0xFEDCBA98;
const uint32_t kExpectedU32 = 0x98BADCFE;
const int32_t kTestS32 = 0x01234568;
const int32_t kExpectedS32 = 0x68452301;
const uint64_t kTestU64 = SB_UINT64_C(0xFEDCBA9876543210);
const uint64_t kExpectedU64 = SB_UINT64_C(0x1032547698BADCFE);
const int64_t kTestS64 = static_cast<int64_t>(SB_INT64_C(0xFEDCBA9876543210));
const int64_t kExpectedS64 =
    static_cast<int64_t>(SB_INT64_C(0x1032547698BADCFE));

TEST(SbByteSwapTest, SunnyDay) {
  EXPECT_EQ(kExpectedU16, SbByteSwapU16(kTestU16));
  EXPECT_EQ(kExpectedS16, SbByteSwapS16(kTestS16));

  EXPECT_EQ(kExpectedU32, SbByteSwapU32(kTestU32));
  EXPECT_EQ(kExpectedS32, SbByteSwapS32(kTestS32));

  EXPECT_EQ(kExpectedU64, SbByteSwapU64(kTestU64));
  EXPECT_EQ(kExpectedS64, SbByteSwapS64(kTestS64));
}

#if SB_IS(BIG_ENDIAN)
TEST(SbByteSwapTest, BigEndian) {
  EXPECT_EQ(kTestU16, SB_HOST_TO_NET_U16(kTestU16));
  EXPECT_EQ(kTestS16, SB_HOST_TO_NET_S16(kTestS16));

  EXPECT_EQ(kTestU32, SB_HOST_TO_NET_U32(kTestU32));
  EXPECT_EQ(kTestS32, SB_HOST_TO_NET_S32(kTestS32));

  EXPECT_EQ(kTestU64, SB_HOST_TO_NET_U64(kTestU64));
  EXPECT_EQ(kTestS64, SB_HOST_TO_NET_S64(kTestS64));
}
#else
TEST(SbByteSwapTest, LittleEndian) {
  EXPECT_EQ(kExpectedU16, SB_HOST_TO_NET_U16(kTestU16));
  EXPECT_EQ(kExpectedS16, SB_HOST_TO_NET_S16(kTestS16));

  EXPECT_EQ(kExpectedU32, SB_HOST_TO_NET_U32(kTestU32));
  EXPECT_EQ(kExpectedS32, SB_HOST_TO_NET_S32(kTestS32));

  EXPECT_EQ(kExpectedU64, SB_HOST_TO_NET_U64(kTestU64));
  EXPECT_EQ(kExpectedS64, SB_HOST_TO_NET_S64(kTestS64));
}
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
