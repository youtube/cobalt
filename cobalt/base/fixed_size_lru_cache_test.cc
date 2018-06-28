// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/fixed_size_lru_cache.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class EmptyDeleter {
 public:
  void operator()(double) {}
};
class CacheTest : public ::testing::Test {
 public:
  FixedSizeLRUCache<int, double, 4, EmptyDeleter> cache;
};

TEST_F(CacheTest, Create) {
  EXPECT_EQ(cache.size(), 0);
  EXPECT_TRUE(cache.empty());
}

TEST_F(CacheTest, Add3) {
  cache.put(11, -1.0);
  cache.put(12, -2.0);
  cache.put(13, -3.0);

  EXPECT_EQ(cache.find(10), cache.end());
  EXPECT_NE(cache.find(11), cache.end());
  EXPECT_NE(cache.find(12), cache.end());
  EXPECT_NE(cache.find(13), cache.end());
  EXPECT_DOUBLE_EQ(cache.find(11)->value, -1.0);
  EXPECT_DOUBLE_EQ(cache.find(12)->value, -2.0);
  EXPECT_DOUBLE_EQ(cache.find(13)->value, -3.0);
}

TEST_F(CacheTest, Replacement) {
  cache.put(11, -1.0);
  cache.put(12, -2.0);
  cache.put(12, -3.0);

  EXPECT_EQ(cache.find(10), cache.end());
  EXPECT_NE(cache.find(11), cache.end());
  EXPECT_NE(cache.find(12), cache.end());
  EXPECT_DOUBLE_EQ(cache.find(11)->value, -1.0);
  EXPECT_DOUBLE_EQ(cache.find(12)->value, -3.0);
}

TEST_F(CacheTest, Evict) {
  cache.put(11, -1.0);
  cache.put(12, -2.0);
  cache.put(13, -3.0);
  cache.put(14, -4.0);
  cache.find(11);

  // This should evict 12, since we're a LRU cache.
  cache.put(15, -5.0);

  EXPECT_EQ(cache.find(10), cache.end());
  EXPECT_EQ(cache.find(12), cache.end());
  EXPECT_NE(cache.find(11), cache.end());
  EXPECT_NE(cache.find(13), cache.end());
  EXPECT_NE(cache.find(14), cache.end());
  EXPECT_NE(cache.find(15), cache.end());

  EXPECT_DOUBLE_EQ(cache.find(11)->value, -1.0);
  EXPECT_DOUBLE_EQ(cache.find(13)->value, -3.0);
  EXPECT_DOUBLE_EQ(cache.find(14)->value, -4.0);
  EXPECT_DOUBLE_EQ(cache.find(15)->value, -5.0);
}

}  // namespace
}  // namespace base
