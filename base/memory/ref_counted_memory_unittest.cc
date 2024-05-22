// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"

#include <stdint.h>

#include <utility>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Each;
using testing::ElementsAre;

namespace base {

namespace {

void ConvertToByteSpanAndCheckSize(span<const uint8_t> data,
                                   size_t expected_size) {
  EXPECT_EQ(expected_size, data.size());
}

}  // namespace

TEST(RefCountedMemoryUnitTest, RefCountedStaticMemory) {
  auto mem = MakeRefCounted<RefCountedStaticMemory>("static mem00", 10);

  EXPECT_EQ(10U, mem->size());
  EXPECT_EQ("static mem", std::string(mem->front_as<char>(), mem->size()));

  ConvertToByteSpanAndCheckSize(*mem, 10);
}

TEST(RefCountedMemoryUnitTest, RefCountedBytes) {
  std::vector<uint8_t> data;
  data.push_back(45);
  data.push_back(99);
  scoped_refptr<RefCountedMemory> mem = RefCountedBytes::TakeVector(&data);

  EXPECT_EQ(0U, data.size());

  ASSERT_EQ(2U, mem->size());
  EXPECT_EQ(45U, mem->front()[0]);
  EXPECT_EQ(99U, mem->front()[1]);

  scoped_refptr<RefCountedMemory> mem2;
  {
    const unsigned char kData[] = {12, 11, 99};
    mem2 = MakeRefCounted<RefCountedBytes>(kData, std::size(kData));
  }
  ASSERT_EQ(3U, mem2->size());
  EXPECT_EQ(12U, mem2->front()[0]);
  EXPECT_EQ(11U, mem2->front()[1]);
  EXPECT_EQ(99U, mem2->front()[2]);

  ConvertToByteSpanAndCheckSize(*mem2, 3);
}

TEST(RefCountedMemoryUnitTest, RefCountedBytesMutable) {
  auto mem = MakeRefCounted<RefCountedBytes>(10);

  ASSERT_EQ(10U, mem->size());
  EXPECT_THAT(mem->data(), Each(0U));

  // Test non-const versions of data(), front() and front_as<>().
  mem->data()[0] = 1;
  mem->front()[1] = 2;
  mem->front_as<char>()[2] = 3;

  EXPECT_THAT(mem->data(), ElementsAre(1, 2, 3, 0, 0, 0, 0, 0, 0, 0));
}

TEST(RefCountedMemoryUnitTest, RefCountedString) {
  scoped_refptr<RefCountedMemory> mem =
      base::MakeRefCounted<base::RefCountedString>(std::string("destroy me"));

  ASSERT_EQ(10U, mem->size());
  EXPECT_EQ('d', mem->front()[0]);
  EXPECT_EQ('e', mem->front()[1]);
  EXPECT_EQ('e', mem->front()[9]);

  ConvertToByteSpanAndCheckSize(*mem, 10);
}

TEST(RefCountedMemoryUnitTest, Equals) {
  scoped_refptr<RefCountedMemory> mem1 =
      base::MakeRefCounted<base::RefCountedString>(std::string("same"));

  std::vector<unsigned char> d2 = {'s', 'a', 'm', 'e'};
  scoped_refptr<RefCountedMemory> mem2 = RefCountedBytes::TakeVector(&d2);

  EXPECT_TRUE(mem1->Equals(mem2));

  std::string s3("diff");
  scoped_refptr<RefCountedMemory> mem3 =
      base::MakeRefCounted<base::RefCountedString>(std::move(s3));

  EXPECT_FALSE(mem1->Equals(mem3));
  EXPECT_FALSE(mem2->Equals(mem3));
}

TEST(RefCountedMemoryUnitTest, EqualsNull) {
  std::string s("str");
  scoped_refptr<RefCountedMemory> mem =
      base::MakeRefCounted<base::RefCountedString>(std::move(s));
  EXPECT_FALSE(mem->Equals(nullptr));
}

}  //  namespace base
