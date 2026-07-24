// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

/*
BENCHMARK RESULTS & ANALYSIS

=== LINUX ENABLED LOGS (MinLogLevel = INFO) ===
Scenario                               Wall Time (ns)   CPU Time (ns) Iterations
1) Simple ("Hello World")                  885,010           9,602        62,072
2) Multiple Parts (5 ints)               1,934,243          11,162        10,000
3) starboard::Size                       2,134,875           9,100        10,000
4) Custom Foo (10 vars)                  3,742,759          10,470         1,000

=== ANDROID ENABLED LOGS (MinLogLevel = INFO) ===
Scenario                               Wall Time (ns)   CPU Time (ns) Iterations
1) Simple ("Hello World")                   89,807          29,596        23,612
2) Multiple Parts (5 ints)                  93,640          32,056        21,820
3) starboard::Size                          90,566          30,771        22,954
4) Custom Foo (10 vars)                    101,457          37,238        19,055

=== LINUX DISABLED LOGS (MinLogLevel = WARNING) ===
Scenario                               Wall Time (ns)   CPU Time (ns) Iterations
1) Simple                                     1.53            1.53 455,485,452
2) Multiple Parts                             1.51            1.51 463,329,833
3) starboard::Size                            1.51            1.51 463,817,559
4) Custom Foo                                 1.76            1.76 397,285,599

=== ANDROID DISABLED LOGS (MinLogLevel = WARNING) ===
Scenario                               Wall Time (ns)   CPU Time (ns) Iterations
1) Simple                                     5.72            5.52 126,884,974
2) Multiple Parts                             5.70            5.52 126,546,370
3) starboard::Size                            5.73            5.52 126,808,585
4) Custom Foo                                 5.63            5.52 127,037,479

Key Observations:
1. Linux vs. Android (Enabled Logs):
   - Android is significantly faster than Linux (10x to 37x) when logs are
enabled.
   - Linux SbLog writes synchronously to stdout/stderr and calls fflush(),
causing heavy blocking I/O (Wall Time is ~100x larger than CPU Time).
   - Android SbLog calls __android_log_write which offloads logging
asynchronously to the logd daemon, paying only the IPC cost.

2. Cost of Formatting:
   - On Android, formatting a complex object like Foo (10 variables) adds
about 7.6 us of CPU overhead compared to a simple string, but the base IPC cost
(~30 us CPU) still dominates.
   - On Linux, Wall Time scales significantly with message size (0.88 ms -> 3.74
ms) due to larger I/O transfers, while CPU remains small.

3. Disabled Logs:
   - Disabled logs are extremely cheap (~1.5 ns on Linux, ~5.5 ns on Android)
and payload complexity has zero impact because stream arguments are not
evaluated (short-circuited via SB_LAZY_STREAM).
*/

#include "starboard/common/log.h"
#include "starboard/common/size.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

class Foo {
 public:
  friend std::ostream& operator<<(std::ostream& os, const Foo& foo) {
    return os << "{a=" << foo.a_ << ", b=" << foo.b_ << ", c=" << foo.c_
              << ", d=" << foo.d_ << ", e=" << foo.e_ << ", f=" << foo.f_
              << ", g=" << foo.g_ << ", h=" << foo.h_ << ", i=" << foo.i_
              << ", j=" << foo.j_ << "}";
  }

 private:
  int a_ = 1;
  std::string b_ = "hi";
  starboard::Size c_{12, 34};
  int d_ = 4;
  std::string e_ = "hello";
  starboard::Size f_{56, 78};
  int g_ = 7;
  std::string h_ = "world";
  starboard::Size i_{90, 12};
  int j_ = 10;
};

// --- Enabled Benchmarks ---

void BM_LogEnabled_Simple(::benchmark::State& state) {
  SetMinLogLevel(kSbLogPriorityInfo);
  for (auto _ : state) {
    SB_LOG(INFO) << "Hello World";
  }
}
BENCHMARK(BM_LogEnabled_Simple);

void BM_LogEnabled_MultipleParts(::benchmark::State& state) {
  SetMinLogLevel(kSbLogPriorityInfo);
  for (auto _ : state) {
    SB_LOG(INFO) << "Hello World: " << 1 << ", " << 2 << ", " << 3 << ", " << 4
                 << ", " << 5;
  }
}
BENCHMARK(BM_LogEnabled_MultipleParts);

void BM_LogEnabled_Size(::benchmark::State& state) {
  SetMinLogLevel(kSbLogPriorityInfo);
  starboard::Size size{12, 34};
  for (auto _ : state) {
    SB_LOG(INFO) << "Hello World: size=" << size;
  }
}
BENCHMARK(BM_LogEnabled_Size);

void BM_LogEnabled_Foo(::benchmark::State& state) {
  SetMinLogLevel(kSbLogPriorityInfo);
  Foo foo;
  for (auto _ : state) {
    SB_LOG(INFO) << "Hello World: foo=" << foo;
  }
}
BENCHMARK(BM_LogEnabled_Foo);

// --- Disabled Benchmarks ---

void BM_LogDisabled_Simple(::benchmark::State& state) {
  SetMinLogLevel(kSbLogPriorityWarning);
  for (auto _ : state) {
    SB_LOG(INFO) << "Hello World";
  }
}
BENCHMARK(BM_LogDisabled_Simple);

void BM_LogDisabled_MultipleParts(::benchmark::State& state) {
  SetMinLogLevel(kSbLogPriorityWarning);
  for (auto _ : state) {
    SB_LOG(INFO) << "Hello World: " << 1 << ", " << 2 << ", " << 3 << ", " << 4
                 << ", " << 5;
  }
}
BENCHMARK(BM_LogDisabled_MultipleParts);

void BM_LogDisabled_Size(::benchmark::State& state) {
  SetMinLogLevel(kSbLogPriorityWarning);
  starboard::Size size{12, 34};
  for (auto _ : state) {
    SB_LOG(INFO) << "Hello World: size=" << size;
  }
}
BENCHMARK(BM_LogDisabled_Size);

void BM_LogDisabled_Foo(::benchmark::State& state) {
  SetMinLogLevel(kSbLogPriorityWarning);
  Foo foo;
  for (auto _ : state) {
    SB_LOG(INFO) << "Hello World: foo=" << foo;
  }
}
BENCHMARK(BM_LogDisabled_Foo);

}  // namespace
}  // namespace starboard
