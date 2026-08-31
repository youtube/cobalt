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

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

#if BUILDFLAG(IS_ANDROID)
#include "perfetto/tracing.h"
#endif

namespace starboard {
namespace benchmark {

// Dummy functions to force linking of benchmark objects
void LinkPlayerCreateBenchmark();
void LinkPlayerDestroyBenchmark();

TEST(SbPlayerBenchmarkTest, RunPlayerBenchmarks) {
#if BUILDFLAG(IS_ANDROID)
  perfetto::TracingInitArgs perfetto_args;
  perfetto_args.backends = perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(perfetto_args);
#endif

  // Force linking of benchmarks
  LinkPlayerCreateBenchmark();
  LinkPlayerDestroyBenchmark();

  // Run only the player benchmarks.
  // Google Benchmark modifies argc/argv.
  int argc = 2;
  char* argv[] = {(char*)"player_benchmarks",
                  (char*)"--benchmark_filter=BM_Player"};

  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
}

}  // namespace benchmark
}  // namespace starboard
