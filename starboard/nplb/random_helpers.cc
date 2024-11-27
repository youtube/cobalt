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

#include "starboard/nplb/random_helpers.h"

#include <limits>

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

void TestProducesBothValuesOfAllBits(RandomFunction get_random) {
  const uint64_t kAllZeros = SB_UINT64_C(0);
  const uint64_t kAllOnes = ~kAllZeros;
  const int kTrialCount = 1000;
  uint64_t found_ones = kAllZeros;
  uint64_t found_zeros = kAllOnes;

  for (size_t i = 0; i < kTrialCount; ++i) {
    uint64_t value = get_random();
    found_ones |= value;
    found_zeros &= value;

    if (found_zeros == kAllZeros && found_ones == kAllOnes)
      return;
  }

  ADD_FAILURE() << "Didn't achieve all bit values in maximum number of tries.";
}

void TestIsFairlyUniform(RandomFunction get_random) {
  const uint64_t kTopOfRange = std::numeric_limits<uint64_t>::max();
  const uint64_t kExpectedAverage = kTopOfRange / SB_UINT64_C(2);
  const uint64_t kAllowedVariance = kExpectedAverage / SB_UINT64_C(50);
  const int kMinAttempts = 1000;
  const int kMaxAttempts = 1000000;

  double cumulative_average = 0.0;
  int count = 0;
  while (count < kMaxAttempts) {
    uint64_t value = get_random();
    cumulative_average = (count * cumulative_average + value) / (count + 1);

    // Don't quit too quickly for things to start converging, or we may have
    // a false positive.
    if (count > kMinAttempts &&
        kExpectedAverage - kAllowedVariance < cumulative_average &&
        cumulative_average < kExpectedAverage + kAllowedVariance) {
      break;
    }

    ++count;
  }

  EXPECT_LT(count, kMaxAttempts) << "Expected average was " << kExpectedAverage
                                 << ", average ended at " << cumulative_average;
}

}  // namespace nplb
}  // namespace starboard
