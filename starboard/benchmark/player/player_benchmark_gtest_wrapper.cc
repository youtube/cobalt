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

#include <string>
#include <vector>

#include "starboard/common/command_line.h"
#include "starboard/testing/test_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace benchmark {
namespace {

// Console reporter that additionally fails the enclosing gtest for every
// benchmark that reported an error through State::SkipWithError(), which
// RunSpecifiedBenchmarks() would otherwise only print.
class FailOnBenchmarkErrorReporter : public ::benchmark::ConsoleReporter {
 public:
  void ReportRuns(const std::vector<Run>& reports) override {
    for (const Run& run : reports) {
      if (run.skipped == ::benchmark::internal::SkippedWithError) {
        ADD_FAILURE() << run.benchmark_name() << ": " << run.skip_message;
      }
    }
    ConsoleReporter::ReportRuns(reports);
  }
};

}  // namespace

TEST(SbPlayerBenchmarkTest, RunPlayerBenchmarks) {
  // Run only the player benchmarks unless a filter is passed explicitly.
  std::string filter = "BM_Player";
  const starboard::CommandLine* command_line =
      starboard::testing::GetTestCommandLine();
  if (command_line && command_line->HasSwitch("benchmark_filter")) {
    filter = command_line->GetSwitchValue("benchmark_filter");
  }

  // Google Benchmark may modify argc/argv.
  std::string program_name = "player_benchmarks";
  std::string filter_arg = "--benchmark_filter=" + filter;
  std::vector<char*> argv = {program_name.data(), filter_arg.data(), nullptr};
  int argc = static_cast<int>(argv.size()) - 1;

  ::benchmark::Initialize(&argc, argv.data());
  FailOnBenchmarkErrorReporter reporter;
  size_t benchmarks_run = ::benchmark::RunSpecifiedBenchmarks(&reporter);
  EXPECT_GT(benchmarks_run, 0u)
      << "No benchmark matched the filter '" << filter << "'";
}

}  // namespace benchmark
}  // namespace starboard
