// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_PERFORMANCE_HELPERS_H_
#define STARBOARD_NPLB_PERFORMANCE_HELPERS_H_

#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

// Default for parameter |count_calls| of TestPerformanceOfFunction.
constexpr int kDefaultTestPerformanceCountCalls = 1000;
// Default for parameter |max_time_per_call| of TestPerformanceOfFunction.
constexpr SbTimeMonotonic kDefaultTestPerformanceMaxTimePerCall =
    kSbTimeMillisecond / 2;

// Helper function for testing function call time performance.
template <const size_t count_calls,
          const SbTimeMonotonic max_time_per_call,
          typename R,
          typename... Args>
void TestPerformanceOfFunction(const char* const name_of_f,
                               R (*const f)(Args...),
                               Args... args) {
  // Measure time pre calls to |f|.
  const SbTimeMonotonic time_start = SbTimeGetMonotonicNow();

  // Call |f| |count_calls| times.
  for (int i = 0; i < count_calls; ++i) {
    f(args...);
  }

  // Measure time post calls to |f|.
  const SbTimeMonotonic time_last = SbTimeGetMonotonicNow();
  const SbTimeMonotonic time_delta = time_last - time_start;
  const double time_per_call = static_cast<double>(time_delta) / count_calls;

  // Pretty printing.
  SB_LOG(INFO) << "TestPerformanceOfFunction - " << name_of_f;
  SB_LOG(INFO) << "  Measured duration " << time_delta << "us total across "
               << count_calls << " calls.";
  SB_LOG(INFO) << "  Measured duration " << time_per_call
               << "us average per call.";

  // Compare |time_delta| to |max_time_delta|.
  // Using the aggregate time avoids loss of precision at the us range.
  const SbTimeMonotonic max_time_delta = max_time_per_call * count_calls;
  EXPECT_LT(time_delta, max_time_delta);
}

// The following macros should be preferred over direct calls to
// TestPerformanceOfFunction to assure consistency between |name_of_f| and |f|.
// Note: TEST_PERF_FUNCWITHARGS_xxx and TEST_PERF_FUNCNOARGS_xxx variants exist
// to accommodate proper variadic parameter passing from macros to templates.

// TEST_PERF_FUNCNOARGS_EXPLICIT: No args to |f|, explicit test args.
#define TEST_PERF_FUNCNOARGS_EXPLICIT(C, T, name) \
  TestPerformanceOfFunction<C, T>(#name, &name)

// TEST_PERF_FUNCNOARGS_EXPLICIT: No args to |f|, default test args.
#define TEST_PERF_FUNCNOARGS_DEFAULT(name)                         \
  TEST_PERF_FUNCNOARGS_EXPLICIT(kDefaultTestPerformanceCountCalls, \
                                kDefaultTestPerformanceMaxTimePerCall, name)

// TEST_PERF_FUNCWITHARGS_EXPLICIT: Variadic args to |f|, explicit test args.
#define TEST_PERF_FUNCWITHARGS_EXPLICIT(C, T, name, ...) \
  TestPerformanceOfFunction<C, T>(#name, &name, __VA_ARGS__)

// TEST_PERF_FUNCWITHARGS_DEFAULT: Variadic args to |f|, default test args.
#define TEST_PERF_FUNCWITHARGS_DEFAULT(name, ...)                              \
  TEST_PERF_FUNCWITHARGS_EXPLICIT(kDefaultTestPerformanceCountCalls,           \
                                  kDefaultTestPerformanceMaxTimePerCall, name, \
                                  __VA_ARGS__)

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_PERFORMANCE_HELPERS_H_
