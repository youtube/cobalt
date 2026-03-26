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

#include <mutex>

#include "absl/synchronization/mutex.h"
#include "base/synchronization/lock.h"
#include "starboard/common/mutex.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard::benchmark {

namespace {

// --- Single Lock Benchmarks ---

void BM_SingleAbslMutexLock(::benchmark::State& state) {
  absl::Mutex m;
  int i = 0;
  for (auto _ : state) {
    absl::MutexLock lock(&m);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_SingleAbslMutexLock);

void BM_SingleBaseLock(::benchmark::State& state) {
  base::Lock m;
  int i = 0;
  for (auto _ : state) {
    base::AutoLock lock(m);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_SingleBaseLock);

void BM_SingleStarboardMutexLock(::benchmark::State& state) {
  Mutex m;
  int i = 0;
  for (auto _ : state) {
    ScopedLock lock(m);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_SingleStarboardMutexLock);

void BM_SingleStdMutexLock(::benchmark::State& state) {
  std::mutex m;
  int i = 0;
  for (auto _ : state) {
    std::lock_guard lock(m);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_SingleStdMutexLock);

// --- Double Lock Benchmarks ---

void BM_AbslMutexSequentialLock(::benchmark::State& state) {
  absl::Mutex m1, m2;
  int i = 0;
  for (auto _ : state) {
    absl::MutexLock lock1(&m1);
    absl::MutexLock lock2(&m2);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_AbslMutexSequentialLock);

void BM_BaseLockSequentialLock(::benchmark::State& state) {
  base::Lock m1, m2;
  int i = 0;
  for (auto _ : state) {
    base::AutoLock lock1(m1);
    base::AutoLock lock2(m2);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_BaseLockSequentialLock);

void BM_StarboardSequentialLock(::benchmark::State& state) {
  Mutex m1, m2;
  int i = 0;
  for (auto _ : state) {
    ScopedLock lock1(m1);
    ScopedLock lock2(m2);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_StarboardSequentialLock);

// Sequential locking of two std::mutexes.
void BM_StdSequentialLock(::benchmark::State& state) {
  std::mutex m1, m2;
  int i = 0;
  for (auto _ : state) {
    std::lock_guard lock1(m1);
    std::lock_guard lock2(m2);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_StdSequentialLock);

// Locking of two std::mutexes using std::scoped_lock (Deadlock Avoidance).
void BM_StdScopedLock(::benchmark::State& state) {
  std::mutex m1, m2;
  int i = 0;
  for (auto _ : state) {
    std::scoped_lock lock(m1, m2);
    i++;
    ::benchmark::DoNotOptimize(i);
  }
}
BENCHMARK(BM_StdScopedLock);

}  // namespace

}  // namespace starboard::benchmark
