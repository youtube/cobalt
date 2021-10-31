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

#include "cobalt/websocket/buffered_amount_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace websocket {

class BufferedAmountTrackerTest : public ::testing::Test {
 public:
  BufferedAmountTracker tracker_;
};

TEST_F(BufferedAmountTrackerTest, Construct) {
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 0ul);
}

TEST_F(BufferedAmountTrackerTest, AddNonPayload) {
  tracker_.Add(false, 5);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 0ul);
}

TEST_F(BufferedAmountTrackerTest, AddPayload) {
  tracker_.Add(true, 5);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 5ul);
}

TEST_F(BufferedAmountTrackerTest, AddCombination) {
  tracker_.Add(true, 5);
  tracker_.Add(false, 3);
  tracker_.Add(true, 11);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 16ul);
}

TEST_F(BufferedAmountTrackerTest, AddCombination2) {
  tracker_.Add(false, 2);
  tracker_.Add(false, 3);
  tracker_.Add(true, 11);
  tracker_.Add(true, 1);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 12ul);
}

TEST_F(BufferedAmountTrackerTest, AddZero) {
  tracker_.Add(true, 0);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 0ul);

  tracker_.Add(false, 0);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 0ul);
}

TEST_F(BufferedAmountTrackerTest, PopZero) {
  std::size_t amount_poped = 0;
  tracker_.Add(false, 5);
  amount_poped = tracker_.Pop(0);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 0ul);
  EXPECT_EQ(amount_poped, 0);

  tracker_.Add(false, 0);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 0ul);
  EXPECT_EQ(amount_poped, 0);
}

TEST_F(BufferedAmountTrackerTest, PopPayload) {
  std::size_t amount_poped = 0;
  tracker_.Add(true, 5);
  amount_poped = tracker_.Pop(4);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 1ul);
  EXPECT_EQ(amount_poped, 4);
}

TEST_F(BufferedAmountTrackerTest, PopNonPayload) {
  std::size_t amount_poped = 0;
  tracker_.Add(false, 5);
  amount_poped = tracker_.Pop(3);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 0ul);
  EXPECT_EQ(amount_poped, 0);
}

TEST_F(BufferedAmountTrackerTest, PopCombined) {
  std::size_t amount_poped = 0;
  tracker_.Add(false, 5);
  tracker_.Add(true, 3);
  amount_poped = tracker_.Pop(7);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 1ul);
  EXPECT_EQ(amount_poped, 2);
}

TEST_F(BufferedAmountTrackerTest, PopOverflow) {
  std::size_t amount_poped = 0;
  tracker_.Add(false, 2);
  tracker_.Add(true, 2);
  amount_poped = tracker_.Pop(16);
  EXPECT_EQ(amount_poped, 2);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 0ul);
}

TEST_F(BufferedAmountTrackerTest, MultipleOperations) {
  tracker_.Add(false, 2);
  tracker_.Add(true, 3);
  tracker_.Add(false, 1);
  tracker_.Add(false, 6);
  tracker_.Add(true, 14);
  tracker_.Add(false, 3);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 17ul);

  std::size_t amount_poped = 0;
  tracker_.Add(false, 3);
  amount_poped = tracker_.Pop(4);
  EXPECT_EQ(amount_poped, 2);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 15ul);
  tracker_.Add(false, 4);
  amount_poped = tracker_.Pop(2);
  EXPECT_EQ(amount_poped, 1);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 14ul);
  tracker_.Add(true, 5);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 19ul);
  amount_poped = tracker_.Pop(4);
  EXPECT_EQ(amount_poped, 0);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 19ul);
  tracker_.Add(false, 6);
  amount_poped = tracker_.Pop(4);
  EXPECT_EQ(amount_poped, 2);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 17ul);
  tracker_.Add(true, 7);
  amount_poped = tracker_.Pop(4);
  EXPECT_EQ(amount_poped, 4);
  EXPECT_EQ(tracker_.GetTotalBytesInflight(), 20ul);
}

}  // namespace websocket
}  // namespace cobalt
