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

void BenchmarkRegistrar::ExecuteBenchmarks() {
  for (BenchmarkList::iterator iter = benchmarks_.begin();
       iter != benchmarks_.end(); ++iter) {
    ExecuteBenchmark(*iter);
  }
}

void BenchmarkRegistrar::ExecuteBenchmark(Benchmark* benchmark) {
  DLOG(INFO) << "Executing benchmark: " << benchmark->name();

  {
    ScopedEventParserTrace event_watcher(
        base::Bind(&Benchmark::AnalyzeTraceEvent, base::Unretained(benchmark)),
        FilePath("benchmark_" + benchmark->name() + ".json"));
    benchmark->Experiment();
  }

  std::vector<Benchmark::Result> results = benchmark->CompileResults();

  // Output the results.
  for (std::vector<Benchmark::Result>::iterator iter = results.begin();
       iter != results.end(); ++iter) {
    if (iter->samples.empty()) {
      continue;
    }

    DLOG(INFO) << "  " << iter->name;
    double samples_average = 0;
    double samples_min = std::numeric_limits<double>::max();
    double samples_max = std::numeric_limits<double>::min();
    for (std::vector<double>::iterator sample = iter->samples.begin();
         sample != iter->samples.end(); ++sample) {
      samples_average += *sample;
      samples_min = std::min(samples_min, *sample);
      samples_max = std::max(samples_max, *sample);
    }
    samples_average /= iter->samples.size();
    DLOG(INFO) << "    "
               << "Number of samples: " << iter->samples.size();
    if (!iter->samples.empty()) {
      DLOG(INFO) << "    "
                 << "Average: " << samples_average;
      DLOG(INFO) << "    "
                 << "Minimum: " << samples_min;
      DLOG(INFO) << "    "
                 << "Maximum: " << samples_max;
    }
  }
}

}  // namespace trace_event
}  // namespace cobalt
