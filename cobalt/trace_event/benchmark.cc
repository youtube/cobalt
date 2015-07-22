/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/trace_event/benchmark.h"
#include "cobalt/trace_event/scoped_event_parser_trace.h"

namespace cobalt {
namespace trace_event {

BenchmarkRegistrar* BenchmarkRegistrar::GetInstance() {
  // The BenchmarkRegistrar must be a leaky singleton because it is likely to
  // be constructed during static initialization while benchmarks are being
  // registered, and so, before an AtExitManager has been created.
  return Singleton<BenchmarkRegistrar,
                   LeakySingletonTraits<BenchmarkRegistrar> >::get();
}

BenchmarkRegistrar::BenchmarkRegistrar() {}

BenchmarkRegistrar::~BenchmarkRegistrar() {
  for (BenchmarkList::iterator iter = benchmarks_.begin();
       iter != benchmarks_.end(); ++iter) {
    delete *iter;
  }
}

void BenchmarkRegistrar::RegisterBenchmark(Benchmark* benchmark) {
  benchmarks_.push_back(benchmark);
}

BenchmarkResultsMap BenchmarkRegistrar::ExecuteBenchmarks() {
  BenchmarkResultsMap result;
  for (BenchmarkList::iterator iter = benchmarks_.begin();
       iter != benchmarks_.end(); ++iter) {
    result[(*iter)->name()] = ExecuteBenchmark(*iter);
  }
  return result;
}

std::vector<Benchmark::Result> BenchmarkRegistrar::ExecuteBenchmark(
    Benchmark* benchmark) {
  {
    ScopedEventParserTrace event_watcher(
        base::Bind(&Benchmark::AnalyzeTraceEvent, base::Unretained(benchmark)),
        FilePath("benchmark_" + benchmark->name() + ".json"));
    benchmark->Experiment();
  }

  return benchmark->CompileResults();
}

}  // namespace trace_event
}  // namespace cobalt
