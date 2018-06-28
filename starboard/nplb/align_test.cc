// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

bool IsAligned(void *pointer, size_t alignment) {
  return (reinterpret_cast<uintptr_t>(pointer) % alignment) == 0;
}

size_t GetAlignment(void *pointer) {
  for (size_t alignment = static_cast<size_t>(1) << ((sizeof(size_t) * 8) - 1);
       alignment != 0; alignment /= 2) {
    if (IsAligned(pointer, alignment)) {
      return alignment;
    }
  }

  return 1;
}

struct AlignedFields {
  char unaligned1;
  SB_ALIGNAS(2) char by_2;
  char unaligned2;
  SB_ALIGNAS(4) char by_4;
  char unaligned3;
  SB_ALIGNAS(8) char by_8;
  char unaligned4;
  SB_ALIGNAS(16) char by_16;
  char unaligned5;
  SB_ALIGNAS(32) char by_32;
  char unaligned6;
  SB_ALIGNAS(64) char by_64;
  char unaligned7;
  SB_ALIGNAS(128) char by_128;
  char unaligned8;
  SB_ALIGNAS(256) char by_256;
  char unaligned9;
};

struct AlignedFieldsOf {
  char unaligned1;
  SB_ALIGNAS(SB_ALIGNOF(void*)) char by_void_star;
  char unaligned2;
  SB_ALIGNAS(SB_ALIGNOF(int)) char by_int;
  char unaligned3;
  SB_ALIGNAS(SB_ALIGNOF(uint64_t)) char by_uint64_t;
  char unaligned4;
};

TEST(SbAlignTest, AlignAsStructFieldOnStack) {
  char unaligned = 0;
  EXPECT_NE(1, unaligned);
  AlignedFields aligned;
  EXPECT_LE(2, GetAlignment(&(aligned.by_2)));
  EXPECT_LE(4, GetAlignment(&(aligned.by_4)));
  EXPECT_LE(8, GetAlignment(&(aligned.by_8)));
  EXPECT_LE(16, GetAlignment(&(aligned.by_16)));
  EXPECT_LE(32, GetAlignment(&(aligned.by_32)));
  EXPECT_LE(64, GetAlignment(&(aligned.by_64)));
  EXPECT_LE(128, GetAlignment(&(aligned.by_128)));
  EXPECT_LE(256, GetAlignment(&(aligned.by_256)));
}

TEST(SbAlignTest, AlignAsStackVariable) {
  char unaligned1 = 1;
  SB_ALIGNAS(2) char by_2;
  char unaligned2 = 2;
  EXPECT_NE(unaligned2, unaligned1);  // These are to try to keep the stack
                                      // variables around.
  SB_ALIGNAS(4) char by_4;
  char unaligned3 = 3;
  EXPECT_NE(unaligned3, unaligned1);
  SB_ALIGNAS(8) char by_8;
  char unaligned4 = 4;
  EXPECT_NE(unaligned4, unaligned1);
  SB_ALIGNAS(16) char by_16;
  char unaligned5 = 5;
  EXPECT_NE(unaligned5, unaligned1);

  EXPECT_LE(2, GetAlignment(&by_2));
  EXPECT_LE(4, GetAlignment(&by_4));
  EXPECT_LE(8, GetAlignment(&by_8));
  EXPECT_LE(16, GetAlignment(&by_16));

#if !SB_HAS_QUIRK(DOES_NOT_STACK_ALIGN_OVER_16_BYTES)
  SB_ALIGNAS(32) char by_32;
  char unaligned6 = 6;
  EXPECT_NE(unaligned6, unaligned1);
  SB_ALIGNAS(64) char by_64;
  char unaligned7 = 7;
  EXPECT_NE(unaligned7, unaligned1);
  SB_ALIGNAS(128) char by_128;
  char unaligned8 = 8;
  EXPECT_NE(unaligned8, unaligned1);
  SB_ALIGNAS(256) char by_256;
  char unaligned9 = 9;
  EXPECT_NE(unaligned9, unaligned1);

  EXPECT_LE(32, GetAlignment(&by_32));
  EXPECT_LE(64, GetAlignment(&by_64));
  EXPECT_LE(128, GetAlignment(&by_128));
  EXPECT_LE(256, GetAlignment(&by_256));
#endif  // !SB_HAS_QUIRK(DOES_NOT_STACK_ALIGN_OVER_16_BYTES)
}

TEST(SbAlignTest, AlignOf) {
  EXPECT_LE(sizeof(uint8_t), SB_ALIGNOF(uint8_t));
  EXPECT_LE(sizeof(uint16_t), SB_ALIGNOF(uint16_t));
  EXPECT_LE(sizeof(uint32_t), SB_ALIGNOF(uint32_t));
  EXPECT_LE(sizeof(uint64_t), SB_ALIGNOF(uint64_t));
  EXPECT_LE(sizeof(uintptr_t), SB_ALIGNOF(uintptr_t));
  EXPECT_LE(sizeof(void*), SB_ALIGNOF(void*));
}

TEST(SbAlignTest, AlignAsAlignOfStructFieldOnStack) {
  char unaligned = 0;
  EXPECT_NE(1, unaligned);

  AlignedFieldsOf aligned;
  EXPECT_LE(SB_ALIGNOF(void*), GetAlignment(&(aligned.by_void_star)));
  EXPECT_LE(SB_ALIGNOF(int), GetAlignment(&(aligned.by_int)));
  EXPECT_LE(SB_ALIGNOF(uint64_t), GetAlignment(&(aligned.by_uint64_t)));
}

TEST(SbAlignTest, AlignAsAlignOfStackVariable) {
  char unaligned = 0;
  EXPECT_NE(1, unaligned);
  SB_ALIGNAS(SB_ALIGNOF(void*)) char by_void_star;
  SB_ALIGNAS(SB_ALIGNOF(int)) char by_int;
  SB_ALIGNAS(SB_ALIGNOF(uint64_t)) char by_uint64_t;

  EXPECT_LE(SB_ALIGNOF(void*), GetAlignment(&by_void_star));
  EXPECT_LE(SB_ALIGNOF(int), GetAlignment(&by_int));
  EXPECT_LE(SB_ALIGNOF(uint64_t), GetAlignment(&by_uint64_t));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
