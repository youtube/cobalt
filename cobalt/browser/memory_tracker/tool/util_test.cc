// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/tool/util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "nb/bit_cast.h"
#include "starboard/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {
namespace {

const char* ToAddress(uintptr_t val) {
  return nb::bit_cast<const char*>(val);
}

// 4k page size.
static const uintptr_t kPageSize = 4096;

class FakeTimeSource {
 public:
  explicit FakeTimeSource(base::TimeTicks value) : static_time_(value) {}
  void set_static_time(base::TimeTicks value) {
    static_time_ = value;
  }
  base::TimeTicks GetTime() {
    return static_time_;
  }
 private:
  base::TimeTicks static_time_;
};

}  // namespace.

// Tests the expectation that AllocationSizeBinner will correctly bin
// allocations.
TEST(UtilTest, RemoveString) {
  std::string value = "abba";
  value = RemoveString(value, "bb");
  EXPECT_STREQ("aa", value.c_str());
}

// Tests the expectation that AllocationSizeBinner will correctly bin
// allocations.
TEST(UtilTest, InsertCommasIntoNumberString) {
  std::string value = "2345.54";
  std::string value_with_commas = InsertCommasIntoNumberString(value);

  EXPECT_STREQ("2,345.54", value_with_commas.c_str());
}

TEST(UtilTest, NumberFormatWithCommas) {
  int value = 1000;
  std::string value_with_commas = NumberFormatWithCommas<int>(value);

  EXPECT_STREQ("1,000", value_with_commas.c_str());
}

// Tests the expectation that RemoveOddElements() removes the odd elements of
// a vector and resizes it.
TEST(UtilTest, RemoveOddElements) {
  std::vector<int> values;

  // EVEN TEST.
  values.push_back(0);
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);

  RemoveOddElements(&values);

  ASSERT_EQ(2, values.size());
  EXPECT_EQ(0, values[0]);
  EXPECT_EQ(2, values[1]);

  values.clear();

  // ODD TEST
  values.push_back(0);
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  values.push_back(4);
  RemoveOddElements(&values);

  ASSERT_EQ(3, values.size());
  EXPECT_EQ(0, values[0]);
  EXPECT_EQ(2, values[1]);
  EXPECT_EQ(4, values[2]);
}

// Tests the expectation that GetLinearFit() generates the expected linear
// regression values.
TEST(UtilTest, GetLinearFit) {
  std::vector<std::pair<int, int> > data;
  for (int i = 0; i < 10; ++i) {
    data.push_back(std::pair<int, int>(i+1, 2*i));
  }
  double slope = 0;
  double y_intercept = 0;
  GetLinearFit(data.begin(), data.end(), &slope, &y_intercept);

  EXPECT_DOUBLE_EQ(2.0f, slope);
  EXPECT_DOUBLE_EQ(-2.0f, y_intercept);
}

// Test the expectation that BaseNameFast() works correctly for both windows
// and linux path types.
TEST(UtilTest, BaseNameFast) {
  const char* linux_path = "directory/filename.cc";
  const char* win_path = "directory\\filename.cc";

  EXPECT_STREQ("filename.cc", BaseNameFast(linux_path));
  EXPECT_STREQ("filename.cc", BaseNameFast(win_path));
}

TEST(UtilTest, TimerUse) {
  base::TimeTicks initial_time = base::TimeTicks::Now();
  FakeTimeSource time_source(initial_time);

  Timer::TimeFunctor time_functor =
      base::Bind(&FakeTimeSource::GetTime, base::Unretained(&time_source));

  Timer test_timer(base::TimeDelta::FromSeconds(30), time_functor);
  EXPECT_FALSE(test_timer.UpdateAndIsExpired());  // 0 time has elapsed.

  time_source.set_static_time(initial_time + base::TimeDelta::FromSeconds(29));
  // 29 seconds has elapsed, which is less than the 30 seconds required for
  // the timer to fire.
  EXPECT_FALSE(test_timer.UpdateAndIsExpired());
  time_source.set_static_time(initial_time + base::TimeDelta::FromSeconds(30));
  // 31 seconds has elapsed, which means that the next call to
  // UpdateAndIsExpired() should succeed.
  EXPECT_TRUE(test_timer.UpdateAndIsExpired());
  // Now that the value fired, expect that it won't fire again (until the next
  // 30 seconds has passed).
  EXPECT_FALSE(test_timer.UpdateAndIsExpired());
  time_source.set_static_time(initial_time + base::TimeDelta::FromSeconds(60));
  EXPECT_TRUE(test_timer.UpdateAndIsExpired());
  EXPECT_FALSE(test_timer.UpdateAndIsExpired());
}

TEST(Segment, SplitAcrossPageBoundaries_Simple) {
  std::string name("AllocName");
  const char* start_addr = ToAddress(0);
  const char* end_addr = ToAddress(1);

  Segment memory_segment(&name, start_addr, end_addr);

  std::vector<Segment> segments;
  memory_segment.SplitAcrossPageBoundaries(kPageSize, &segments);
  ASSERT_EQ(1, segments.size());

  ASSERT_EQ(memory_segment, segments[0]);
}

TEST(Segment, SplitAcrossPageBoundaries_WholePage) {
  std::string name("AllocName");
  const char* start_addr = ToAddress(0);
  const char* end_addr = ToAddress(kPageSize);

  Segment memory_segment(&name, start_addr, end_addr);

  std::vector<Segment> segments;
  memory_segment.SplitAcrossPageBoundaries(kPageSize, &segments);
  ASSERT_EQ(1, segments.size());

  ASSERT_EQ(memory_segment, segments[0]);
}

TEST(Segment, SplitAcrossPageBoundaries_Across) {
  std::string name("AllocName");
  const char* start_addr = ToAddress(kPageSize / 2);
  const char* mid_addr = ToAddress(kPageSize);
  const char* end_addr = ToAddress(kPageSize + kPageSize / 2);

  Segment memory_segment(&name, start_addr, end_addr);

  std::vector<Segment> segments;
  memory_segment.SplitAcrossPageBoundaries(kPageSize, &segments);

  ASSERT_EQ(2, segments.size());

  Segment s1(&name, start_addr, mid_addr);
  Segment s2(&name, mid_addr, end_addr);

  ASSERT_EQ(s1, segments[0]);
  ASSERT_EQ(s2, segments[1]);
}

TEST(Segment, SplitAcrossPageBoundaries_Many) {
  std::string name("AllocName");
  const char* start_addr = ToAddress(kPageSize / 2);
  const char* mid_0_addr = ToAddress(kPageSize);
  const char* mid_1_addr = ToAddress(kPageSize * 2);
  const char* end_addr = ToAddress(2 * kPageSize + kPageSize / 2);

  Segment memory_segment(&name, start_addr, end_addr);

  std::vector<Segment> segments;
  memory_segment.SplitAcrossPageBoundaries(4096, &segments);
  ASSERT_EQ(3, segments.size());

  Segment s1(&name, start_addr, mid_0_addr);
  Segment s2(&name, mid_0_addr, mid_1_addr);
  Segment s3(&name, mid_1_addr, end_addr);

  ASSERT_EQ(s1, segments[0]);
  ASSERT_EQ(s2, segments[1]);
  ASSERT_EQ(s3, segments[2]);
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
