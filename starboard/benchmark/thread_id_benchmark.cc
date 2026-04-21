// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/thread.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

// 1. Raw system call / Starboard API
void BM_ThreadGetId(::benchmark::State& state) {
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(SbThreadGetId());
  }
}
BENCHMARK(BM_ThreadGetId);

// 2. Simple TLS cache
SbThreadId GetThreadId() {
  // NOTE: We can cache the thread ID in thread-local storage since Cobalt
  // doesn't use fork().
  thread_local SbThreadId tls_thread_id = kSbThreadInvalidId;
  if (tls_thread_id == kSbThreadInvalidId) {
    tls_thread_id = SbThreadGetId();
  }
  return tls_thread_id;
}

void BM_ThreadGetIdCache(::benchmark::State& state) {
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(GetThreadId());
  }
}
BENCHMARK(BM_ThreadGetIdCache);

}  // namespace
}  // namespace starboard
