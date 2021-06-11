// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

// Thread joining is mostly tested in the other tests.

#include "starboard/common/murmurhash2.h"
#include "starboard/common/byte_swap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(MurmerHash2, AlignedEqualsUnaligned) {
  uint32_t val = 0x1234;
  uint32_t hash_unaligned = MurmurHash2_32(&val, sizeof(val));
  uint32_t hash_aligned = MurmurHash2_32_Aligned(&val, sizeof(val));
  EXPECT_EQ(hash_aligned, hash_unaligned);
  char data[sizeof(uint32_t)*2];
  memcpy(data+1, &val, sizeof(val));
  EXPECT_EQ(hash_aligned, MurmurHash2_32(data + 1, sizeof(val)));
}

TEST(MurmerHash2, ChainableHash) {
  uint32_t val = 0x1234;
  EXPECT_NE(MurmurHash2_32(&val, sizeof(val)),
            MurmurHash2_32(&val, sizeof(val), 1234));
}

TEST(MurmerHash2, IgnoresBytesNotConsidered) {
  const char* v1 = "Hello";
  const char* v2 = "Hello2";
  EXPECT_EQ(MurmurHash2_32(v1, 5), MurmurHash2_32(v2, 5));
}

#if !SB_IS(BIG_ENDIAN)
TEST(MurmerHash2, ExpectedLittleEndianValue) {
  char s[] = "1234";
  // Checks that output matches expected value generated online
  // at http://murmurhash.shorelabs.com/
  EXPECT_EQ(3347951060, MurmurHash2_32(s, sizeof(s), 1));
}
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
