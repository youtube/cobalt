// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <cstdlib>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Test to ensure random() returns a value within [0, RAND_MAX].
TEST(PosixInitStateTest, RandomReturnsValueWithinRange) {
  for (int i = 0; i < 100; ++i) {
    long random_value = random();
    EXPECT_GE(random_value, 0);
    EXPECT_LE(random_value, RAND_MAX);
  }
}

// Test that the same seed produces a repeatable sequence.
TEST(PosixInitStateTest, SameSeedProducesRepeatableSequence) {
  std::vector<char> state(256);
  initstate(123, state.data(), state.size());
  long seq1_val1 = random();
  long seq1_val2 = random();
  long seq1_val3 = random();

  initstate(123, state.data(), state.size());
  long seq2_val1 = random();
  long seq2_val2 = random();
  long seq2_val3 = random();

  EXPECT_EQ(seq1_val1, seq2_val1);
  EXPECT_EQ(seq1_val2, seq2_val2);
  EXPECT_EQ(seq1_val3, seq2_val3);
}

// Test that setstate can restore a previous state.
TEST(PosixInitStateTest, SetStateRestoresState) {
  std::vector<char> state1(256);
  std::vector<char> state2(256);

  // Generate a sequence of numbers and save the state.
  initstate(123, state1.data(), state1.size());
  long val1 = random();
  long val2 = random();

  // Switch to a different state and generate a different sequence.
  char* old_state = setstate(state2.data());
  initstate(456, state2.data(), state2.size());
  long val3 = random();
  long val4 = random();

  EXPECT_NE(val1, val3);
  EXPECT_NE(val2, val4);

  // Restore the original state and verify the sequence continues.
  setstate(old_state);
  long val5 = random();
  long val6 = random();

  // Re-generate the original sequence to compare.
  initstate(123, state1.data(), state1.size());
  random();
  random();
  long expected_val5 = random();
  long expected_val6 = random();

  EXPECT_EQ(val5, expected_val5);
  EXPECT_EQ(val6, expected_val6);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
