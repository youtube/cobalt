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
#include <limits>
#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Test to ensure rand_r() returns a value within [0, RAND_MAX].
TEST(PosixRandrTest, ReturnsValueWithinRange) {
  unsigned int seed = 123;  // Initial seed
  for (int i = 0; i < 100; ++i) {
    int random_value = rand_r(&seed);
    EXPECT_GE(random_value, 0);
    EXPECT_LE(random_value, RAND_MAX);
  }
}

// Test that RAND_MAX is at least 32767 as per POSIX.
TEST(PosixRandrTest, RandMaxIsAtLeast32767) {
  EXPECT_GE(RAND_MAX, 32767);  // POSIX minimum
}

// Test that different initial seeds produce different sequences.
TEST(PosixRandrTest, DifferentInitialSeedsProduceDifferentSequences) {
  unsigned int seed1 = 1;
  unsigned int seed2 = 2;

  int rand_val1_seq1 = rand_r(&seed1);
  int rand_val2_seq1 = rand_r(&seed1);

  int rand_val1_seq2 = rand_r(&seed2);
  int rand_val2_seq2 = rand_r(&seed2);

  EXPECT_NE(rand_val1_seq1, rand_val1_seq2);
  EXPECT_NE(rand_val2_seq1, rand_val2_seq2);
}

// Test that the same initial seed produces a repeatable sequence.
TEST(PosixRandrTest, SameInitialSeedProducesRepeatableSequence) {
  unsigned int seed_orig = 123;
  unsigned int seed1 = seed_orig;
  unsigned int seed2 = seed_orig;

  int seq1_val1 = rand_r(&seed1);
  int seq1_val2 = rand_r(&seed1);
  int seq1_val3 = rand_r(&seed1);

  int seq2_val1 = rand_r(&seed2);
  int seq2_val2 = rand_r(&seed2);
  int seq2_val3 = rand_r(&seed2);

  EXPECT_EQ(seq1_val1, seq2_val1);
  EXPECT_EQ(seq1_val2, seq2_val2);
  EXPECT_EQ(seq1_val3, seq2_val3);
}

// Test that multiple calls without re-seeding produce different values.
TEST(PosixRandrTest, MultipleCallsProduceDifferentValues) {
  unsigned int seed =
      static_cast<unsigned int>(time(NULL));  // Seed with current time
  int val1 = rand_r(&seed);
  int val2 = rand_r(&seed);
  int val3 = rand_r(&seed);

  EXPECT_NE(val1, val2);
  EXPECT_NE(val2, val3);
  EXPECT_NE(val1, val3);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
