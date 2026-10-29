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

#include <cstddef>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

// Console reporter that additionally fails the enclosing gtest for every
// benchmark that reported an error through State::SkipWithError(), which
// RunSpecifiedBenchmarks() would otherwise only print. Note that passing a
// custom display reporter fixes stdout to tabular console output; JSON/CSV
// output can still be written via --benchmark_out=<path>
// --benchmark_out_format=json.
class FailOnBenchmarkErrorReporter : public ::benchmark::ConsoleReporter {
 public:
  // The output is consumed by test runners rather than a terminal, so never
  // emit color escape sequences, which would also corrupt the gtest result
  // lines printed right after the benchmark table.
  FailOnBenchmarkErrorReporter() : ConsoleReporter(OO_Tabular) {}

  void ReportRuns(const std::vector<Run>& reports) override {
    size_t error_count = 0;
    std::string benchmark_name;
    std::string first_error_message;
    for (const Run& run : reports) {
      if (run.skipped == ::benchmark::internal::SkippedWithError) {
        if (error_count == 0) {
          benchmark_name = run.benchmark_name();
          first_error_message = run.skip_message;
        }
        ++error_count;
      }
    }
    if (error_count == 1) {
      ADD_FAILURE() << benchmark_name << ": " << first_error_message;
    } else if (error_count > 1) {
      ADD_FAILURE() << benchmark_name << " (" << error_count
                    << " repetitions failed): " << first_error_message;
    }
    ConsoleReporter::ReportRuns(reports);
  }
};

// Returns the --benchmark_* flags from the test command line so that e.g.
// --benchmark_filter can be passed through the test runner.
// ::testing::internal::GetArgvs() is an internal GoogleTest API intentionally
// relied upon here because starboard_test_main owns main(); it holds a copy of
// the full original argv captured before InitGoogleTest() strips --gtest_*
// flags, so filtering by the "--benchmark_" prefix forwards only Google
// Benchmark flags.
std::vector<std::string> GetBenchmarkArgs() {
  std::vector<std::string> args = {"player_benchmarks"};
  for (const std::string& arg : ::testing::internal::GetArgvs()) {
    if (arg.rfind("--benchmark_", 0) == 0) {
      args.push_back(arg);
    }
  }
  // Ensure individual repetition runs are always delivered to
  // FailOnBenchmarkErrorReporter::ReportRuns(). In aggregates-only mode,
  // Google Benchmark omits SkippedWithError runs when >= 2 repetitions succeed,
  // which would hide intermittent failures during endurance runs.
  args.push_back("--benchmark_report_aggregates_only=false");
  args.push_back("--benchmark_display_aggregates_only=false");
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
  if (::benchmark::ReportUnrecognizedArguments(argc, argv.data())) {
    ::benchmark::Shutdown();
    FAIL() << "Unrecognized --benchmark_* command-line flag";
  }

  FailOnBenchmarkErrorReporter reporter;
  size_t benchmarks_run = ::benchmark::RunSpecifiedBenchmarks(&reporter);
  ::benchmark::Shutdown();
  EXPECT_GT(benchmarks_run, 0u) << "No benchmark matched the filter";
}

}  // namespace starboard
