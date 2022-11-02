/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jscustomallocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const size_t kSize = 1024 * 128;

TEST(JSCustomallocator, JSMalloc) {
  void* memory = js_malloc(kSize);
  EXPECT_NE(static_cast<void*>(NULL), memory);
  ssize_t current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated,
            AllocationMetadata::GetReservationBytes(kSize));
  js_free(memory);
  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated, 0);
}

TEST(JSCustomallocator, JSCallocWithNumber) {
  void* memory = js_calloc(5, kSize);
  EXPECT_NE(static_cast<void*>(NULL), memory);
  ssize_t current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated,
            AllocationMetadata::GetReservationBytes(5 * kSize));
  js_free(memory);
  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated, 0);
}

TEST(JSCustomallocator, JSCalloc) {
  void* memory = js_calloc(kSize);
  EXPECT_NE(static_cast<void*>(NULL), memory);
  ssize_t current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated,
            AllocationMetadata::GetReservationBytes(kSize));
  js_free(memory);
  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated, 0);
}

TEST(JSCustomallocator, JSReallocSmaller) {
  void* memory = js_malloc(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = i;
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  ssize_t current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated,
            AllocationMetadata::GetReservationBytes(kSize));

  memory = js_realloc(memory, kSize / 2);
  data = static_cast<char*>(memory);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  for (int i = 0; i < kSize / 2; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated,
            AllocationMetadata::GetReservationBytes(kSize / 2));

  js_free(memory);

  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated, 0);
}

TEST(JSCustomallocator, JSReallocBigger) {
  void* memory = js_malloc(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    data[i] = i;
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  ssize_t current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated,
            AllocationMetadata::GetReservationBytes(kSize));

  memory = js_realloc(memory, kSize * 2);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  data = static_cast<char*>(memory);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  for (int i = kSize; i < kSize * 2; ++i) {
    data[i] = i;
  }

  for (int i = kSize; i < kSize * 2; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated,
            AllocationMetadata::GetReservationBytes(kSize * 2));

  js_free(memory);

  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated, 0);
}

TEST(JSCustomallocator, JSReallocNULL) {
  void* memory = js_realloc(NULL, kSize);
  EXPECT_NE(static_cast<void*>(NULL), memory);
  ssize_t current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated,
            AllocationMetadata::GetReservationBytes(kSize));

  js_free(memory);

  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated, 0);
}

TEST(JSCustomallocator, JSStrdup) {
  const char* input = "abcedfg123456";
  char* dupe = js_strdup(const_cast<char*>(input));
  const char* kNull = NULL;
  EXPECT_NE(kNull, dupe);
  EXPECT_EQ(0, SbStringCompareNoCase(input, dupe));
  EXPECT_EQ(SbStringGetLength(input), SbStringGetLength(dupe));

  ssize_t current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated, AllocationMetadata::GetReservationBytes(
                                         SbStringGetLength(dupe) + 1));

  js_free(dupe);

  current_bytes_allocated =
      MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
  EXPECT_EQ(current_bytes_allocated, 0);
}

}  // namespace
