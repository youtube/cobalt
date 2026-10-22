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
  // The output is consumed by test runners rather than a terminal, so never
  // emit color escape sequences, which would also corrupt the gtest result
  // lines printed right after the benchmark table.
  FailOnBenchmarkErrorReporter() : ConsoleReporter(OO_Tabular) {}

  void ReportRuns(const std::vector<Run>& reports) override {
    for (const Run& run : reports) {
      if (run.skipped == ::benchmark::internal::SkippedWithError) {
        ADD_FAILURE() << run.benchmark_name() << ": " << run.skip_message;
      }
    }
    ConsoleReporter::ReportRuns(reports);
  }
};

// Returns the --benchmark_* flags of the test command line, so that e.g.
// --benchmark_filter can be passed through the test runner. gtest keeps a
// copy of the original command line, and InitGoogleTest() leaves the flags it
// does not recognize in it. Other flags are dropped as Google Benchmark
// rejects flags it does not know. Runs only the player benchmarks unless a
// filter is given explicitly.
std::vector<std::string> GetBenchmarkArgs() {
  std::vector<std::string> args = {"player_benchmarks"};
  bool has_filter = false;
  for (const std::string& arg : ::testing::internal::GetArgvs()) {
    if (arg.rfind("--benchmark_", 0) == 0) {
      args.push_back(arg);
      has_filter |= arg.rfind("--benchmark_filter", 0) == 0;
    }
  }
  if (!has_filter) {
    args.push_back("--benchmark_filter=BM_Player");
  }
  return args;
}

}  // namespace

TEST(SbPlayerBenchmarkTest, RunPlayerBenchmarks) {
  std::vector<std::string> args = GetBenchmarkArgs();
  // Google Benchmark may modify argc/argv.
  std::vector<char*> argv;
  for (std::string& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  int argc = static_cast<int>(argv.size()) - 1;

  ::benchmark::Initialize(&argc, argv.data());
  FailOnBenchmarkErrorReporter reporter;
  size_t benchmarks_run = ::benchmark::RunSpecifiedBenchmarks(&reporter);
  EXPECT_GT(benchmarks_run, 0u) << "No benchmark matched the filter";
}

}  // namespace benchmark
}  // namespace starboard
