// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <utility>

#include "starboard/shared/starboard/player/buffer_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(BufferTest, DefaultCtor) {
  Buffer buffer;
  EXPECT_EQ(buffer.size(), 0);
}

TEST(BufferTest, Allocate) {
  Buffer buffer(128);
  ASSERT_EQ(buffer.size(), 128);
  ASSERT_NE(buffer.data(), nullptr);
  memset(buffer.data(), 'x', 128);
}

TEST(BufferTest, CopyCtor) {
  Buffer original(128);
  memset(original.data(), 'x', 128);

  Buffer copy(original);
  ASSERT_EQ(copy.size(), original.size());
  ASSERT_NE(copy.data(), nullptr);

  for (int i = 0; i < original.size(); ++i) {
    ASSERT_EQ(original.data()[i], copy.data()[i]);
  }

  memset(copy.data(), 'y', 128);

  for (int i = 0; i < original.size(); ++i) {
    ASSERT_EQ(original.data()[i], 'x');
    ASSERT_EQ(copy.data()[i], 'y');
  }
}

TEST(BufferTest, CopyEmptyBuffer) {
  Buffer original;
  Buffer copy(original);
  EXPECT_EQ(copy.size(), 0);
  EXPECT_EQ(copy.data(), nullptr);
}

TEST(BufferTest, MoveCtor) {
  Buffer original(128);
  memset(original.data(), 'x', 128);

  const uint8_t* original_data_pointer = original.data();

  Buffer copy(std::move(original));
  ASSERT_EQ(copy.size(), 128);
  ASSERT_NE(copy.data(), nullptr);
  ASSERT_EQ(copy.data(), original_data_pointer);

  for (int i = 0; i < copy.size(); ++i) {
    ASSERT_EQ(copy.data()[i], 'x');
  }
}

TEST(BufferTest, MoveAssignmentOperator) {
  Buffer original(128);
  memset(original.data(), 'x', 128);

  const uint8_t* original_data_pointer = original.data();

  Buffer copy;

  copy = std::move(original);
  ASSERT_EQ(copy.size(), 128);
  ASSERT_NE(copy.data(), nullptr);
  ASSERT_EQ(copy.data(), original_data_pointer);

  for (int i = 0; i < copy.size(); ++i) {
    ASSERT_EQ(copy.data()[i], 'x');
  }
}

TEST(BufferTest, AsSpanEmpty) {
  Buffer empty;
  EXPECT_TRUE(empty.as_span().empty());
  EXPECT_EQ(empty.as_span().data(), nullptr);

  const Buffer& const_empty = empty;
  EXPECT_TRUE(const_empty.as_span().empty());
  EXPECT_EQ(const_empty.as_span().data(), nullptr);
}

TEST(BufferTest, AsSpanAndImplicitConversion) {
  Buffer buffer(64);
  span<uint8_t> mutable_span = buffer.as_span();
  ASSERT_EQ(mutable_span.size(), 64U);
  EXPECT_EQ(mutable_span.data(), buffer.data());

  for (size_t i = 0; i < mutable_span.size(); ++i) {
    mutable_span[i] = static_cast<uint8_t>(i);
  }

  const Buffer& const_buffer = buffer;
  span<const uint8_t> const_span = const_buffer.as_span();
  ASSERT_EQ(const_span.size(), 64U);
  EXPECT_EQ(const_span.data(), const_buffer.data());

  // Verify implicit container conversion to span<const uint8_t>.
  auto read_via_span = [](span<const uint8_t> s) { return s.subspan(10, 4); };
  span<const uint8_t> slice = read_via_span(const_buffer);
  ASSERT_EQ(slice.size(), 4U);
  EXPECT_EQ(slice[0], 10U);
  EXPECT_EQ(slice[3], 13U);
}

}  // namespace
}  // namespace starboard
