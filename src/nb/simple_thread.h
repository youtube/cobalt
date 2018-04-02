/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef NB_THREAD_H_
#define NB_THREAD_H_

#include <string>

#include "starboard/atomic.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/types.h"

namespace nb {

class SimpleThread {
 public:
  explicit SimpleThread(const std::string& name);
  virtual ~SimpleThread();

  // Subclasses should override the Run method.
  virtual void Run() = 0;

  // Signals to the thread to break out of it's loop.
  virtual void Cancel() {}

  // Called by the main thread, this will cause Run() to be invoked
  // on another thread.
  void Start();
  // Destroys the threads resources.
  void Join();

  bool join_called() const { return join_called_.load(); }

 protected:
  static void Sleep(SbTime microseconds);
  static void SleepMilliseconds(int value);

 private:
  static void* ThreadEntryPoint(void* context);

  const std::string name_;
  SbThread thread_;
  starboard::atomic_bool join_called_;

  SB_DISALLOW_COPY_AND_ASSIGN(SimpleThread);
};

}  // namespace nb

#endif  // NB_THREAD_H_
