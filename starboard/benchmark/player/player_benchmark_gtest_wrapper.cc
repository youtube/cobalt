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

#include "starboard/common/command_line.h"
#include "starboard/testing/test_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace benchmark {

TEST(SbPlayerBenchmarkTest, RunPlayerBenchmarks) {
  // Run only the player benchmarks.
  // Google Benchmark modifies argc/argv.
  int argc = 2;
  std::string filter = "BM_Player";
  const starboard::CommandLine* command_line =
      starboard::testing::GetTestCommandLine();
  if (command_line && command_line->HasSwitch("benchmark_filter")) {
    filter = command_line->GetSwitchValue("benchmark_filter");
  }
  std::string filter_arg = "--benchmark_filter=" + filter;
  char* argv[] = {(char*)"player_benchmarks",
                  const_cast<char*>(filter_arg.c_str())};

  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
}

}  // namespace benchmark
}  // namespace starboard
