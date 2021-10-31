// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/time_ranges.h"

#include <limits>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/testing/fake_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

void CheckEqual(const scoped_refptr<TimeRanges>& time_ranges1,
                const scoped_refptr<TimeRanges>& time_ranges2) {
  ASSERT_EQ(time_ranges1->length(), time_ranges2->length());
  for (uint32 i = 0; i < time_ranges1->length(); ++i) {
    EXPECT_EQ(time_ranges1->Start(i, NULL), time_ranges2->Start(i, NULL));
    EXPECT_EQ(time_ranges1->End(i, NULL), time_ranges2->End(i, NULL));
  }
}

}  // namespace

using testing::FakeExceptionState;

TEST(TimeRangesTest, Constructors) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
  FakeExceptionState exception_state;

  EXPECT_EQ(0, time_ranges->length());

  time_ranges = new TimeRanges(0.0, 1.0);
  EXPECT_EQ(1, time_ranges->length());
  EXPECT_EQ(0.0, time_ranges->Start(0, &exception_state));
  EXPECT_EQ(1.0, time_ranges->End(0, &exception_state));
}

TEST(TimeRangesTest, IncrementalNonOverlappedAdd) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
  FakeExceptionState exception_state;

  time_ranges->Add(0.0, 1.0);
  time_ranges->Add(2.0, 3.0);

  EXPECT_EQ(2, time_ranges->length());
  EXPECT_EQ(0.0, time_ranges->Start(0, &exception_state));
  EXPECT_EQ(1.0, time_ranges->End(0, &exception_state));
  EXPECT_EQ(2.0, time_ranges->Start(1, &exception_state));
  EXPECT_EQ(3.0, time_ranges->End(1, &exception_state));
}

TEST(TimeRangesTest, Contains) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;

  time_ranges->Add(0.0, 1.0);
  time_ranges->Add(2.0, 3.0);
  time_ranges->Add(4.0, 5.0);
  time_ranges->Add(6.0, 7.0);
  time_ranges->Add(8.0, 9.0);

  EXPECT_TRUE(time_ranges->Contains(0.0));
  EXPECT_TRUE(time_ranges->Contains(0.5));
  EXPECT_TRUE(time_ranges->Contains(1.0));
  EXPECT_TRUE(time_ranges->Contains(3.0));
  EXPECT_TRUE(time_ranges->Contains(5.0));
  EXPECT_TRUE(time_ranges->Contains(7.0));
  EXPECT_TRUE(time_ranges->Contains(9.0));

  EXPECT_FALSE(time_ranges->Contains(-0.5));
  EXPECT_FALSE(time_ranges->Contains(1.5));
  EXPECT_FALSE(time_ranges->Contains(3.5));
  EXPECT_FALSE(time_ranges->Contains(5.5));
  EXPECT_FALSE(time_ranges->Contains(7.5));
  EXPECT_FALSE(time_ranges->Contains(9.5));
}

TEST(TimeRangesTest, MergeableAdd) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges(10.0, 11.0);
  FakeExceptionState exception_state;

  // Connected at the start.
  time_ranges->Add(9.0, 10.0);

  EXPECT_EQ(1, time_ranges->length());
  EXPECT_EQ(9.0, time_ranges->Start(0, &exception_state));
  EXPECT_EQ(11.0, time_ranges->End(0, &exception_state));

  // Connected at the end.
  time_ranges->Add(11.0, 12.0);

  EXPECT_EQ(1, time_ranges->length());
  EXPECT_EQ(9.0, time_ranges->Start(0, &exception_state));
  EXPECT_EQ(12.0, time_ranges->End(0, &exception_state));

  // Overlapped at the start.
  time_ranges->Add(8.0, 10.0);

  EXPECT_EQ(1, time_ranges->length());
  EXPECT_EQ(8.0, time_ranges->Start(0, &exception_state));
  EXPECT_EQ(12.0, time_ranges->End(0, &exception_state));

  // Overlapped at the end.
  time_ranges->Add(11.0, 13.0);

  EXPECT_EQ(1, time_ranges->length());
  EXPECT_EQ(8.0, time_ranges->Start(0, &exception_state));
  EXPECT_EQ(13.0, time_ranges->End(0, &exception_state));

  // Cover the whole range.
  time_ranges->Add(7.0, 14.0);

  EXPECT_EQ(1, time_ranges->length());
  EXPECT_EQ(7.0, time_ranges->Start(0, &exception_state));
  EXPECT_EQ(14.0, time_ranges->End(0, &exception_state));
}

