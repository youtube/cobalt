/*
 * Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_THREAD_H_
#define STARBOARD_COMMON_THREAD_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "starboard/configuration.h"
#include "starboard/thread.h"

namespace starboard {

class Semaphore;

class Thread {
 public:
  explicit Thread(const std::string& name);
  Thread(const std::string& name, int64_t stack_size);
  template <size_t N>
  explicit Thread(char const (&name)[N]) : Thread(std::string(name)) {
    // Common to all user code, limited by Linux pthreads default
    static_assert(N <= 16, "Thread name too long, max 16");
  }
  template <size_t N>
  Thread(char const (&name)[N], int64_t stack_size)
      : Thread(std::string(name), stack_size) {
    // Common to all user code, limited by Linux pthreads default
    static_assert(N <= 16, "Thread name too long, max 16");
  }
  virtual ~Thread();

  // Subclasses should override the Run method.
  // Example:
  //  void Run() {
  //    while (!WaitForJoin(kWaitTime)) {
  //      ... do work ...
  //    }
  //  }
  virtual void Run() = 0;

  // Called by the main thread, this will cause Run() to be invoked
  // on another thread.
  virtual void Start();
  virtual void Join();
  bool join_called() const;

 protected:
  static void* ThreadEntryPoint(void* context);
  static void Sleep(int64_t microseconds);
  static void SleepMilliseconds(int value);

  // Waits at most |timeout| microseconds for Join() to be called. If
  // Join() was called then return |true|, else |false|.
  bool WaitForJoin(int64_t timeout);
  Semaphore* join_sema();
  std::atomic_bool* joined_bool();

  struct Data;
  std::unique_ptr<Data> d_;

  Thread(const Thread&) = delete;
  void operator=(const Thread&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_THREAD_H_
