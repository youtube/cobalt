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

#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

const size_t kSize = 1024 * 128;

TEST(PosixMallocTest, AllocatesAndFreesMemory) {
  const size_t size = 1024;
  void* mem = malloc(size);
  ASSERT_NE(mem, nullptr);

  memset(mem, 0xAB, size);
  EXPECT_EQ(static_cast<unsigned char*>(mem)[0], 0xAB);
  EXPECT_EQ(static_cast<unsigned char*>(mem)[size - 1], 0xAB);

  free(mem);
}

TEST(PosixMallocTest, AllocatesZero) {
  // malloc(0) may return either a NULL pointer or a unique
  // pointer that can be successfully passed to free().
  void* mem = malloc(0);

  if (mem != nullptr) {
    free(mem);
  }
}

TEST(PosixMallocTest, MallocAlignment) {
  // POSIX guarantees that the returned address is suitably aligned
  // for any built-in type.
  void* ptr = malloc(sizeof(long double));
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(long double), 0u);
  free(ptr);

  ptr = malloc(sizeof(long long));
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(long long), 0u);
  free(ptr);

  ptr = malloc(sizeof(void*));
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(void*), 0u);
  free(ptr);
}

TEST(PosixMallocTest, CanReadWriteToResult) {
  void* memory = malloc(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (size_t i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  for (size_t i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  free(memory);
}

}  // namespace
}  // namespace starboard::nplb