TEST(TimeRangesTest, UnorderedAdd) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
  FakeExceptionState exception_state;

  time_ranges->Add(50.0, 60.0);
  time_ranges->Add(10.0, 20.0);
  time_ranges->Add(90.0, 100.0);
  time_ranges->Add(70.0, 80.0);
  time_ranges->Add(30.0, 40.0);

  EXPECT_EQ(5, time_ranges->length());
  EXPECT_EQ(10.0, time_ranges->Start(0, &exception_state));
  EXPECT_EQ(30.0, time_ranges->Start(1, &exception_state));
  EXPECT_EQ(50.0, time_ranges->Start(2, &exception_state));
  EXPECT_EQ(80.0, time_ranges->End(3, &exception_state));
  EXPECT_EQ(100.0, time_ranges->End(4, &exception_state));
}

TEST(TimeRangesTest, Nearest) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
  FakeExceptionState exception_state;

  time_ranges->Add(0.0, 1.0);
  time_ranges->Add(2.0, 3.0);
  time_ranges->Add(4.0, 5.0);
  time_ranges->Add(6.0, 7.0);
  time_ranges->Add(8.0, 9.0);

  EXPECT_EQ(0.0, time_ranges->Nearest(-0.5));
  EXPECT_EQ(9.0, time_ranges->Nearest(9.5));

  EXPECT_EQ(0.0, time_ranges->Nearest(0.0));
  EXPECT_EQ(0.5, time_ranges->Nearest(0.5));
  EXPECT_EQ(2.5, time_ranges->Nearest(2.5));
  EXPECT_EQ(4.5, time_ranges->Nearest(4.5));
  EXPECT_EQ(6.5, time_ranges->Nearest(6.5));
  EXPECT_EQ(8.5, time_ranges->Nearest(8.5));

  EXPECT_EQ(3.0, time_ranges->Nearest(3.1));
  EXPECT_EQ(8.0, time_ranges->Nearest(7.8));
}

TEST(TimeRangesTest, IndexOutOfRangeException) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;

  time_ranges->Add(0.0, 1.0);
  time_ranges->Add(2.0, 3.0);

  EXPECT_EQ(2, time_ranges->length());
  {
    FakeExceptionState exception_state;
    time_ranges->Start(2, &exception_state);
    EXPECT_EQ(DOMException::kIndexSizeErr, exception_state.GetExceptionCode());
  }
  {
    FakeExceptionState exception_state;
    time_ranges->End(2, &exception_state);
    EXPECT_EQ(DOMException::kIndexSizeErr, exception_state.GetExceptionCode());
  }
}

TEST(TimeRangesTest, Clone) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
  scoped_refptr<TimeRanges> cloned = time_ranges->Clone();
  EXPECT_NE(time_ranges, cloned);
  scoped_refptr<TimeRanges> another_cloned = time_ranges->Clone();
  EXPECT_NE(cloned, another_cloned);

  time_ranges->Add(0.0, 1.0);
  time_ranges->Add(2.0, 3.0);

  cloned = time_ranges->Clone();
  EXPECT_NE(time_ranges, cloned);
  EXPECT_EQ(time_ranges->length(), cloned->length());

  for (uint32 i = 0; i < cloned->length(); ++i) {
    EXPECT_EQ(time_ranges->Start(i, NULL), cloned->Start(i, NULL));
    EXPECT_EQ(time_ranges->End(i, NULL), cloned->End(i, NULL));
  }
}

