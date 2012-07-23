// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_ALIGNED(ptr, align) \
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(ptr) & (align - 1))

namespace {

using base::AlignedMemory;

TEST(AlignedMemoryTest, StaticAlignment) {
  static AlignedMemory<8, 8> raw8;
  static AlignedMemory<8, 16> raw16;
  static AlignedMemory<8, 256> raw256;
  static AlignedMemory<8, 4096> raw4096;

  EXPECT_EQ(8u, ALIGNOF(raw8));
  EXPECT_EQ(16u, ALIGNOF(raw16));
  EXPECT_EQ(256u, ALIGNOF(raw256));
  EXPECT_EQ(4096u, ALIGNOF(raw4096));

  EXPECT_ALIGNED(raw8.void_data(), 8);
  EXPECT_ALIGNED(raw16.void_data(), 16);
  EXPECT_ALIGNED(raw256.void_data(), 256);
  EXPECT_ALIGNED(raw4096.void_data(), 4096);
}

TEST(AlignedMemoryTest, StackAlignment) {
  AlignedMemory<8, 8> raw8;
  AlignedMemory<8, 16> raw16;
  AlignedMemory<8, 256> raw256;
  AlignedMemory<8, 4096> raw4096;

  EXPECT_EQ(8u, ALIGNOF(raw8));
  EXPECT_EQ(16u, ALIGNOF(raw16));
  EXPECT_EQ(256u, ALIGNOF(raw256));
  EXPECT_EQ(4096u, ALIGNOF(raw4096));

  EXPECT_ALIGNED(raw8.void_data(), 8);
  EXPECT_ALIGNED(raw16.void_data(), 16);
  EXPECT_ALIGNED(raw256.void_data(), 256);
  EXPECT_ALIGNED(raw4096.void_data(), 4096);
}

}  // namespace
