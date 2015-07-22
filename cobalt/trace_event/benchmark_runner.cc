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
#include <cstdio>
#include <map>

#include "base/at_exit.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/trace_event/benchmark.h"

namespace {

using cobalt::trace_event::Benchmark;
using cobalt::trace_event::BenchmarkResultsMap;

void Output(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  std::vfprintf(stdout, fmt, ap);

  va_end(ap);

  std::fflush(stdout);
}

void YamlPrint(const BenchmarkResultsMap& benchmarks) {
  Output("---\n");
  for (BenchmarkResultsMap::const_iterator benchmark = benchmarks.begin();
       benchmark != benchmarks.end(); ++benchmark) {
    const std::string& name = benchmark->first;
    const std::vector<Benchmark::Result>& results = benchmark->second;
    Output("\"%s\":\n", name.c_str());
    for (std::vector<Benchmark::Result>::const_iterator result =
             results.begin();
         result != results.end(); ++result) {
      Output("  \"%s\":\n", result->name.c_str());
      if (result->samples.empty()) {
        Output("    {}  # No samples found.\n");
      } else {
        int count = result->samples.size();
        double average = 0;
        double minimum = result->samples[0];
        double maximum = result->samples[0];
        for (std::vector<double>::const_iterator sample =
                 result->samples.begin();
             sample != result->samples.end(); ++sample) {
          average += *sample;
          minimum = std::min(minimum, *sample);
          maximum = std::max(maximum, *sample);
        }
        average /= result->samples.size();

        Output("    \"Number of samples\": %f\n", count);
        Output("    Average: %f\n", average);
        Output("    Minimum: %f\n", minimum);
        Output("    Maximum: %f\n", maximum);
      }
    }
  }
  Output("...\n");
}
}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  cobalt::InitCobalt(argc, argv);
  BenchmarkResultsMap benchmarks =
      cobalt::trace_event::BenchmarkRegistrar::GetInstance()
          ->ExecuteBenchmarks();
  YamlPrint(benchmarks);
  return 0;
}
