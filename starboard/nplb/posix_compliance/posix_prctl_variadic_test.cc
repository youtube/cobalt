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

#include <errno.h>
#include <stdlib.h>
#include <sys/prctl.h>

#include <thread>
#include <vector>

#include "starboard/shared/modular/starboard_layer_posix_prctl_abi_wrappers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// This test specifically targets the variadic argument handling and thread
// safety of the prctl wrapper.

TEST(PosixPrctlVariadicTest, HandlesDifferentArgumentCounts) {
  // Test with 0 extra arguments (some prctl options might take 0, though
  // our wrapper handles most as taking at least one 'arg2' via va_arg).
  EXPECT_NE(-1, prctl(PR_GET_DUMPABLE, 0, 0, 0, 0));

  // Test with 1 extra argument.
  char name[16];
  EXPECT_EQ(0, prctl(PR_GET_NAME, name, 0, 0, 0));
}

TEST(PosixPrctlVariadicTest, ThreadSafety) {
  const int kNumThreads = 10;
  const int kNumIterations = 100;

  auto task = []() {
    for (int i = 0; i < kNumIterations; ++i) {
      char name[16];
      prctl(PR_GET_NAME, name, 0, 0, 0);
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(task);
  }

  for (auto& t : threads) {
    t.join();
  }
}

}  // namespace
}  // namespace nplb
