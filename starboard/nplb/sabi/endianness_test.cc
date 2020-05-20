// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 12

namespace starboard {
namespace sabi {
namespace {

static constexpr int32_t kCobalt21 = 0xC0BA1721;

#if SB_IS(BIG_ENDIAN)
static constexpr uint8_t kBytes[] = {0xC0, 0xBA, 0x17, 0x21};
#else  // !SB_IS(BIG_ENDIAN)
static constexpr uint8_t kBytes[] = {0x21, 0x17, 0xBA, 0xC0};
#endif  // SB_IS(BIG_ENDIAN)

}  // namespace

TEST(SbSabiEndiannessTest, Endianness) {
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(*(reinterpret_cast<const uint8_t*>(&kCobalt21) + i), kBytes[i]);
  }
}

}  // namespace sabi
}  // namespace starboard

#endif  // SB_API_VERSION >= 12
