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

#include "cobalt/base/c_val_time_interval_entry_stats.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace base {

TEST(CValTimeIntervalEntryStatsTest, DefaultValues) {
  const std::string name = "TimeIntervalEntryStats";
  const size_t time_interval_in_ms = 50;

  base::CValTimeIntervalEntryStats<int> cval(name, time_interval_in_ms);

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "0");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "0");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "0");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "0");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0");
}

TEST(CValTimeIntervalEntryStatsTest, NoTimeout) {
  const std::string name = "TimeIntervalEntryStats";
  const size_t time_interval_in_ms = 50;

  base::CValTimeIntervalEntryStats<int> cval(name, time_interval_in_ms);
  cval.AddEntry(3, base::TimeTicks::FromInternalValue(1000));
  cval.AddEntry(9, base::TimeTicks::FromInternalValue(2000));
  cval.AddEntry(1, base::TimeTicks::FromInternalValue(3000));
  cval.AddEntry(7, base::TimeTicks::FromInternalValue(4000));

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "0");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "0");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "0");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "0");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0");
}

TEST(CValTimeIntervalEntryStatsTest, OneTimeout) {
  const std::string name = "TimeIntervalEntryStats";
  const size_t time_interval_in_ms = 50;

  base::CValTimeIntervalEntryStats<int, CValPublic> cval(name,
                                                         time_interval_in_ms);
  cval.AddEntry(3, base::TimeTicks::FromInternalValue(1000));
  cval.AddEntry(9, base::TimeTicks::FromInternalValue(2000));
  cval.AddEntry(1, base::TimeTicks::FromInternalValue(3000));
  cval.AddEntry(7, base::TimeTicks::FromInternalValue(4000));
  cval.AddEntry(5, base::TimeTicks::FromInternalValue(5000));
  cval.AddEntry(6, base::TimeTicks::FromInternalValue(52000));

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "5");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "5");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "1");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "9");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "2");
}

TEST(CValTimeIntervalEntryStatsTest, TwoTimeouts) {
  const std::string name = "TimeIntervalEntryStats";
  const size_t time_interval_in_ms = 50;

  base::CValTimeIntervalEntryStats<int, CValDebug> cval(name,
                                                        time_interval_in_ms);
  cval.AddEntry(3, base::TimeTicks::FromInternalValue(1000));
  cval.AddEntry(9, base::TimeTicks::FromInternalValue(2000));
  cval.AddEntry(1, base::TimeTicks::FromInternalValue(3000));
  cval.AddEntry(7, base::TimeTicks::FromInternalValue(4000));
  cval.AddEntry(5, base::TimeTicks::FromInternalValue(52000));
  cval.AddEntry(6, base::TimeTicks::FromInternalValue(114000));

  base::CValManager* cvm = base::CValManager::GetInstance();
  base::Optional<std::string> count =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Cnt", name.c_str()));
  base::Optional<std::string> avg =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Avg", name.c_str()));
  base::Optional<std::string> min =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Min", name.c_str()));
  base::Optional<std::string> max =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Max", name.c_str()));
  base::Optional<std::string> std =
      cvm->GetValueAsPrettyString(base::StringPrintf("%s.Std", name.c_str()));

  EXPECT_TRUE(count);
  EXPECT_EQ(*count, "1");
  EXPECT_TRUE(avg);
  EXPECT_EQ(*avg, "5");
  EXPECT_TRUE(min);
  EXPECT_EQ(*min, "5");
  EXPECT_TRUE(max);
  EXPECT_EQ(*max, "5");
  EXPECT_TRUE(std);
  EXPECT_EQ(*std, "0");
}

}  // namespace base
