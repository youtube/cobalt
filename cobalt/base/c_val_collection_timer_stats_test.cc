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

#include "base/time/time.h"
#include "cobalt/base/c_val_collection_timer_stats.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace base {

TEST(CValCollectionTimerStatsTest, DefaultValues) {
  const std::string name = "CollectionTimerStats";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = false;

  base::CValCollectionTimerStats<> cval(name, max_size,
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
  EXPECT_EQ(*avg, "0us");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "0us");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "0us");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "0us");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "0us");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "0us");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "0us");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0us");
  EXPECT_FALSE(entry_list);
}

TEST(CValCollectionTimerStatsTest, NoFlush) {
  const std::string name = "CollectionTimerStats";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = true;

  base::CValCollectionTimerStats<> cval(name, max_size,
                                        enable_entry_list_c_val);
  cval.Start(base::TimeTicks::FromInternalValue(1000));
  cval.Stop(base::TimeTicks::FromInternalValue(4000));
  cval.Start(base::TimeTicks::FromInternalValue(4000));
  cval.Stop(base::TimeTicks::FromInternalValue(13000));
  cval.Start(base::TimeTicks::FromInternalValue(13000));
  cval.Stop(base::TimeTicks::FromInternalValue(14000));
  cval.Start(base::TimeTicks::FromInternalValue(14000));
  cval.Stop(base::TimeTicks::FromInternalValue(21000));

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
  EXPECT_EQ(*avg, "0us");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "0us");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "0us");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "0us");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "0us");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "0us");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "0us");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0us");
  EXPECT_TRUE(entry_list);
  EXPECT_EQ(*entry_list, "[]");
}

TEST(CValCollectionTimerStatsTest, MaxSizeFlush) {
  const std::string name = "CollectionTimerStats";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = true;

  base::CValCollectionTimerStats<> cval(name, max_size,
                                        enable_entry_list_c_val);
  cval.Start(base::TimeTicks::FromInternalValue(1000));
  cval.Stop(base::TimeTicks::FromInternalValue(4000));
  cval.Start(base::TimeTicks::FromInternalValue(4000));
  cval.Stop(base::TimeTicks::FromInternalValue(13000));
  cval.Start(base::TimeTicks::FromInternalValue(13000));
  cval.Stop(base::TimeTicks::FromInternalValue(14000));
  cval.Start(base::TimeTicks::FromInternalValue(14000));
  cval.Stop(base::TimeTicks::FromInternalValue(21000));
  cval.Start(base::TimeTicks::FromInternalValue(21000));
  cval.Stop(base::TimeTicks::FromInternalValue(26000));
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
  EXPECT_EQ(*count, "5");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "5.0ms");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "1000us");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "9.0ms");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "3.0ms");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "5.0ms");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "7.0ms");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "8.6ms");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "3.2ms");
  EXPECT_TRUE(entry_list);
  EXPECT_EQ(*entry_list, "[3000, 9000, 1000, 7000, 5000]");
}

TEST(CValCollectionTimerStatsTest, ManualFlush) {
  const std::string name = "CollectionTimerStats";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = true;

  base::CValCollectionTimerStats<CValPublic> cval(name, max_size,
                                                  enable_entry_list_c_val);
  cval.Start(base::TimeTicks::FromInternalValue(1000));
  cval.Stop(base::TimeTicks::FromInternalValue(4000));
  cval.Start(base::TimeTicks::FromInternalValue(4000));
  cval.Stop(base::TimeTicks::FromInternalValue(13000));
  cval.Start(base::TimeTicks::FromInternalValue(13000));
  cval.Stop(base::TimeTicks::FromInternalValue(14000));
  cval.Start(base::TimeTicks::FromInternalValue(14000));
  cval.Stop(base::TimeTicks::FromInternalValue(21000));
  cval.Flush();
  cval.Start(base::TimeTicks::FromInternalValue(21000));
  cval.Stop(base::TimeTicks::FromInternalValue(26000));

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
  EXPECT_EQ(*avg, "5.0ms");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "1000us");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "9.0ms");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "2.5ms");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "5.0ms");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "7.5ms");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "8.7ms");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "3.7ms");
  EXPECT_TRUE(entry_list);
  EXPECT_EQ(*entry_list, "[3000, 9000, 1000, 7000]");
}

TEST(CValCollectionTimerStatsTest, TwoManualFlushes) {
  const std::string name = "CollectionTimer";
  const size_t max_size = 5;
  bool enable_entry_list_c_val = true;

  base::CValCollectionTimerStats<CValDebug> cval(name, max_size,
                                                 enable_entry_list_c_val);
  cval.Start(base::TimeTicks::FromInternalValue(1000));
  cval.Stop(base::TimeTicks::FromInternalValue(4000));
  cval.Start(base::TimeTicks::FromInternalValue(4000));
  cval.Stop(base::TimeTicks::FromInternalValue(13000));
  cval.Start(base::TimeTicks::FromInternalValue(13000));
  cval.Stop(base::TimeTicks::FromInternalValue(14000));
  cval.Start(base::TimeTicks::FromInternalValue(14000));
  cval.Stop(base::TimeTicks::FromInternalValue(21000));
  cval.Flush();
  cval.Start(base::TimeTicks::FromInternalValue(21000));
  cval.Stop(base::TimeTicks::FromInternalValue(26000));
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
  EXPECT_EQ(*avg, "5.0ms");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "5.0ms");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "5.0ms");
  EXPECT_TRUE(pct25);
  EXPECT_EQ(*pct25, "5.0ms");
  EXPECT_TRUE(pct50);
  EXPECT_EQ(*pct50, "5.0ms");
  EXPECT_TRUE(pct75);
  EXPECT_EQ(*pct75, "5.0ms");
  EXPECT_TRUE(pct95);
  EXPECT_EQ(*pct95, "5.0ms");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0us");
  EXPECT_TRUE(entry_list);
  EXPECT_EQ(*entry_list, "[5000]");
}

}  // namespace base
