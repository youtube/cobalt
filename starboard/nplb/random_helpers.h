// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// Adapted from base/rand_util_unittest.cc

#ifndef STARBOARD_NPLB_RANDOM_HELPERS_H_
#define STARBOARD_NPLB_RANDOM_HELPERS_H_

#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

// Pointer to function that produces a random 64-bit number.
typedef uint64_t (*RandomFunction)(void);

// This tests to see that our underlying random generator is good enough, for
// some value of good enough. It does this by seeing if both 0 and 1 are seen at
// every position in a 64-bit random number over a number of trials. The lower
// the number, the better, I'd imagine, up to a certain point.
void TestProducesBothValuesOfAllBits(RandomFunction get_random);

// Verify that |get_random| has a uniform distribution.
void TestIsFairlyUniform(RandomFunction get_random);

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_RANDOM_HELPERS_H_
