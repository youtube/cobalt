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
#include <cstdio>
#include <cstdlib>
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
//
// Stack-owned by RunPlayerBenchmarks for the duration of
// RunSpecifiedBenchmarks() (which borrows it without taking ownership) and used
// exclusively on the gtest runner thread.
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
  // which would hide intermittent failures during endurance runs. Note that
  // appending these after the loop overrides any value set by the user in the
  // command-line arguments above.
  args.push_back("--benchmark_report_aggregates_only=false");
  args.push_back("--benchmark_display_aggregates_only=false");
  return args;
}

// Google Benchmark calls its HelperPrintf callback followed by exit(0) when a
// recognized --benchmark_* flag has an invalid value (e.g.
// --benchmark_out_format=invalid or --benchmark_time_unit=bad). Terminate with
// _Exit(1) instead so malformed flags cannot pass silently and static
// starboard::Thread destructors do not run while SbRunStarboardMain's event
// loop is still on the stack; --help is already handled by InitGoogleTest()
// before GetBenchmarkArgs() filters for "--benchmark_*".
void PrintBenchmarkHelpAndFail() {
  ::benchmark::PrintDefaultHelp();
  std::fflush(stdout);
  std::fflush(stderr);
  std::_Exit(1);
}

// Initializes Google Benchmark once per process and shuts it down at process
// exit. ::benchmark::Shutdown() deletes internal::global_context without
// nulling the pointer, so calling Initialize()/Shutdown() per test iteration
// would use-after-free under --gtest_repeat > 1.
struct BenchmarkProcessState {
  bool has_unrecognized_args = false;

  BenchmarkProcessState() {
    std::vector<std::string> args = GetBenchmarkArgs();

    // Google Benchmark may modify argc/argv.
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);

    for (std::string& arg : args) {
      argv.push_back(arg.data());
    }

    argv.push_back(nullptr);
    int argc = static_cast<int>(argv.size()) - 1;

    ::benchmark::Initialize(&argc, argv.data(), &PrintBenchmarkHelpAndFail);
    has_unrecognized_args =
        ::benchmark::ReportUnrecognizedArguments(argc, argv.data());
  }

  ~BenchmarkProcessState() { ::benchmark::Shutdown(); }
};

}  // namespace

TEST(SbPlayerBenchmarkTest, RunPlayerBenchmarks) {
  static BenchmarkProcessState state;
  ASSERT_FALSE(state.has_unrecognized_args)
      << "Unrecognized --benchmark_* command-line flag";

  FailOnBenchmarkErrorReporter reporter;
  size_t benchmarks_run = ::benchmark::RunSpecifiedBenchmarks(&reporter);
  EXPECT_GT(benchmarks_run, 0u) << "No benchmark matched the filter";
}

}  // namespace starboard
