/*
 * Copyright 2018 Google Inc. All Rights Reserved.
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

#include <functional>
#include <string>

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/thread.h"
#include "starboard/types.h"

namespace starboard {

class Semaphore;
class atomic_bool;

class Thread {
 public:
  struct Options {
    Options() : stack_size(0),  // Signal for default stack size.
                priority_(kSbThreadNoPriority),
                joinable(true) {}
    int64_t stack_size;
    SbThreadPriority priority_;
    bool joinable = true;
  };

  explicit Thread(const std::string& name);
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
  virtual void Start(const Options& options = Options());
  virtual void Join();
  bool join_called() const;

 protected:
  static void* ThreadEntryPoint(void* context);
  static void Sleep(SbTime microseconds);
  static void SleepMilliseconds(int value);

  // Waits at most |timeout| microseconds for Join() to be called. If
  // Join() was called then return |true|, else |false|.
  bool WaitForJoin(SbTime timeout);
  Semaphore* join_sema();
  atomic_bool* joined_bool();

  struct Data;
  scoped_ptr<Data> d_;

  SB_DISALLOW_COPY_AND_ASSIGN(Thread);
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_THREAD_H_
