// Copyright (C) 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "protostore/crc32.h"

#include <zlib.h>

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace protostore {
namespace {

using ::testing::Eq;

TEST(Crc32Test, Get) {
  Crc32 crc32_test{10};
  Crc32 crc32_test_empty{};
  EXPECT_THAT(crc32_test.Get(), Eq(10));
  EXPECT_THAT(crc32_test_empty.Get(), Eq(0));
}

TEST(Crc32Test, Append) {
  // Test the complement logic inside Append()
  const uLong kCrcInitZero = crc32(0L, nullptr, 0);
  uint32_t foo_crc =
      crc32(kCrcInitZero, reinterpret_cast<const Bytef*>("foo"), 3);
  uint32_t foobar_crc =
      crc32(kCrcInitZero, reinterpret_cast<const Bytef*>("foobar"), 6);

  Crc32 crc32_test(~foo_crc);
  ASSERT_THAT(~crc32_test.Append("bar"), Eq(foobar_crc));

  // Test Append() that appending things separately should be the same as
  // appending in one shot
  Crc32 crc32_foobar{};
  crc32_foobar.Append("foobar");
  Crc32 crc32_foo_and_bar{};
  crc32_foo_and_bar.Append("foo");
  crc32_foo_and_bar.Append("bar");

  EXPECT_THAT(crc32_foo_and_bar.Get(), Eq(crc32_foobar.Get()));
}

}  // namespace
}  // namespace protostore
