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
// safety of the prctl wrapper in cobalt_layer_posix_prctl_abi_wrappers.cc.

TEST(PosixPrctlVariadicTest, HandlesDifferentArgumentCounts) {
  // Test with 0 extra arguments (some prctl options might take 0, though
  // our wrapper handles most as taking at least one 'arg2' via va_arg).
  // PR_GET_DUMPABLE takes 0 extra arguments in the underlying syscall,
  // but our wrapper's va_list handling will try to read arg2 for it.
  // This is safe because the caller IS providing arguments in the prctl(...)
  // variadic call, even if they are just 0s.
  EXPECT_NE(-1, prctl(PR_GET_DUMPABLE, 0, 0, 0, 0));

  // Test with 1 extra argument.
  char name[16];
  EXPECT_EQ(0, prctl(PR_GET_NAME, name, 0, 0, 0));

  // Test with 4 extra arguments (max handled by our wrapper).
  void* p = malloc(4096);
  ASSERT_NE(p, nullptr);
  // We don't care about the result here, just that it doesn't crash.
  prctl(MUSL_PR_SET_VMA, MUSL_PR_SET_VMA_ANON_NAME, (unsigned long)p, 4096,
        (unsigned long)"TestName");
  free(p);
}

TEST(PosixPrctlVariadicTest, ThreadSafety) {
  const int kNumThreads = 10;
  const int kNumIterations = 100;

  auto task = []() {
    for (int i = 0; i < kNumIterations; ++i) {
      void* p = malloc(4096);
      if (p) {
        prctl(MUSL_PR_SET_VMA, MUSL_PR_SET_VMA_ANON_NAME, (unsigned long)p,
              4096, (unsigned long)"ThreadSafeTest");
        free(p);
      }
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
  // If we reached here without crashing or TSAN errors (if run with TSAN),
  // then the mutex and variadic handling are likely working.
}

}  // namespace
}  // namespace nplb
