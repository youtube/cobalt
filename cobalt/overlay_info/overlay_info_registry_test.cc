// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/overlay_info/overlay_info_registry.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "starboard/memory.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace overlay_info {

namespace {

typedef std::pair<std::string, std::vector<uint8_t>> ValuePair;

// See comment of OverlayInfoRegistry for format of |info|.
std::vector<ValuePair> ParseOverlayInfo(std::vector<uint8_t> info) {
  std::vector<ValuePair> parsed_infos;

  while (!info.empty()) {
    // Parse the size
    size_t size = info[0];
    info.erase(info.begin());

    CHECK_LE(size, info.size());

    // Parse the category name
    const auto kDelimiter = OverlayInfoRegistry::kDelimiter;
    auto iter = std::find(info.begin(), info.end(), kDelimiter);
    CHECK(iter != info.end());

    const auto category_length = iter - info.begin();
    CHECK_LE(static_cast<size_t>(category_length), size);

    std::string category(
        reinterpret_cast<char*>(info.data()),
        reinterpret_cast<char*>(info.data()) + category_length);

    // Parse the data
    std::vector<uint8_t> data(info.begin() + category_length + 1,
                              info.begin() + size);
    info.erase(info.begin(), info.begin() + size);

    parsed_infos.push_back(std::make_pair(category, data));
  }

  CHECK(info.empty());
  return parsed_infos;
}

bool IsSame(const std::vector<uint8_t>& data, const std::string& str) {
  return data.size() == str.size() &&
         SbMemoryCompare(data.data(), str.c_str(), data.size()) == 0;
}

}  // namespace

TEST(OverlayInfoRegistryTest, RetrieveOnEmptyData) {
  std::vector<uint8_t> infos('a');
  OverlayInfoRegistry::RetrieveAndClear(&infos);
  EXPECT_TRUE(infos.empty());

  std::vector<uint8_t> infos1('a');
  OverlayInfoRegistry::RetrieveAndClear(&infos1);
  EXPECT_TRUE(infos1.empty());
}

TEST(OverlayInfoRegistryTest, RegisterSingleStringAndRetrieve) {
  const char kCategory[] = "category";
  const size_t kMaxStringLength = 20;

  for (size_t i = 0; i < kMaxStringLength; ++i) {
    std::string value(i, 'q');

    OverlayInfoRegistry::Register(kCategory, value.c_str());

    std::vector<uint8_t> infos('a');
    OverlayInfoRegistry::RetrieveAndClear(&infos);
    auto parsed_infos = ParseOverlayInfo(infos);
    EXPECT_EQ(parsed_infos.size(), 1);
    EXPECT_EQ(parsed_infos[0].first, kCategory);
    EXPECT_TRUE(IsSame(parsed_infos[0].second, value));

    OverlayInfoRegistry::RetrieveAndClear(&infos);
    EXPECT_TRUE(infos.empty());
  }
}

TEST(OverlayInfoRegistryTest, RegisterSingleBinaryStringAndRetrieve) {
  const char kCategory[] = "category";
  const size_t kMaxDataSize = 20;

  for (size_t i = 0; i < kMaxDataSize; ++i) {
    std::vector<uint8_t> value(i, static_cast<uint8_t>(i % 2));

    OverlayInfoRegistry::Register(kCategory, value.data(), value.size());

    std::vector<uint8_t> infos('a');
    OverlayInfoRegistry::RetrieveAndClear(&infos);
    auto parsed_infos = ParseOverlayInfo(infos);
    EXPECT_EQ(parsed_infos.size(), 1);
    EXPECT_EQ(parsed_infos[0].first, kCategory);
    EXPECT_TRUE(parsed_infos[0].second == value);

    OverlayInfoRegistry::RetrieveAndClear(&infos);
    EXPECT_TRUE(infos.empty());
  }
}

TEST(OverlayInfoRegistryTest, RegisterMultipleStringsAndRetrieve) {
  const char kCategory[] = "c";
  const size_t kMaxStringLength = 20;

  std::vector<std::string> values;

  for (size_t i = 0; i < kMaxStringLength; ++i) {
    values.push_back(std::string(i, 'x'));

    OverlayInfoRegistry::Register(kCategory, values.back().c_str());
  }

  std::vector<uint8_t> infos('a');
  OverlayInfoRegistry::RetrieveAndClear(&infos);
  auto parsed_infos = ParseOverlayInfo(infos);
  OverlayInfoRegistry::RetrieveAndClear(&infos);
  EXPECT_TRUE(infos.empty());

  ASSERT_EQ(parsed_infos.size(), kMaxStringLength);

  for (size_t i = 0; i < kMaxStringLength; ++i) {
    EXPECT_EQ(parsed_infos[i].first, kCategory);
    EXPECT_TRUE(IsSame(parsed_infos[i].second, values[i]));
  }
}

TEST(OverlayInfoRegistryTest, RegisterMultipleBinaryDataAndRetrieve) {
  const char kCategory[] = "c";
  const size_t kMaxDataSize = 20;

  std::vector<std::vector<uint8_t>> values;

  for (size_t i = 0; i < kMaxDataSize; ++i) {
    values.push_back(std::vector<uint8_t>(i, static_cast<uint8_t>(i % 2)));

    OverlayInfoRegistry::Register(kCategory, values.back().data(),
                                  values.back().size());
  }

  std::vector<uint8_t> infos('a');
  OverlayInfoRegistry::RetrieveAndClear(&infos);
  auto parsed_infos = ParseOverlayInfo(infos);
  OverlayInfoRegistry::RetrieveAndClear(&infos);
  EXPECT_TRUE(infos.empty());

  ASSERT_EQ(parsed_infos.size(), kMaxDataSize);

  for (size_t i = 0; i < kMaxDataSize; ++i) {
    EXPECT_EQ(parsed_infos[i].first, kCategory);
    EXPECT_TRUE(parsed_infos[i].second == values[i]);
  }
}

}  // namespace overlay_info
}  // namespace cobalt
