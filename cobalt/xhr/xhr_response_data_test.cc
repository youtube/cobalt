/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/xhr/xhr_response_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace xhr {

namespace {

TEST(XhrResponseData, InitialState) {
  XhrResponseData empty;
  EXPECT_EQ(0, empty.size());
  EXPECT_EQ(0, empty.capacity());
  EXPECT_TRUE(empty.data() == NULL);
}

TEST(XhrResponseData, Append) {
  XhrResponseData data;
  uint8 raw_data[64];
  for (int i = 0; i < 64; ++i) {
    raw_data[i] = static_cast<uint8>(i);
  }
  data.Append(raw_data, 64);
  EXPECT_EQ(64, data.size());
  EXPECT_EQ(64, data.capacity());

  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(raw_data[i], data.data()[i]);
  }
}

TEST(XhrResponseData, Reserve) {
  XhrResponseData data;
  data.Reserve(1);
  EXPECT_EQ(1, data.capacity());
  EXPECT_EQ(0, data.size());
  EXPECT_TRUE(data.data() != NULL);
}

TEST(XhrResponseData, Pass) {
  XhrResponseData data;
  data.Reserve(55);
  for (int i = 0; i < 77; ++i) {
    uint8 raw_data = static_cast<uint8>(i);
    data.Append(&raw_data, 1);
  }
  EXPECT_EQ(77, data.capacity());
  EXPECT_EQ(77, data.size());

  scoped_array<uint8> new_owner = data.Pass();
  EXPECT_EQ(0, data.capacity());
  EXPECT_EQ(0, data.size());
  EXPECT_TRUE(data.data() == NULL);
  for (int i = 0; i < 77; ++i) {
    EXPECT_EQ(i, new_owner.get()[i]);
  }
}

}  // namespace
}  // namespace xhr
}  // namespace cobalt
