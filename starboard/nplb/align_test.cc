// Copyright 2017 Google Inc. All Rights Reserved.
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
  char unaligned;
  SB_ALIGNAS(2) char by_2;
  SB_ALIGNAS(4) char by_4;
  SB_ALIGNAS(8) char by_8;
  SB_ALIGNAS(16) char by_16;
  SB_ALIGNAS(32) char by_32;
  SB_ALIGNAS(64) char by_64;
};

struct AlignedFieldsOf {
  char unaligned;
  SB_ALIGNAS(SB_ALIGNOF(void*)) char by_void_star;
  SB_ALIGNAS(SB_ALIGNOF(int)) char by_int;
  SB_ALIGNAS(SB_ALIGNOF(uint64_t)) char by_uint64_t;
};

TEST(SbAlignTest, AlignAsStructFieldOnHeap) {
  scoped_ptr<AlignedFields> aligned(new AlignedFields());
  EXPECT_LE(2, GetAlignment(&(aligned->by_2)));
  EXPECT_LE(4, GetAlignment(&(aligned->by_4)));
  EXPECT_LE(8, GetAlignment(&(aligned->by_8)));
  EXPECT_LE(16, GetAlignment(&(aligned->by_16)));
  EXPECT_LE(32, GetAlignment(&(aligned->by_32)));
  EXPECT_LE(64, GetAlignment(&(aligned->by_64)));
}

TEST(SbAlignTest, AlignAsStructFieldOnStack) {
  AlignedFields aligned;
  EXPECT_LE(2, GetAlignment(&(aligned.by_2)));
  EXPECT_LE(4, GetAlignment(&(aligned.by_4)));
  EXPECT_LE(8, GetAlignment(&(aligned.by_8)));
  EXPECT_LE(16, GetAlignment(&(aligned.by_16)));
  EXPECT_LE(32, GetAlignment(&(aligned.by_32)));
  EXPECT_LE(64, GetAlignment(&(aligned.by_64)));
}

TEST(SbAlignTest, AlignAsStackVariable) {
  SB_ALIGNAS(2) char by_2;
  SB_ALIGNAS(4) char by_4;
  SB_ALIGNAS(8) char by_8;
  SB_ALIGNAS(16) char by_16;

  EXPECT_LE(2, GetAlignment(&by_2));
  EXPECT_LE(4, GetAlignment(&by_4));
  EXPECT_LE(8, GetAlignment(&by_8));
  EXPECT_LE(16, GetAlignment(&by_16));

#if !SB_HAS_QUIRK(DOES_NOT_STACK_ALIGN_OVER_16_BYTES)
  SB_ALIGNAS(32) char by_32;
  SB_ALIGNAS(64) char by_64;

  EXPECT_LE(32, GetAlignment(&by_32));
  EXPECT_LE(64, GetAlignment(&by_64));
#endif
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
  AlignedFieldsOf aligned;
  EXPECT_LE(SB_ALIGNOF(void*), GetAlignment(&(aligned.by_void_star)));
  EXPECT_LE(SB_ALIGNOF(int), GetAlignment(&(aligned.by_int)));
  EXPECT_LE(SB_ALIGNOF(uint64_t), GetAlignment(&(aligned.by_uint64_t)));
}

TEST(SbAlignTest, AlignAsAlignOfStructFieldOnHeap) {
  scoped_ptr<AlignedFieldsOf> aligned(new AlignedFieldsOf());
  EXPECT_LE(SB_ALIGNOF(void*), GetAlignment(&(aligned->by_void_star)));
  EXPECT_LE(SB_ALIGNOF(int), GetAlignment(&(aligned->by_int)));
  EXPECT_LE(SB_ALIGNOF(uint64_t), GetAlignment(&(aligned->by_uint64_t)));
}

TEST(SbAlignTest, AlignAsAlignOfStackVariable) {
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