TEST(TimeRangesTest, UnionWithSelf) {
  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
  scoped_refptr<TimeRanges> unioned = time_ranges->UnionWith(time_ranges);
  EXPECT_NE(time_ranges, unioned);
  CheckEqual(time_ranges, unioned);
}

TEST(TimeRangesTest, UnionWith) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  scoped_refptr<TimeRanges> time_ranges1 = new TimeRanges;
  time_ranges1->Add(-kInfinity, 1.0);
  time_ranges1->Add(4.0, 6.0);
  time_ranges1->Add(8.0, 10.0);

  // 1:******************   **  **
  // ===================0123456789A==================
  // 2:                   *  **   *******************
  // U:****************** * *** *********************
  scoped_refptr<TimeRanges> time_ranges2 = new TimeRanges;
  time_ranges2->Add(2.0, 3.0);
  time_ranges2->Add(5.0, 7.0);
  time_ranges2->Add(10.0, kInfinity);

  scoped_refptr<TimeRanges> cloned_time_ranges1 = time_ranges1->Clone();
  scoped_refptr<TimeRanges> cloned_time_ranges2 = time_ranges2->Clone();
  scoped_refptr<TimeRanges> unioned = time_ranges1->UnionWith(time_ranges2);

  CheckEqual(cloned_time_ranges1, time_ranges1);
  CheckEqual(cloned_time_ranges2, time_ranges2);

  EXPECT_EQ(4, unioned->length());

  EXPECT_EQ(-kInfinity, unioned->Start(0, NULL));
  EXPECT_EQ(1, unioned->End(0, NULL));

  EXPECT_EQ(2, unioned->Start(1, NULL));
  EXPECT_EQ(3, unioned->End(1, NULL));

  EXPECT_EQ(4, unioned->Start(2, NULL));
  EXPECT_EQ(7, unioned->End(2, NULL));

  EXPECT_EQ(8, unioned->Start(3, NULL));
  EXPECT_EQ(kInfinity, unioned->End(3, NULL));
}

TEST(TimeRangesTest, IntersectWithEmptyRange) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  scoped_refptr<TimeRanges> time_ranges1 = new TimeRanges;
  scoped_refptr<TimeRanges> intersection =
      time_ranges1->IntersectWith(time_ranges1);
  EXPECT_NE(time_ranges1, intersection);
  CheckEqual(time_ranges1, intersection);

  scoped_refptr<TimeRanges> time_ranges2 = new TimeRanges;
  time_ranges2->Add(-kInfinity, kInfinity);

  intersection = time_ranges1->IntersectWith(time_ranges2);
  CheckEqual(time_ranges1, intersection);
}

TEST(TimeRangesTest, IntersectWith) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  scoped_refptr<TimeRanges> time_ranges1 = new TimeRanges;
  time_ranges1->Add(-kInfinity, 1.0);
  time_ranges1->Add(4.0, 6.0);
  time_ranges1->Add(8.0, kInfinity);

  // 1:******************   **  *********************
  // ===================0123456789A==================
  // 2:                   *  **   *******************
  // I:                      *    *******************
  scoped_refptr<TimeRanges> time_ranges2 = new TimeRanges;
  time_ranges2->Add(2.0, 3.0);
  time_ranges2->Add(5.0, 7.0);
  time_ranges2->Add(10.0, kInfinity);

  scoped_refptr<TimeRanges> cloned_time_ranges1 = time_ranges1->Clone();
  scoped_refptr<TimeRanges> cloned_time_ranges2 = time_ranges2->Clone();
  scoped_refptr<TimeRanges> intersection =
      time_ranges1->IntersectWith(time_ranges2);

  CheckEqual(cloned_time_ranges1, time_ranges1);
  CheckEqual(cloned_time_ranges2, time_ranges2);

  EXPECT_EQ(2, intersection->length());

  EXPECT_EQ(5, intersection->Start(0, NULL));
  EXPECT_EQ(6, intersection->End(0, NULL));

  EXPECT_EQ(10, intersection->Start(1, NULL));
  EXPECT_EQ(kInfinity, intersection->End(1, NULL));
}

}  // namespace dom
}  // namespace cobalt
