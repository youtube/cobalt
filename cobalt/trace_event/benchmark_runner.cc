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

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/trace_event/benchmark.h"

namespace {

using cobalt::trace_event::Benchmark;
using cobalt::trace_event::BenchmarkResultsMap;

PRINTF_FORMAT(1, 2) void Output(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  std::vfprintf(stdout, fmt, ap);

  va_end(ap);

  std::fflush(stdout);
}

void JsonPrint(const BenchmarkResultsMap& benchmarks) {
  Output("---Benchmark Results Start---\n");
  scoped_ptr<base::DictionaryValue> compilation(new base::DictionaryValue);
  for (BenchmarkResultsMap::const_iterator benchmark = benchmarks.begin();
       benchmark != benchmarks.end(); ++benchmark) {
    const std::string& name = benchmark->first;
    const std::vector<Benchmark::Result>& results = benchmark->second;
    scoped_ptr<base::DictionaryValue> benchmark_value(
        new base::DictionaryValue);
    for (std::vector<Benchmark::Result>::const_iterator result =
             results.begin();
         result != results.end(); ++result) {
      scoped_ptr<base::DictionaryValue> result_value(new base::DictionaryValue);
      if (!result->samples.empty()) {
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
        result_value->SetIntegerWithoutPathExpansion("Samples", count);
        result_value->SetDoubleWithoutPathExpansion("Average", average);
        result_value->SetDoubleWithoutPathExpansion("Minimum", minimum);
        result_value->SetDoubleWithoutPathExpansion("Maximum", maximum);
      }
      benchmark_value->SetWithoutPathExpansion(result->name,
                                               result_value.release());
    }
    compilation->SetWithoutPathExpansion(name, benchmark_value.release());
  }
  std::string print_string;
  base::JSONWriter::WriteWithOptions(
      compilation.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &print_string);
  Output("%s", print_string.c_str());

  Output("---Benchmark Results End---\n");
}

int RunnerMain(int argc, char** argv) {
#if defined(NDEBUG)
  // Get rid of all log output so we only see benchmark results.
  logging::SetMinLogLevel(logging::LOG_FATAL);
#endif
  BenchmarkResultsMap benchmarks =
      cobalt::trace_event::BenchmarkRegistrar::GetInstance()
          ->ExecuteBenchmarks();
  JsonPrint(benchmarks);
  return 0;
}

}  // namespace

COBALT_WRAP_SIMPLE_MAIN(RunnerMain);
