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
#ifndef COBALT_BASE_DEBUGGER_HOOKS_H_
#define COBALT_BASE_DEBUGGER_HOOKS_H_

#include <string>

#include "base/logging.h"

namespace base {

// Interface to allow the WebModule and the various objects implementing the
// DOM, etc. to report their behaviour to the web debugger, without needing to
// directly access the DebugModule.
class DebuggerHooks {
 public:
  DebuggerHooks() = default;
  DebuggerHooks(const DebuggerHooks&) = delete;
  DebuggerHooks* operator=(const DebuggerHooks&) = delete;
  DebuggerHooks(DebuggerHooks&&) = delete;
  DebuggerHooks* operator=(DebuggerHooks&&) = delete;

  // Indicates whether an asynchronous task will run at most once or if it might
  // run multiple times.
  enum class AsyncTaskFrequency {
    kOneshot,
    kRecurring,
  };

  // Logs a message to the JavaScript console.
  virtual void ConsoleLog(::logging::LogSeverity severity,
                          std::string message) const = 0;

  // Record the JavaScript stack on the WebModule thread at the point a task is
  // initiated that will run at a later time (on the same thread), allowing it
  // to be seen as the originator when breaking in the asynchronous task.
  //
  // |task| is a pointer to any arbitrary object (e.g. callback, timer, etc.)
  // uniquely associated with the execution that will take place at a later
  // time.
  //
  // |name| is a user-visible label shown in the debugger to identify what the
  // asynchronous stack trace is.
  //
  // |frequency| whether the task runs at most once or might run multiple times.
  // If kOneshot then the task will be implicitly canceled after it is finished,
  // and if kRecurring then it must be explicitly canceled.
  virtual void AsyncTaskScheduled(const void* task, const std::string& name,
                                  AsyncTaskFrequency frequency) const = 0;

  // Inform the debugger that a scheduled task is starting to run.
  virtual void AsyncTaskStarted(const void* task) const = 0;

  // Inform the debugger that a scheduled task has finished running.
  virtual void AsyncTaskFinished(const void* task) const = 0;

  // Inform the debugger that a scheduled task will no longer be run, and that
  // it may free any resources associated with it.
  virtual void AsyncTaskCanceled(const void* task) const = 0;
};

// Helper to start & finish async tasks using RAII.
class ScopedAsyncTask {
 public:
  ScopedAsyncTask(const DebuggerHooks& debugger_hooks, const void* task)
      : debugger_hooks_(debugger_hooks), task_(task) {
    debugger_hooks_.AsyncTaskStarted(task_);
  }
  ~ScopedAsyncTask() { debugger_hooks_.AsyncTaskFinished(task_); }

 private:
  const DebuggerHooks& debugger_hooks_;
  const void* const task_;
};

// Null implementation for gold builds and tests where there is no debugger.
class NullDebuggerHooks : public DebuggerHooks {
 public:
  void ConsoleLog(::logging::LogSeverity severity,
                  std::string message) const override {}
  void AsyncTaskScheduled(const void* task, const std::string& name,
                          AsyncTaskFrequency frequency) const override {}
  void AsyncTaskStarted(const void* task) const override {}
  void AsyncTaskFinished(const void* task) const override {}
  void AsyncTaskCanceled(const void* task) const override {}
};

}  // namespace base

#endif  // COBALT_BASE_DEBUGGER_HOOKS_H_
