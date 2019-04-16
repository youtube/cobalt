// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "cobalt/base/c_val_collection_entry_stats.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace base {

TEST(CValCollectionEntryStatsTest, DefaultValues) {
  const std::string name = "CollectionEntry";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = false;

  base::CValCollectionEntryStats<int> cval(name, max_size,
                                           enable_entry_list_c_val);

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> pct25 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.25th", name.c_str()));
  base::Optional<std::string> pct50 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.50th", name.c_str()));
  base::Optional<std::string> pct75 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.75th", name.c_str()));
  base::Optional<std::string> pct95 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.95th", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));
  base::Optional<std::string> entry_list = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.EntryList", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "0");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "0");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "0");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "0");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "0");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "0");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "0");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "0");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0");
  EXPECT_FALSE(entry_list);
}

TEST(CValCollectionEntryStatsTest, NoFlush) {
  const std::string name = "CollectionEntryStats";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = true;

  base::CValCollectionEntryStats<int> cval(name, max_size,
                                           enable_entry_list_c_val);
  cval.AddEntry(3);
  cval.AddEntry(9);
  cval.AddEntry(1);
  cval.AddEntry(7);

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> pct25 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.25th", name.c_str()));
  base::Optional<std::string> pct50 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.50th", name.c_str()));
  base::Optional<std::string> pct75 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.75th", name.c_str()));
  base::Optional<std::string> pct95 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.95th", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));
  base::Optional<std::string> entry_list = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.EntryList", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "0");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "0");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "0");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "0");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "0");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "0");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "0");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "0");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0");
  EXPECT_TRUE(entry_list);
  EXPECT_EQ(*entry_list, "[]");
}

TEST(CValCollectionEntryStatsTest, MaxSizeFlush) {
  const std::string name = "CollectionEntryStats";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = true;

  base::CValCollectionEntryStats<int> cval(name, max_size,
                                           enable_entry_list_c_val);
  cval.AddEntry(3);
  cval.AddEntry(9);
  cval.AddEntry(1);
  cval.AddEntry(7);
  cval.AddEntry(5);

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> pct25 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.25th", name.c_str()));
  base::Optional<std::string> pct50 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.50th", name.c_str()));
  base::Optional<std::string> pct75 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.75th", name.c_str()));
  base::Optional<std::string> pct95 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.95th", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));
  base::Optional<std::string> entry_list = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.EntryList", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "5");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "5");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "1");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "9");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "3");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "5");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "7");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "8");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "3");
  EXPECT_TRUE(entry_list);
  EXPECT_EQ(*entry_list, "[3, 9, 1, 7, 5]");
}

TEST(CValCollectionEntryStatsTest, ManualFlush) {
  const std::string name = "CollectionEntryStats";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = true;

  base::CValCollectionEntryStats<int> cval(name, max_size,
                                           enable_entry_list_c_val);
  cval.AddEntry(3);
  cval.AddEntry(9);
  cval.AddEntry(1);
  cval.AddEntry(7);
  cval.Flush();
  cval.AddEntry(5);

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> pct25 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.25th", name.c_str()));
  base::Optional<std::string> pct50 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.50th", name.c_str()));
  base::Optional<std::string> pct75 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.75th", name.c_str()));
  base::Optional<std::string> pct95 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.95th", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));
  base::Optional<std::string> entry_list = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.EntryList", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "4");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "5");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "1");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "9");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "2");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "5");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "7");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "8");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "3");
  EXPECT_TRUE(entry_list);
  EXPECT_EQ(*entry_list, "[3, 9, 1, 7]");
}

TEST(CValCollectionEntryStatsTest, TwoManualFlushes) {
  const std::string name = "CollectionEntryStats";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = true;

  base::CValCollectionEntryStats<int> cval(name, max_size,
                                           enable_entry_list_c_val);
  cval.AddEntry(3);
  cval.AddEntry(9);
  cval.AddEntry(1);
  cval.AddEntry(7);
  cval.Flush();
  cval.AddEntry(5);
  cval.Flush();

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> pct25 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.25th", name.c_str()));
  base::Optional<std::string> pct50 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.50th", name.c_str()));
  base::Optional<std::string> pct75 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.75th", name.c_str()));
  base::Optional<std::string> pct95 = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.Pct.95th", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));
  base::Optional<std::string> entry_list = cvm->GetValueAsPrettyString(
      base::StringPrintf("%s.EntryList", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "1");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "5");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "5");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "5");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "5");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "5");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "5");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "5");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0");
  EXPECT_TRUE(entry_list);
  EXPECT_EQ(*entry_list, "[5]");
}

}  // namespace base
