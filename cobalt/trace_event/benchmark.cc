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

#include "base/logging.h"
#include "base/stringprintf.h"
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

BenchmarkRegistrar::~BenchmarkRegistrar() {}

void BenchmarkRegistrar::RegisterBenchmarkCreator(
    BenchmarkCreator* benchmark_registerer) {
  benchmark_registerers_.push_back(benchmark_registerer);
}

BenchmarkResultsMap BenchmarkRegistrar::ExecuteBenchmarks() {
  BenchmarkResultsMap result;

  for (RegistererList::iterator iter = benchmark_registerers_.begin();
       iter != benchmark_registerers_.end(); ++iter) {
    std::vector<BenchmarkCreator::CreateBenchmarkFunction> benchmark_creators =
        (*iter)->GetBenchmarkCreators();

    for (std::vector<BenchmarkCreator::CreateBenchmarkFunction>::iterator iter =
             benchmark_creators.begin();
         iter != benchmark_creators.end(); ++iter) {
      scoped_ptr<Benchmark> benchmark = iter->Run();
      result[benchmark->name()] = ExecuteBenchmark(benchmark.get());
    }
  }

  return result;
}

std::vector<Benchmark::Result> BenchmarkRegistrar::ExecuteBenchmark(
    Benchmark* benchmark) {
  for (int i = 0; i < benchmark->num_iterations(); ++i) {
    {
      std::string filename_iteration_postfix =
          benchmark->num_iterations() > 1 ? base::StringPrintf("_%d", i)
                                          : std::string("");

      ScopedEventParserTrace event_watcher(
          base::Bind(&Benchmark::AnalyzeTraceEvent,
                     base::Unretained(benchmark)),
          FilePath("benchmark_" + benchmark->name() +
                   filename_iteration_postfix + ".json"));
      benchmark->Experiment();
    }

    if (!benchmark->on_iteration_complete().is_null()) {
      benchmark->on_iteration_complete().Run();
    }
  }

  return benchmark->CompileResults();
}

}  // namespace trace_event
}  // namespace cobalt
