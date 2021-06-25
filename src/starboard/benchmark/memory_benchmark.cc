// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/memory.h"

#include "third_party/google_benchmark/include/benchmark/benchmark.h"

#include <cstring>

namespace starboard {
namespace benchmark {
namespace {

void BM_MemoryCopy(::benchmark::State& state) {
  void* memory1 = SbMemoryAllocate(state.range(0));
  void* memory2 = SbMemoryAllocate(state.range(0));

  for (auto _ : state) {
    memcpy(memory1, memory2, state.range(0));
    ::benchmark::ClobberMemory();
  }
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(state.range(0)));

  SbMemoryDeallocate(memory1);
  SbMemoryDeallocate(memory2);
}

void BM_MemoryMove(::benchmark::State& state) {
  void* memory1 = SbMemoryAllocate(state.range(0));
  void* memory2 = SbMemoryAllocate(state.range(0));

  for (auto _ : state) {
    memmove(memory1, memory2, state.range(0));
    ::benchmark::ClobberMemory();
  }
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(state.range(0)));

  SbMemoryDeallocate(memory1);
  SbMemoryDeallocate(memory2);
}

BENCHMARK(BM_MemoryCopy)->RangeMultiplier(4)->Range(16, 1024 * 1024);
BENCHMARK(BM_MemoryCopy)
    ->Arg(1024 * 1024)
    ->DenseThreadRange(1, 4)
    ->UseRealTime();
BENCHMARK(BM_MemoryMove)->Arg(1024 * 1024);

}  // namespace
}  // namespace benchmark
}  // namespace starboard
