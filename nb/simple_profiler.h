/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef NB_SIMPLE_PROFILER_H_
#define NB_SIMPLE_PROFILER_H_

#include <string>

#include "starboard/time.h"
#include "starboard/types.h"

namespace nb {

// SimpleProfiler is useful for development. It will allow the developer
// to track where the CPU spends most of it's time in a call chain.
// CPU time is tracked via thread-local-storage. When the last SimpleProfiler
// in the thread is destroyed then a printout of the timing will be printed
// to the output log.
//
// By default, SimpleProfiler will generate output whenever it's used. To
// selectively profile a thread see class SimpleProfiler::EnableScope.
//
// Example:
//  void Foo() {
//    SimpleProfiler profile(__FUNCTION__);
//    Bar();
//    Baz();
//  }
//  void Bar() {
//    SimpleProfiler profile(__FUNCTION__);
//    // ... do something expensive ...
//    Qux();
//  }
//  void Baz() {
//    SimpleProfiler profile(__FUNCTION__);
//    // ... do something cheap ...
//  }
//  void Qux() {
//    SimpleProfiler profile(__FUNCTION__);
//    // ... do something nearly free ...
//  }
//
//  Outputs: "Foo: 25us\n"
//           " Bar: 20us\n"
//           "  Qux: 1us\n"
//           " Baz: 4us\n"
class SimpleProfiler {
 public:
  explicit SimpleProfiler(const char* name);
  ~SimpleProfiler();
  // EnableScope can be used to enable and disable SimpleProfiler in the
  // thread. A scoped object is used so that SimpleProfiler
  // constructor / destructor order is maintained in relation to
  // enabling / disabling.
  // Example:
  //  // Assume SimpleProfiler was globally disabled by default.
  //  void Foo() {
  //     SimpleProfiler::ThreadScope enable_scope(true);  // enabled in scope.
  //     SimpleProfiler profile(__FUNCTION__);
  //     DoWork();
  //  }
  class EnableScope {
   public:
    explicit EnableScope(bool enabled);
    ~EnableScope();
   private:
    bool prev_enabled_;
  };
  // If SetThreadLocalEnableFlag() isn't explicitly set by the thread
  // then this |input| value will control whether the SimpleProfiler
  // is active or not. For best results, set this value as early as
  // possible during program execution.
  static void SetEnabledByDefault(bool value);
  // Is SimpleProfiler enabled? If Get/SetThreadLocalEnableFlag() isn't
  // set then this will return an enabled by default flag, which defaults
  // to true.
  static bool IsEnabled();
  typedef void (*MessageHandlerFunction)(const char* msg);
  // Useful for tests. Setting to nullptr will reset the behavior to
  // default functionality.
  static void SetLoggingFunction(MessageHandlerFunction fcn);
  // Useful for tests. Setting to nullptr will reset the behavior to
  // default functionality.
  typedef SbTimeMonotonic (*ClockFunction)();
  static void SetClockFunction(ClockFunction fcn);
 private:
  int momento_index_;
};

}  // namespace nb

#endif  // NB_SIMPLE_PROFILER_H_
