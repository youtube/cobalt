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

// Benchmarks sequential locking of two absl::Mutexes.
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

// Benchmarks sequential locking of two base::Locks.
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

// Benchmarks sequential locking of two starboard::Mutexes.
// This is the "original implementation" style (risky for deadlocks).
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

// Benchmarks sequential locking of two std::mutexes.
// This is equivalent to BM_StarboardSequentialLock but using std:: primitives.
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

// Benchmarks locking of two std::mutexes using std::scoped_lock.
// This uses the deadlock avoidance algorithm (std::lock).
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
