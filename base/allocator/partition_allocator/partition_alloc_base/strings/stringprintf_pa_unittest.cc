// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/strings/stringprintf.h"

#include <errno.h>
#include <stddef.h>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::base {

TEST(PartitionAllocStringPrintfTest, TruncatingStringPrintfEmpty) {
  EXPECT_EQ("", TruncatingStringPrintf("%s", ""));
}

TEST(PartitionAllocStringPrintfTest, TruncatingStringPrintfMisc) {
  EXPECT_EQ("123hello w",
            TruncatingStringPrintf("%3d%2s %1c", 123, "hello", 'w'));
}

// Test that TruncatingStringPrintf truncates too long result.
// The original TruncatingStringPrintf does not truncate. Instead, it allocates
// memory and returns an entire result.
TEST(PartitionAllocStringPrintfTest, TruncatingStringPrintfTruncatesResult) {
  std::vector<char> buffer;
  buffer.resize(kMaxLengthOfTruncatingStringPrintfResult + 1);
  std::fill(buffer.begin(), buffer.end(), 'a');
  buffer.push_back('\0');
  std::string result = TruncatingStringPrintf("%s", buffer.data());
  EXPECT_EQ(kMaxLengthOfTruncatingStringPrintfResult, result.length());
  EXPECT_EQ(std::string::npos, result.find_first_not_of('a'));
}

// Test that TruncatingStringPrintf does not change errno.
TEST(PartitionAllocStringPrintfTest, TruncatingStringPrintfErrno) {
  errno = 1;
  EXPECT_EQ("", TruncatingStringPrintf("%s", ""));
  EXPECT_EQ(1, errno);
}

}  // namespace partition_alloc::internal::base
