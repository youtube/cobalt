// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
#include "base/strings/string_split.h"
#include "starboard/memory.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace overlay_info {

namespace {

typedef std::pair<std::string, std::string> ValuePair;

// See comment of OverlayInfoRegistry for format of |info|.
bool ParseOverlayInfo(std::string infos, std::vector<ValuePair>* values) {
  CHECK(values);

  const char delimiter[] = {OverlayInfoRegistry::kDelimiter, 0};

  auto tokens = base::SplitString(infos, delimiter, base::KEEP_WHITESPACE,
                                  base::SPLIT_WANT_ALL);
  if (tokens.size() % 2 != 0) {
    return false;
  }

  while (!tokens.empty()) {
    values->push_back(std::make_pair(tokens[0], tokens[1]));
    tokens.erase(tokens.begin(), tokens.begin() + 2);
  }

  return true;
}

}  // namespace

TEST(OverlayInfoRegistryTest, RetrieveOnEmptyData) {
  std::string infos("a");
  OverlayInfoRegistry::RetrieveAndClear(&infos);
  EXPECT_TRUE(infos.empty());

  std::string infos1("b");
  OverlayInfoRegistry::RetrieveAndClear(&infos1);
  EXPECT_TRUE(infos1.empty());
}

TEST(OverlayInfoRegistryTest, RegisterSingleStringAndRetrieve) {
  const char kCategory[] = "category";
  const size_t kMaxStringLength = 20;

  for (size_t i = 0; i < kMaxStringLength; ++i) {
    std::string value(i, 'q');

    OverlayInfoRegistry::Register(kCategory, value.c_str());

    std::string infos("a");
    OverlayInfoRegistry::RetrieveAndClear(&infos);
    std::vector<ValuePair> parsed_infos;
    ASSERT_TRUE(ParseOverlayInfo(infos, &parsed_infos));
    EXPECT_EQ(parsed_infos.size(), 1);
    EXPECT_EQ(parsed_infos[0].first, kCategory);
    EXPECT_EQ(parsed_infos[0].second, value);

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

  std::string infos("a");
  OverlayInfoRegistry::RetrieveAndClear(&infos);
  std::vector<ValuePair> parsed_infos;
  ASSERT_TRUE(ParseOverlayInfo(infos, &parsed_infos));
  OverlayInfoRegistry::RetrieveAndClear(&infos);
  EXPECT_TRUE(infos.empty());

  ASSERT_EQ(parsed_infos.size(), kMaxStringLength);

  for (size_t i = 0; i < kMaxStringLength; ++i) {
    EXPECT_EQ(parsed_infos[i].first, kCategory);
    EXPECT_EQ(parsed_infos[i].second, values[i]);
  }
}

TEST(OverlayInfoRegistryTest, RegisterMultipleTypes) {
  OverlayInfoRegistry::Register("string", "string_value");
  OverlayInfoRegistry::Register("int", -12345);
  OverlayInfoRegistry::Register("uint64", 123456789012u);

  std::string infos("a");
  OverlayInfoRegistry::RetrieveAndClear(&infos);

  EXPECT_EQ(infos, "string,string_value,int,-12345,uint64,123456789012");
}

}  // namespace overlay_info
}  // namespace cobalt
