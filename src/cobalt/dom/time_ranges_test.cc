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

#include "cobalt/dom/time_ranges.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {
class FakeExceptionState : public script::ExceptionState {
 public:
  void SetException(
      const scoped_refptr<script::ScriptException>& exception) OVERRIDE {
    dom_exception_ = make_scoped_refptr(
        base::polymorphic_downcast<DOMException*>(exception.get()));
  }
  void SetSimpleException(
      script::ExceptionState::SimpleExceptionType simple_exception_type,
      const std::string& message) OVERRIDE {
    simple_exception_ = simple_exception_type;
    simple_exception_message_ = message;
  }
  scoped_refptr<DOMException> dom_exception_;
  script::ExceptionState::SimpleExceptionType simple_exception_;
  std::string simple_exception_message_;
};
}  // namespace

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
    ASSERT_TRUE(exception_state.dom_exception_);
    EXPECT_EQ(DOMException::kIndexSizeErr,
              exception_state.dom_exception_->code());
  }
  {
    FakeExceptionState exception_state;
    time_ranges->End(2, &exception_state);
    ASSERT_TRUE(exception_state.dom_exception_);
    EXPECT_EQ(DOMException::kIndexSizeErr,
              exception_state.dom_exception_->code());
  }
}

}  // namespace dom
}  // namespace cobalt
