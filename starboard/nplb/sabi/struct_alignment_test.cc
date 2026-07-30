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

#include <cstddef>

#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

// 8-byte integers make structs 4-byte aligned on x86.
#if SB_IS(ARCH_X86)
#define ALIGNMENT_8_BYTE_INT 4
#else  // !SB_IS(ARCH_X86)
#define ALIGNMENT_8_BYTE_INT 8
#endif  // SB_IS(ARCH_X86)

namespace nplb {
namespace {

static const int8_t kInt8 = 0x74;
static const int16_t kInt16 = 0x2DEF;
static const int32_t kInt32 = 0x35C7ADD2;
static const int64_t kInt64 = 0x16FE0870D4784352;

// Checks the trailing padding of members of ascending data type sizes.
typedef struct Struct1 {
  int8_t a;
  int16_t b;
  int32_t c;
  int64_t d;
} Struct1;

static_assert(SB_ALIGNOF(Struct1) == ALIGNMENT_8_BYTE_INT);

static_assert(offsetof(Struct1, a) == 0);
static_assert(offsetof(Struct1, b) == 2);
static_assert(offsetof(Struct1, c) == 4);
static_assert(offsetof(Struct1, d) == 8);

// Checks the trailing padding of members of descending data type sizes.
typedef struct Struct2 {
  int64_t a;
  int32_t b;
  int16_t c;
  int8_t d;
} Struct2;

static_assert(SB_ALIGNOF(Struct2) == ALIGNMENT_8_BYTE_INT);

static_assert(offsetof(Struct2, a) == 0);
static_assert(offsetof(Struct2, b) == 8);
static_assert(offsetof(Struct2, c) == 12);
static_assert(offsetof(Struct2, d) == 14);

// Checks the trailing padding of nested struct members.
typedef struct Struct3 {
  int8_t a;

  struct {
    int32_t b;
  } c;

  int8_t d;
} Struct3;

static_assert(SB_ALIGNOF(Struct3) == 4);

static_assert(offsetof(Struct3, a) == 0);
static_assert(offsetof(Struct3, c) == 4);
static_assert(offsetof(Struct3, c.b) == 4);
static_assert(offsetof(Struct3, d) == 8);

// Checks the trailing padding of nested union members.
typedef struct Struct4 {
  int8_t a;

  union {
    int32_t b;
  } c;

  int8_t d;
} Struct4;

static_assert(SB_ALIGNOF(Struct4) == 4);

static_assert(offsetof(Struct4, a) == 0);
static_assert(offsetof(Struct4, c) == 4);
static_assert(offsetof(Struct4, c.b) == 4);
static_assert(offsetof(Struct4, d) == 8);

}  // namespace

TEST(SbSabiStructAlignmentTest, AscendingDataTypeSizes) {
  const Struct1 struct1 = {kInt8, kInt16, kInt32, kInt64};
  const int8_t* base = reinterpret_cast<const int8_t*>(&struct1);

  EXPECT_EQ(kInt8, *base);
  EXPECT_EQ(kInt16, *reinterpret_cast<const int16_t*>(base + 2));
  EXPECT_EQ(kInt32, *reinterpret_cast<const int32_t*>(base + 4));
  EXPECT_EQ(kInt64, *reinterpret_cast<const int64_t*>(base + 8));
}

TEST(SbSabiStructAlignmentTest, DescendingDataTypeSizes) {
  const Struct2 struct2 = {kInt64, kInt32, kInt16, kInt8};
  const int8_t* base = reinterpret_cast<const int8_t*>(&struct2);

  EXPECT_EQ(kInt64, *reinterpret_cast<const int64_t*>(base));
  EXPECT_EQ(kInt32, *reinterpret_cast<const int32_t*>(base + 8));
  EXPECT_EQ(kInt16, *reinterpret_cast<const int16_t*>(base + 12));
  EXPECT_EQ(kInt8, *(base + 14));
}

TEST(SbSabiStructAlignmentTest, NestedStruct) {
  const Struct3 struct3 = {kInt8, {kInt32}, kInt8};
  const int8_t* base = reinterpret_cast<const int8_t*>(&struct3);

  EXPECT_EQ(kInt8, *base);
  EXPECT_EQ(kInt32, *reinterpret_cast<const int32_t*>(base + 4));
  EXPECT_EQ(kInt8, *(base + 8));
}

TEST(SbSabiStructAlignmentTest, NestedUnion) {
  const Struct4 struct4 = {kInt8, {kInt32}, kInt8};
  const int8_t* base = reinterpret_cast<const int8_t*>(&struct4);

  EXPECT_EQ(kInt8, *base);
  EXPECT_EQ(kInt32, *reinterpret_cast<const int32_t*>(base + 4));
  EXPECT_EQ(kInt8, *(base + 8));
}

}  // namespace nplb

#undef ALIGNMENT_8_BYTE_INT
